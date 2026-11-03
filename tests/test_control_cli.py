"""Offline acceptance tests: no key, API calls, board, or virtual serial pair needed."""
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

exe = str(Path(sys.argv[1]).resolve())
example = Path(sys.argv[2]).resolve()
dependency = example.parents[1] / 'third_party' / 'nlohmann'
manifest = json.loads((dependency / 'manifest.json').read_text(encoding='utf-8'))
for name, metadata in manifest['files'].items():
    assert hashlib.sha256((dependency / name).read_bytes()).hexdigest() == metadata['sha256']
env = os.environ.copy()
env.pop('DEEPSEEK_API_KEY', None)
env['MCU_SERIAL_PORT'] = 'INVALID_MUST_NOT_BE_USED'

def invoke(*args, code=0, stdin=''):
    result = subprocess.run([exe, *map(str, args)], input=stdin, text=True,
                            encoding='utf-8', env=env, capture_output=True, timeout=10)
    if result.returncode != code:
        raise AssertionError((args, result.returncode, result.stdout, result.stderr))
    return result

with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    invoke('--help')
    out = root / 'demo'
    invoke('--demo', '--simulate', '--output', out)
    report = json.loads((out / 'run.json').read_text(encoding='utf-8'))
    assert report['status'] == 'completed' and report['source'] == 'fixture'
    assert report['mode'] == 'firmware-decoder-simulation'
    assert report['samples'] == report['framesWritten'] == report['framesAcknowledged'] == 121
    assert report['durationMs'] == 6000 and report['bytesWritten'] == 1210
    rows = list(csv.DictReader((out / 'trajectory.csv').open(encoding='utf-8')))
    assert len(rows) == 121 and float(rows[40]['x_grid']) == 2
    assert float(rows[-1]['y_grid']) == 4 and int(rows[-1]['time_ms']) == 6000
    svg = ET.parse(out / 'preview.svg').getroot()
    assert len(svg.findall('.//{http://www.w3.org/2000/svg}animate')) == 2
    invoke('--demo', '--output', out, code=2)  # Existing output must not be overwritten.
    preview = root / '中文预览'
    invoke('--plan', example, '--output', preview)
    assert json.loads((preview / 'run.json').read_text(encoding='utf-8'))['framesWritten'] == 0
    cancelled = root / 'cancelled'
    invoke('--demo', '--execute', '--port', 'COM3', '--output', cancelled, stdin='NO\n', code=4)
    assert json.loads((cancelled / 'run.json').read_text(encoding='utf-8'))['status'] == 'cancelled'
    command = json.loads(example.read_text(encoding='utf-8'))
    envelope = {'choices': [{'finish_reason': 'stop', 'message': {'role': 'assistant',
                                                              'content': json.dumps(command)}}]}
    fixture = root / 'response.json'
    fixture.write_text(json.dumps(envelope), encoding='utf-8')
    recorded = root / 'recorded'
    invoke('--provider-response', fixture, '--output', recorded)
    assert json.loads((recorded / 'run.json').read_text(encoding='utf-8'))['source'] == 'provider-fixture'
    bad = root / 'bad.json'
    command['plan']['actions'][0]['to']['x'] = 100
    bad.write_text(json.dumps(command), encoding='utf-8')
    invalid = root / 'invalid'
    invoke('--plan', bad, '--execute', '--port', 'COM3', '--output', invalid, stdin='EXECUTE\n', code=2)
    assert not invalid.exists()
    bad.write_text('{"status":"clarification","question":"请提供完整指令。"}', encoding='utf-8')
    invoke('--plan', bad, '--execute', '--port', 'COM3', '--output', invalid, stdin='EXECUTE\n', code=3)
    assert not invalid.exists()
    invoke('--text', '从左下角走到右上角', '--output', invalid, code=2)  # No key, no network.
    assert not invalid.exists()
    invoke('--demo', '--execute', '--output', invalid, code=2)
    invoke('--demo', '--port', 'COM3', '--output', invalid, code=2)
    invoke('--demo', '--demo', '--output', invalid, code=2)
print('PASS: offline CLI, artifacts, provider envelope, invalid plan, clarification, cancellation, no-key behavior')
