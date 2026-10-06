#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Bound an owned verification command and its children without affecting other apps."""
import argparse
import os
import signal
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--timeout', type=int, default=900)
parser.add_argument('command', nargs=argparse.REMAINDER)
args = parser.parse_args()
if args.timeout <= 0 or not args.command:
    parser.error('a positive timeout and command are required')
process = subprocess.Popen(args.command, start_new_session=True)
try:
    code = process.wait(timeout=args.timeout)
except subprocess.TimeoutExpired:
    print('Verification timed out; stopping only the owned process group.', file=sys.stderr)
    os.killpg(process.pid, signal.SIGKILL)
    process.wait()
    sys.exit(124)
sys.exit(code if code >= 0 else 128 - code)
