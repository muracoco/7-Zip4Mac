# SPDX-License-Identifier: LGPL-3.0-or-later
"""Source privacy policy shared by local bundle export and public snapshots."""
import os
from pathlib import Path
import re

SECRET = re.compile(
    rb'(?<![A-Za-z0-9_])(?:gh[pousr]_[A-Za-z0-9]{36,255}|'
    rb'github_pat_[A-Za-z0-9_]{40,255}|(?:AKIA|ASIA)[A-Z0-9]{16}|'
    rb'AIza[0-9A-Za-z_-]{35}|sk-[A-Za-z0-9_-]{32,}|'
    rb'xox[baprs]-[A-Za-z0-9-]{20,})(?![A-Za-z0-9_])|'
    rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----')
USER_PATH = re.compile(rb'/Users/(?!Shared(?:/|$)|\$|<)[^/\s\x22\x27<>()[\]`]+')
VOLUME_PATH = re.compile(rb'/Volumes/(?!\$|<)[^/\s\x22\x27<>()[\]`]+')
TEMP_PATH = re.compile(rb'(?:/private)?/var/folders/[A-Za-z0-9_-]+/[A-Za-z0-9_-]+/')


def normalized(root, name, data):
    # Diagnostics stay intact locally. Only the documentation copy is redacted;
    # upstream resources, licenses, imported bodies and patches retain bytes.
    if not (name.startswith('docs/') or name in {'README.md', 'README.ja.md'}):
        return data
    deps = Path(os.environ.get('SEVENZIP_DEPS_DIR', Path.home() / '.cache/7zip-mac-port'))
    data = data.replace(str(root).encode(), b'/REPOSITORY')
    data = data.replace(str(deps).encode(), b'/DEPS')
    data = USER_PATH.sub(b'/HOME', data)
    data = VOLUME_PATH.sub(b'/VOLUME', data)
    return TEMP_PATH.sub(b'/TMP/', data)


def checked(root, name, data):
    if SECRET.search(data):
        raise ValueError("Credential pattern in source: " + name)
    data = normalized(root, name, data)
    if USER_PATH.search(data) or VOLUME_PATH.search(data) or TEMP_PATH.search(data):
        raise ValueError("Unreviewed machine path in source: " + name)
    return data
