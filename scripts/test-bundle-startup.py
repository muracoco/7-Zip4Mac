#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Check the owned bundle process with an explicit private INI settings profile."""
import argparse
import os
import re
from pathlib import Path
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('app', type=Path)
args = parser.parse_args()
executable = args.app / 'Contents/MacOS/7-Zip Mac'
preferences = Path.home() / 'Library/Preferences'

def native_settings():
    return {p.name: p.read_bytes() for p in preferences.iterdir()
            if p.is_file() and ('sevenzip' in p.name.lower() or '7-zip' in p.name.lower())}

def run(environment, arguments, observe=False):
    process = subprocess.Popen([str(executable), *arguments], env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    alive = False
    if observe:
        time.sleep(3)
        alive = process.poll() is None
        if alive:
            process.terminate()
    try:
        stdout, stderr = process.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate()
    return alive, process.returncode, stdout, stderr

before = native_settings()
with tempfile.TemporaryDirectory(prefix='7zip-isolated-startup-') as owned:
    root = Path(owned)
    folder = root / '日本語 space'
    folder.mkdir()
    (folder / 'owned.txt').write_text('Owned startup fixture 日本語\n', encoding='utf-8')
    profile = root / 'settings'
    environment = {k: v for k, v in os.environ.items()
                   if not k.startswith(('QT_', 'DYLD_', 'PORT_TEST_'))
                   and k not in {'CFFIXED_USER_HOME', 'CFPREFERENCES_AVOID_DAEMON'}}
    environment['SEVENZIP_PORT_SETTINGS_DIR'] = str(profile)
    alive, code, stdout, stderr = run(environment, ['--file-manager', str(folder)], True)
    files = list(profile.rglob('*.ini'))
    unchanged = native_settings() == before
    print('Owned bundle executable alive after 3 seconds:', alive)
    print('Private INI files created:', len(files))
    print('Existing matching native user preferences unchanged:', unchanged)
    # macOS InputMethodKit can log this while an otherwise usable background
    # app starts. Keep the exact OS diagnostic visible; never ignore Qt/engine
    # errors or other stderr merely because the process is still alive.
    input_method = re.compile(rb'^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+ 7-Zip Mac\[\d+:\d+\] error messaging the mach port for IMKCFRunLoopWakeUpReliable$')
    unexpected = []
    for line in stderr.splitlines():
        if input_method.fullmatch(line):
            print('Observed OS input-method diagnostic:', line.decode('utf-8'))
        else:
            unexpected.append(line)
    print('stdout bytes:', len(stdout), 'stderr bytes:', len(stderr), 'unexpected stderr lines:', len(unexpected))
    if not alive or not files or not unchanged or stdout or unexpected:
        for line in unexpected:
            print('Unexpected stderr:', line.decode('utf-8', errors='replace'))
        raise SystemExit(1)
    environment['SEVENZIP_PORT_SETTINGS_DIR'] = 'invalid-relative-profile'
    _, code, _, error = run(environment, [])
    unchanged = native_settings() == before
    rejected = code == 2 and b'Cannot initialize' in error
    print('Invalid explicit profile rejected without native fallback:', rejected)
    print('Native user preferences remain unchanged:', unchanged)
    print('Only owned processes terminated; physical Finder/Fn/menu input not checked.')
    if not rejected or not unchanged:
        raise SystemExit(1)
