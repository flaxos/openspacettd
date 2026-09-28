#!/usr/bin/env python3
"""Remove a company from the integrated native save, then cold-load neutral factories."""
import argparse
import hashlib
import json
from pathlib import Path

from test_wp11_slice import Engine, ROOT, require


def state(engine):
    return json.loads(engine.command('resource_sites', 'RESOURCE state '))['integrated_economy']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/openttd')
    parser.add_argument('--save', type=Path, default=ROOT / 'demo/OpenSpaceTTD-Integrated-Economy-UAT.sav')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    config = ROOT / 'demo/integrated_economy.cfg'
    engine = Engine(args.binary.resolve(), config, output, 'bankruptcy', save=args.save.resolve())
    saved = output / 'neutral-factories.sav'
    try:
        before = state(engine)
        require(before['version'] == 1 and before['factories'], 'Expected integrated factories')
        engine.command('reset_company 1', 'Company deleted.')
        for _ in range(100):
            after = state(engine)
            if all(factory[1] == 16 for factory in after['factories']):
                break
        require(len(after['factories']) == len(before['factories']), 'Company removal deleted physical factories')
        require(all(factory[1] == 16 for factory in after['factories']), 'Removed company retained factory ownership')
        require(not after['research'], 'Removed company retained research escrow')
        engine.save(saved)
    finally:
        engine.close()
    loaded = Engine(args.binary.resolve(), config, output, 'reload', save=saved)
    try:
        require(state(loaded) == after, 'Cold reload changed neutral factory state')
    finally:
        loaded.close()
    report = {'passed': True, 'factories': len(after['factories']), 'neutral_owner': 16,
              'cold_reload': True, 'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(),
              'source_save_sha256': hashlib.sha256(args.save.read_bytes()).hexdigest(),
              'save_sha256': hashlib.sha256(saved.read_bytes()).hexdigest()}
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Native company removal and neutral-factory cold reload passed', flush=True)


if __name__ == '__main__':
    main()
