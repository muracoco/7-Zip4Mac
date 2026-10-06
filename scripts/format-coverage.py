#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Record each official handler/extension and real QtTest outcome."""
import datetime
import json
import pathlib
import sys
import xml.etree.ElementTree as ET

manifest, xml, markdown, output = map(pathlib.Path, sys.argv[1:5])
followups = list(map(pathlib.Path, sys.argv[5:]))
source=json.loads(manifest.read_text())
inventory=json.loads((pathlib.Path(__file__).resolve().parents[1]/'resources/formats.json').read_text())
incidents={}
incomplete=[]
for report in [xml, *followups]:
    if not report.exists():
        incomplete.append(str(report)); continue
    function=None
    try:
        # A terminated Qt process may leave a partial XML document. Retain only
        # fully written incidents; later affected runs supersede those cases.
        for event, element in ET.iterparse(report, events=('start','end')):
            if event=='start' and element.tag=='TestFunction': function=element.attrib['name']
            elif event=='end' and element.tag=='Incident':
                tag=element.findtext('DataTag')
                if function=='genuineFormats' and tag: incidents[tag]=element.attrib['type']
                element.clear()
            elif event=='end' and element.tag=='TestFunction': function=None
    except ET.ParseError:
        incomplete.append(str(report))
        print('Incomplete Qt report; retained completed incidents: '+str(report),file=sys.stderr)
cases={(row['format'],row['extension']):row for row in source['cases']}
rows=[]
for format in inventory['formats']:
    for extension in format['extensions']:
        case=cases.get((format['name'],extension)); status=incidents.get(format['name']+'.'+extension,'not run' if case else 'fixture pending')
        rows.append({'format':format['name'],'extension':extension,'status':status,'expectedTestExit':case.get('expectedTestExit',0) if case else None,'expectedExtractExit':case.get('expectedExtractExit',0) if case else None,'provenance':case['provenance'] if case else None})
data={'version':source['version'],'timestamp':datetime.datetime.now().astimezone().isoformat(),'handlers':len(inventory['formats']),'extensions':len(inventory['extensions']),'registrationCases':len(rows),'passed':sum(row['status']=='pass' for row in rows),'generationFailures':source['generationFailures'],'incompleteReports':incomplete,'rows':rows}
output.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
lines=['# Archive format verification','',f"7-Zip {data['version']}: **{data['passed']} / {len(rows)} registration cases passed**, {data['handlers']} handlers / {data['extensions']} distinct extensions.",'','Tests verify the intended official handler, backend listing, Test result, extraction result, SHA-256 of original data and File Manager archive reading. Container aliases use explicit Open Inside; the original default external-open profile is checked separately. The context-menu test uses Qt-delivered mouse events. Native desktop/Finder clicks require a separate unlocked-session test.','', 'A passing row with an expected nonzero exit verifies an upstream engine limitation, not successful extraction or hashing. Hash manifests have no archived payload; console x reports E_NOTIMPL. Unsupported checksum algorithms retain the official engine error. Container aliases verify archive reading, not application-specific document/viewer semantics.','']
if incomplete: lines.extend([f'{len(incomplete)} earlier report(s) are incomplete. Counts use completed incidents and any supplied affected follow-up runs; this does not claim that the earlier process completed.',''])
lines.extend(['| Handler | Extension | Result | Expected Test / Extract exit |','|---|---|---|---|'])
for row in rows:lines.append(f"| {row['format']} | .{row['extension']} | {row['status']} | {row['expectedTestExit']} / {row['expectedExtractExit']} |")
markdown.write_text('\n'.join(lines)+'\n')
print(f"Coverage: {data['passed']} / {len(rows)} registration cases passed")
