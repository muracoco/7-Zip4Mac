#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Import unchanged English HTML Help from the official 7-Zip 26.03 Windows CHM."""
import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("engine", type=Path)
parser.add_argument("chm", type=Path)
args = parser.parse_args()
digest = hashlib.sha256(args.chm.read_bytes()).hexdigest()
if digest != "e0b70a83b79c938d7a868013ef94a0b1d34ff145f1e67bb7bf9a56aa913622cb":
    parser.error("Not the pinned official 7-Zip 26.03 7-zip.chm")
root = Path(__file__).resolve().parent.parent / "resources"
output = root / "help"
listing = subprocess.check_output([str(args.engine), "l", "-slt", str(args.chm)], text=True)
paths = sorted(line[7:] for line in listing.splitlines() if line.startswith("Path = ") and Path(line[7:]).suffix in {".htm", ".css", ".hhc", ".hhk"})
if len(paths) != 80 or any(Path(p).is_absolute() or ".." in Path(p).parts for p in paths):
    parser.error("Unexpected HTML Help inventory")
files = {}
for path in paths:
    data = subprocess.check_output([str(args.engine), "x", "-so", "-bd", "-y", str(args.chm), path])
    target = output / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)  # Retain upstream encoding, CRLF and copyright text.
    files[path] = hashlib.sha256(data).hexdigest()

class Sitemap(HTMLParser):
    def __init__(self):
        super().__init__()
        self.nodes = []
        self.depth = 0
        self.current = None
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "ul": self.depth += 1
        if tag == "object" and attrs.get("type") == "text/sitemap": self.current = {"depth": max(0, self.depth - 1)}
        if tag == "param" and self.current is not None:
            name = attrs.get("name", "").casefold()
            if name in {"name", "local"}: self.current[name] = attrs.get("value", "")
    def handle_endtag(self, tag):
        if tag == "ul": self.depth -= 1
        if tag == "object" and self.current is not None:
            if "local" in self.current: self.nodes.append(self.current)
            self.current = None

def sitemap(suffix):
    source = next(path for path in paths if path.endswith(suffix))
    parsed = Sitemap()
    parsed.feed((output / source).read_bytes().decode("cp1252"))
    return parsed.nodes

manifest = {"upstream": "7-Zip 26.03", "chmSha256": digest, "files": files, "contents": sitemap(".hhc"), "index": sitemap(".hhk")}
if len(manifest["contents"]) != 70 or len(manifest["index"]) != 69: parser.error("Unexpected sitemap inventory")
(output / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
resource = ET.Element("RCC")
group = ET.SubElement(resource, "qresource", prefix="/help")
for path in paths + ["manifest.json"]:
    ET.SubElement(group, "file", alias=path).text = "help/" + path
ET.indent(resource)
(root / "help.qrc").write_text(ET.tostring(resource, encoding="unicode") + "\n", encoding="utf-8")
print(f"Imported {len(paths)} original files, 70 contents entries and 69 index entries")
