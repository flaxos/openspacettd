#!/usr/bin/env python3
"""Prepare, then execute the one owner-authorized resumed Core basket campaign.

Preparation is the default and never launches a game. After source checks and the
final build, use --execute with the same output directory exactly once. This is
ASSISTED FUNCTIONAL evidence, not an ordinary-start or independent replay test.
"""
import argparse
import copy
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import time
import unittest

from test_commonwealth_platform_art import environment, profile_environment
from test_integrated_economy import PACKS, acceptance_inputs, audit
from test_wp11_slice import OfflineEngine, ROOT, console_payload, require


ARCHIVE = ROOT / 'docs/audit/2026-10-01/functional-core/continuation-proof.tar.gz'
ARCHIVE_SHA256 = '43115195bef21ae59dce8c33a607fa01ff070d13a78ced13b170c028e5b1a896'
SOURCE_SHA256 = '206acdd9485bed64927197af89e07a70926e0b1e386ed51cb93744df2c78e2f7'
SOURCE_CONFIG_SHA256 = 'b8e43582d002dbe239aa8a0037c64d97e529a24f342b80c9b5033c69c8834009'
FROZEN_STATE_SHA256 = '4f0094aa5ccadbea15c6cfd4257bfe656726a00ac9f7e39c7ccd244bc4016d26'
START_CASH = 3705864
DEBT = 100000
START_TICK = 167168
TICKS_PER_ADVANCE = 2048
QUOTE_CAP = None
GROSS_CAP = None
RECOVERY_CLOCK = {'start_utc': '2026-10-03T00:58:49.320471+00:00',
                  'stop_utc': '2026-10-03T02:13:49.320471+00:00',
                  'trigger': 'Retained first failed advance; no clock reset', 'duration_seconds': 4500}
RECOVERY_INITIAL_STOP = RECOVERY_CLOCK['stop_utc']  # Explicit phase allocation; original outer clock is unchanged.
PRIOR_GROSS_DEBITS = 1095834  # All preserved paid attempts; no gross cap remains.
PRIOR_REQUESTED_ADVANCES = 15
PRIOR_ACTUAL_TICKS = 28672
CHECKPOINT_TICK = 189696
CHECKPOINT_SHA256 = 'ae2bbfeb73d24648cfc17bb529dee92b7d17e614ef2f385cd7707554df0b47e2'
CHECKPOINT_OUTPUT = ROOT / 'build/agent-logs/core-basket-campaign-checkpoint-dbc07c'
CHECKPOINT_DEBITS = 366598
CHECKPOINT_QUOTE = 364281
HISTORICAL_PRIMARY_DEBITS = 2761963
HISTORICAL_CONTROL_DEBITS = 4809
HISTORICAL_ADVANCES = {'initial': 82, 'cold': 24, 'initial_controls': 25, 'total': 106}
ADVANCE_CAPS = {'initial': 240, 'cold': 120}
PHASE_SECONDS = {'preflight': 1800, 'initial': 1800, 'cold': 900}
OUTER_SECONDS = 4500
LABELS = ('IRON', 'STEL', 'SILC', 'BALL')
CARGO_COUNT = 64


class CampaignStop(RuntimeError):
    """An exhausted authorized bound is partial evidence, not a passed proof."""


def utc_now():
    return datetime.now(timezone.utc)


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')).encode()


def value_sha256(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def write_json(path, value, exclusive=False):
    """Flush a new immutable artifact, or atomically replace durable progress."""
    path = Path(path)
    target = path if exclusive else path.with_name(path.name + '.tmp')
    with target.open('x' if exclusive else 'w') as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    if not exclusive:
        os.replace(target, path)


def write_text_exclusive(path, value):
    with Path(path).open('x') as stream:
        stream.write(value)
        stream.flush()
        os.fsync(stream.fileno())


def safe_console_path(path):
    path = str(Path(path).resolve())
    require(not any(character in path for character in ('"', '\n', '\r', '\0')),
            'A native console path cannot be safely quoted')
    return f'"{path}"'


def authorized_command(command):
    """A closed command set cannot reach a grant, loan, fresh game or research."""
    if command in ('echo FUNCTIONAL_OFFLINE_READY', 'connected_economy audit',
                   'connected_economy basket-status', 'connected_economy basket-advance',
                   'connected_economy basket-dev-money'):
        return True
    patterns = (r'connected_economy basket-(?:plan|build) [01]',
                r'connected_economy basket-(?:stop|restart) [0-9]+',
                r'connected_economy basket-arm "[^"\n\r\x00]+"',
                r'save "[^"\n\r\x00]+"')
    return any(re.fullmatch(pattern, command) for pattern in patterns)


def pairs(rows):
    require(isinstance(rows, list) and all(isinstance(row, list) and len(row) == 2 for row in rows),
            'Malformed native cargo map')
    result = dict(rows)
    require(len(result) == len(rows), 'Duplicate cargo in native cargo map')
    require(all(isinstance(c, int) and 0 <= c < CARGO_COUNT and isinstance(n, int) and n >= 0
                for c, n in result.items()), 'Invalid native cargo map quantity')
    return result


def source_payload():
    """Read named regular members only; never extract an archive over a workspace."""
    require(sha256(ARCHIVE) == ARCHIVE_SHA256, 'Historical continuation archive hash differs')
    names = ('campaign/owner-observation.sav', 'campaign/evidence.json', 'campaign/functional.cfg')
    with tarfile.open(ARCHIVE, 'r:gz') as archive:
        result = {}
        for name in names:
            members = [member for member in archive.getmembers() if member.name == name]
            require(len(members) == 1 and members[0].isfile(), f'Invalid archived source member: {name}')
            result[name] = archive.extractfile(members[0]).read()
    save = result[names[0]]
    evidence = json.loads(result[names[1]])
    config = result[names[2]]
    require(hashlib.sha256(save).hexdigest() == SOURCE_SHA256, 'Archived observation save hash differs')
    require(hashlib.sha256(config).hexdigest() == SOURCE_CONFIG_SHA256, 'Archived canonical profile differs')
    frozen = evidence['final_state']
    require(value_sha256(frozen) == FROZEN_STATE_SHA256, 'Archived complete final_state differs')
    validate_source(frozen)
    require(evidence['financial_reconciliation']['total_primary_debits'] == HISTORICAL_PRIMARY_DEBITS and
            evidence['financial_reconciliation']['disposable_control_debits'] == HISTORICAL_CONTROL_DEBITS and
            evidence['assistance_used_gbp'] == 6000000 and evidence['assistance_remaining_gbp'] == 0,
            'Historical provenance differs from the approved handoff')
    return save, frozen, config.decode(), evidence['inputs']['pack_sha256'], {
        name: hashlib.sha256(data).hexdigest() for name, data in result.items()}


def validate_source(state):
    require(state['tick'] == START_TICK and state['money'] == START_CASH and state['loan'] == DEBT and
            state['seed'] == 11 and state['research'] == [[0, 0, 0, [301]]],
            'The retained tick/cash/debt/Materials I/research-off contract differs')
    town = next(row for row in state['towns'] if row['id'] == 0)
    require(town['world'] == 0 and town['population'] == 1191 and town['profile']['growth'] == 0 and
            town['profile']['passengers'] == 0.5 and pairs(town['reserves']).get(9, 0) == 0 and
            pairs(town['reserves']).get(30, 0) == 0 and pairs(town['reserves']).get(33, 0) == 120,
            'The retained Core basket differs')
    receiver = next(row for row in state['stations'] if row['id'] == 3)
    require(receiver['consumer'] and receiver['town'] == 0 and not receiver['warehouse'] and
            any(house['town'] == 0 and house['tile'] == 68378 for house in receiver['catchment_houses']),
            'The retained Core receiver no longer catches its own house')
    for industry, world, tile, recipe in ((6, 2, 343640, 0), (7, 1, 227925, 102),
                                          (2, 3, 531248, 0), (4, 1, 243507, 101)):
        row = next(row for row in state['industries'] if row['id'] == industry)
        require((row['world'], row['tile'], row['recipe'], row['owner']) == (world, tile, recipe, 16),
                f'Fixed neutral industry {industry} differs')
    require(all(row[5] == 0 for row in state['economy']['factories'] if row[0] in (4, 7)),
            'Material processing was already completed in the source')


def pinned_inputs(binary):
    """Reuse the economy hash helper, including this runner and all tracked code."""
    binary = binary.resolve()
    require(binary.is_file() and os.access(binary, os.X_OK), 'The final native binary is unavailable')
    result = acceptance_inputs(binary)
    result['binary_path'] = str(binary)
    tracked = subprocess.check_output(
        ['git', 'ls-files', '-z', '--', 'src', 'scripts', 'pkg', 'CMakeLists.txt', 'cmake'], cwd=ROOT)
    files = {name.decode() for name in tracked.split(b'\0') if name}
    files.update(('scripts/test_core_construction_basket.py', 'AGENTS.md',
                  'docs/ACTIVE_EXECUTION_PLAN.md', 'docs/SAVEGAME_AUTHORING.md'))
    result['working_files_sha256'] = {name: sha256(ROOT / name) for name in sorted(files) if (ROOT / name).is_file()}
    result['archive_sha256'] = sha256(ARCHIVE)
    return result


def relocated_config(source):
    """Change only the three archive-specific GRF paths in the canonical profile."""
    require('[newgrf-static]' not in source and '[ai]' not in source, 'Optional art or an AI profile is forbidden')
    required = ('autosave_interval = 0', 'autosave_on_exit = false', 'threaded_saves = false',
                'min_active_clients = 1', 'infinite_money = false', 'generation_seed = 11',
                'language = english.lng')
    require(all(source.count(line) == 1 for line in required), 'Unsafe or noncanonical source profile')
    lines = source.splitlines(keepends=True)
    changed = []
    for pack in PACKS:
        matches = [i for i, line in enumerate(lines) if line.strip().endswith(f'/openspacettd_{pack}.grf =')]
        require(len(matches) == 1, f'Canonical profile does not identify one {pack} pack')
        index = matches[0]
        changed.append({'from': lines[index].strip(), 'to': f'{ROOT}/bin/newgrf/openspacettd_{pack}.grf ='})
        lines[index] = changed[-1]['to'] + '\n'
    return ''.join(lines), changed


def initial_report(inputs):
    return {
        'version': 1, 'label': 'ASSISTED FUNCTIONAL', 'status': 'NOT RUN', 'stage': 'prepared',
        'human_uat': 'Pending', 'ordinary_start_economic_proof': 'NOT RUN', 'fresh_replay': 'NOT RUN',
        'ordinary_economics_note': 'Prior revised A1 failure retained; inherited cash is assisted.',
        'processor_scope': 'Physical use of existing neutral processors; Materials I is retained and did not newly enable them.',
        'assistance': {'historical_allowance': 6000000, 'historical_used': 6000000,
                       'historical_remaining': 0, 'development_unlimited_money': True,
                       'new_grant_commands': 0, 'new_grant_amount': 0},
        'historical_debits': {'primary': HISTORICAL_PRIMARY_DEBITS, 'disposable_control': HISTORICAL_CONTROL_DEBITS},
        'historical_advances': copy.deepcopy(HISTORICAL_ADVANCES),
        'retained_failed_basket_attempts': {'gross_debits': PRIOR_GROSS_DEBITS,
                                            'requested_advances': PRIOR_REQUESTED_ADVANCES,
                                            'actual_ticks': PRIOR_ACTUAL_TICKS,
                                            'game_loads': 3},
        'bounds': {'layout_candidates': 2, 'endpoint_calls_per_layout': 8, 'station_candidates_per_call': 16,
                   'predecessor_states_per_call': 30000, 'bridge_span': 16, 'quoted_debits': QUOTE_CAP,
                   'new_gross_debits': GROSS_CAP, 'advance_caps': ADVANCE_CAPS,
                   'ticks_per_advance': TICKS_PER_ADVANCE, 'outer_wall_seconds': OUTER_SECONDS,
                   'phase_wall_seconds': PHASE_SECONDS,
                   'initial_phase_override_stop_utc': RECOVERY_INITIAL_STOP},
        'load_attempts': {'initial': 0, 'cold': 0}, 'fresh_games_started': 0,
        'advance_counts': {'initial': PRIOR_REQUESTED_ADVANCES, 'cold': 0},
        'simulation_clock': copy.deepcopy(RECOVERY_CLOCK),
        'native_receipts': 0, 'native_debits': CHECKPOINT_DEBITS, 'native_transactions': [], 'operations': [],
        'native_ledger_complete': True, 'native_state_current': True, 'accounted_sequences': [],
        'phases': {name: 'NOT RUN' for name in ('admission', 'plan', 'construction', 'missing_ball',
                                              'initial_basket', 'cold_equality', 'cold_basket', 'final_audit', 'final_save')},
        'inputs': inputs, 'prepared_at_utc': utc_now().isoformat(),
    }


def prepare(binary, output):
    safe_console_path(output)
    require(not output.exists(), 'Preserve earlier evidence; preparation requires a new output directory')
    save, frozen, archived_config, pack_hashes, members = source_payload()
    checkpoint = CHECKPOINT_OUTPUT / 'checkpoints/advance-014.sav'
    require(checkpoint.is_file() and sha256(checkpoint) == CHECKPOINT_SHA256,
            'The verified paused continuation checkpoint differs')
    resume_state = json.loads((CHECKPOINT_OUTPUT / 'checkpoints/advance-014-state.json').read_text())
    resume_observation = json.loads((CHECKPOINT_OUTPUT / 'checkpoints/advance-014-observation.json').read_text())
    prior = json.loads((CHECKPOINT_OUTPUT / 'evidence.json').read_text())
    require(resume_state['tick'] == CHECKPOINT_TICK and resume_state['money'] == START_CASH - CHECKPOINT_DEBITS and
            prior['saves']['checkpoints/advance-014.sav'] == CHECKPOINT_SHA256 and
            prior['selected_quote'] == CHECKPOINT_QUOTE and prior['advance_counts']['initial'] == PRIOR_REQUESTED_ADVANCES,
            'The checkpoint state or retained failed-attempt ledger differs')
    inputs = pinned_inputs(binary)
    require(inputs['pack_sha256'] == pack_hashes, 'Published content differs from the archived observation save')
    config, changes = relocated_config(archived_config)
    output.mkdir(parents=True)
    for name in ('source', 'initial', 'cold', 'operations', 'checkpoints', 'diagnostic'):
        (output / name).mkdir()
    with (output / 'source/owner-observation.sav').open('xb') as stream:
        stream.write(save)
        stream.flush()
        os.fsync(stream.fileno())
    with (output / 'source/resume.sav').open('xb') as stream:
        stream.write(checkpoint.read_bytes())
        stream.flush()
        os.fsync(stream.fileno())
    write_json(output / 'source/frozen-state.json', frozen, exclusive=True)
    write_json(output / 'source/resume-state.json', resume_state, exclusive=True)
    write_json(output / 'source/resume-observation.json', resume_observation, exclusive=True)
    write_json(output / 'source/resume-plan.json', json.loads((CHECKPOINT_OUTPUT / 'frozen-plan.json').read_text()), exclusive=True)
    write_json(output / 'source/resume-footprint.json', json.loads((CHECKPOINT_OUTPUT / 'paid-footprint.json').read_text()), exclusive=True)
    write_json(output / 'source/resume-arm.json', {'state': resume_state, 'observation': resume_observation, 'basket': {
        'source_sha256': SOURCE_SHA256, 'initial_advances': PRIOR_REQUESTED_ADVANCES,
        'gross_debits': CHECKPOINT_DEBITS, 'quoted_total': CHECKPOINT_QUOTE,
        'services': prior['services'], 'resume_initial': True}}, exclusive=True)
    write_text_exclusive(output / 'source/archived-functional.cfg', archived_config)
    for name in ('initial', 'cold'):
        write_text_exclusive(output / name / 'functional.cfg', config)
    preparation = {
        'version': 1, 'inputs': inputs, 'source_save_sha256': SOURCE_SHA256,
        'resume_checkpoint_sha256': CHECKPOINT_SHA256, 'resume_checkpoint_tick': CHECKPOINT_TICK,
        'frozen_state_sha256': FROZEN_STATE_SHA256, 'archive_members_sha256': members,
        'canonical_profile_sha256': SOURCE_CONFIG_SHA256, 'profile_path_changes': changes,
        'prepared_files_sha256': {str(path.relative_to(output)): sha256(path)
                                  for path in output.rglob('*') if path.is_file()},
    }
    write_json(output / 'preparation.json', preparation, exclusive=True)
    report = initial_report(inputs)
    report['selected_quote'] = CHECKPOINT_QUOTE
    report['services'] = prior['services']
    report['phases']['plan'] = 'PASS at retained checkpoint'
    report['phases']['construction'] = 'PASS at retained checkpoint'
    report['preparation_sha256'] = sha256(output / 'preparation.json')
    write_json(output / 'evidence.json', report)
    print(f'NOT RUN: prepared exact retained checkpoint and inputs in {output}; no native process launched.', flush=True)


def validate_execution_admission(report, preparation, current_inputs, output):
    require(report['status'] == 'NOT RUN' and report['stage'] == 'prepared' and
            report['load_attempts'] == {'initial': 0, 'cold': 0} and
            report['advance_counts'] == {'initial': PRIOR_REQUESTED_ADVANCES, 'cold': 0} and
            report['simulation_clock'] == RECOVERY_CLOCK,
            'This campaign was already attempted; a retry requires another explicit owner handoff')
    require(preparation['version'] == 1 and current_inputs == preparation['inputs'] == report['inputs'],
            'Pinned binary/source/content inputs changed after preparation')
    require(report['preparation_sha256'] == sha256(output / 'preparation.json'), 'Immutable preparation manifest changed')
    for name, expected in preparation['prepared_files_sha256'].items():
        path = output / name
        require(path.is_file() and sha256(path) == expected, f'Prepared artifact changed: {name}')
    require(sha256(output / 'source/owner-observation.sav') == SOURCE_SHA256 and
            sha256(output / 'source/resume.sav') == CHECKPOINT_SHA256 and
            value_sha256(json.loads((output / 'source/frozen-state.json').read_text())) == FROZEN_STATE_SHA256,
            'Copied source save or full frozen state changed')


def observation_projection(observation):
    """Only native adapter counters are transient; compare every other added field."""
    return {key: value for key, value in observation.items() if key != 'adapter'}


def exact_paused_equality(value, expected_state, expected_observation=None):
    require(value['version'] == 1 and value['state'] == expected_state,
            'Full paused retained-state equality failed; no field normalization is permitted')
    require(value['observation']['paused'], 'Native state is not paused')
    if expected_observation is not None:
        require(observation_projection(value['observation']) == observation_projection(expected_observation),
                'Full paused extended basket/custody/native growth equality failed')


def normalize_unbuilt_vegetation(original, current):
    """Treat ordinary tree growth on clear, neutral, unbuilt candidate tiles as terrain life."""
    projected = copy.deepcopy(current)
    for before, after in zip(original.get('rails', ()), projected.get('rails', ())):
        if (before.get('type') == 0 and after.get('type') == 4 and
                before.get('owner') == after.get('owner') == 16 and
                'tracks' not in before and 'tracks' not in after and
                'railtype' not in before and 'railtype' not in after):
            after['type'] = 0
    return projected


def verify_protected_topology(state, frozen, plan=None, built_footprint=None):
    """Protect public infrastructure while validating the declared private joins."""
    for key in ('gates', 'stocks'):
        require(state[key] == frozen[key], f'Protected {key} changed')
    require([{key: value for key, value in world.items() if key != 'development'} for world in state['worlds']] ==
            [{key: value for key, value in world.items() if key != 'development'} for world in frozen['worlds']] and
            all(isinstance(world['development'], int) and world['development'] >= 0 for world in state['worlds']),
            'Protected world identity, role, phase or geometry changed')
    old_stellar, stellar = frozen['stellar'], state['stellar']
    require({key: value for key, value in stellar.items() if key != 'admitted'} ==
            {key: value for key, value in old_stellar.items() if key != 'admitted'},
            'Static stellar worlds/policies/projects/zones changed')
    fronts = {row['id'] for row in state['trains'] if row['front'] and row['owner'] == 0}
    public_heads = {end['tile'] for gate in state['gates'] for end in gate['ends'] if end['public']}
    admitted = stellar['admitted']
    require(all(isinstance(row, list) and len(row) == 2 and row[0] in fronts and row[1] in public_heads for row in admitted) and
            len({row[0] for row in admitted}) == len(admitted), 'Stellar admission names an invalid front or public gate')
    additions = {}
    for command in (() if plan is None else plan['commands']):
        if command['kind'] == 2:  # BasketCommand::Kind::Rail; native enum order is pinned.
            require(0 <= command['track'] < 6, 'Frozen rail command has an invalid track')
            additions[command['tile']] = additions.get(command['tile'], 0) | (1 << command['track'])
    for key in ('terminals', 'zones'):
        require(len(state[key]) == len(frozen[key]), 'A public terminal or arrival zone was added or removed')
        for index, (old, current) in enumerate(zip(frozen[key], state[key])):
            if key == 'zones':
                # Arrival zones advertise future candidates. Their unbuilt rails,
                # signals and planning projection can change as trees grow; the
                # actual selected network is checked against paid footprint below.
                require(all(current.get(name) == old.get(name) for name in ('world', 'dir')) and
                        all(current['head'].get(name) == old['head'].get(name)
                            for name in ('tile', 'valid', 'world', 'owner', 'height', 'slope')),
                        'Advertised zone identity, ownership or terrain changed')
                if old['head'].get('type') not in (0, 4):
                    require(current['head'] == old['head'], 'Built arrival-zone head changed')
                continue
            require({name: value for name, value in normalize_unbuilt_vegetation(old, current).items() if name not in ('join', 'signals')} ==
                    {name: value for name, value in old.items() if name not in ('join', 'signals')},
                    'Neutral terminal head/rails or static metadata changed')
            require(len(current['signals']) == len(old['signals']), 'Neutral signal topology changed')
            for original, signal in zip(old['signals'], current['signals']):
                require({name: value for name, value in signal.items() if name != 'state_bits'} ==
                        {name: value for name, value in original.items() if name != 'state_bits'},
                        'Neutral signal presence/type/variant/direction changed')
                if 'state_bits' in signal:
                    require(isinstance(signal['state_bits'], int) and 0 <= signal['state_bits'] <= 15,
                            'Native signal state is invalid')
            if 'join' not in old:
                require('join' not in current, 'A terminal join was authored')
                continue
            if built_footprint is not None:
                expected = built_footprint[key][index]['join']
            else:
                expected = copy.deepcopy(old['join'])
                new_bits = additions.get(expected['tile'], 0)
                if new_bits:
                    require(expected['owner'] in (0, 16) and expected['type'] in (0, 1),
                            'The frozen plan attempts to replace neutral join infrastructure')
                    expected.update(owner=0, type=1, railtype=0, tracks=expected.get('tracks', 0) | new_bits)
            require(current.get('join') == expected, 'Private join differs from the exact declared paid footprint')
    if built_footprint is not None:
        require(state['rail'] == built_footprint['rail'] and state['structures'] == built_footprint['structures'],
                'Paid rail/bridge topology changed after construction')


def verify_observation(state, observation, frozen, services=None, plan=None, built_footprint=None):
    adapter = observation['adapter']
    require(observation['paused'] and not adapter['failed'], 'Native pause or integrity guard failed')
    require((not adapter['armed'] or adapter['development_unlimited_money']) and
            0 <= adapter['layout_candidates'] <= 2 and
            0 <= adapter['endpoint_invocations'] <= 8 * adapter['layout_candidates'] and
            0 <= adapter['advances'] <= (120 if adapter['cold'] else 240) and
            0 <= adapter['initial_advances'] <= 240 and adapter['initial_advances'] + adapter['advances'] <= 360,
            'Native adapter search/spend/phase limits differ from the fixed handoff')
    labels = observation['cargo_labels']
    require(all(label in labels for label in ('FOOD', 'GRAI', *LABELS)) and
            len({labels[label] for label in ('FOOD', 'GRAI', *LABELS)}) == 6,
            'Required cargo labels are absent or alias')
    require(all(labels[label] == slot for label, slot in frozen['cargo_labels'].items()),
            'Retained cargo labels changed')
    custody = observation['custody']
    domains = ('industry_inputs', 'industry_outputs', 'stations', 'trains', 'cities', 'stocks', 'research')
    require(all(len(custody[name]) == CARGO_COUNT and all(isinstance(n, int) and n >= 0 for n in custody[name])
                for name in (*domains, 'total')), 'Incomplete all-cargo custody observation')
    require(custody['total'] == state['all_held'] and
            all(sum(custody[name][c] for name in domains) == custody['total'][c] for c in range(CARGO_COUNT)),
            'Custody domains do not sum to the complete native physical holdings')
    if built_footprint is not None:
        for name, live_fields in (('private_rail', {'signal_state', 'reservation'}),
                                  ('private_structures', {'reservation'})):
            static = [{key: value for key, value in row.items() if key not in live_fields} for row in observation[name]]
            expected = [{key: value for key, value in row.items() if key not in live_fields} for row in built_footprint[name]]
            require(static == expected, 'Paid private signal/rail/depot/bridge topology changed')
    require(state['loan'] == DEBT and state['seed'] == 11 and
            state['research'] == frozen['research'] and state['hqs'] == frozen['hqs'] and
            state['economy']['research'] == frozen['economy']['research'],
            'Debt, retained HQ/unlock or research-off contract changed')
    require(not any(train['lost'] or train['crashed'] for train in state['trains']), 'A train is lost or crashed')
    for industry in (2, 4, 6, 7):
        row = next(row for row in state['industries'] if row['id'] == industry)
        old = next(row for row in frozen['industries'] if row['id'] == industry)
        require(all(row[key] == old[key] for key in ('world', 'tile', 'recipe', 'owner')), 'Fixed neutral industry changed')
    verify_protected_topology(state, frozen, plan, built_footprint)
    old_trains = {row['id']: row for row in frozen['trains']}
    trains = {row['id']: row for row in state['trains']}
    require(all(identifier in trains and trains[identifier]['orders'] == row['orders'] and
                trains[identifier]['owner'] == row['owner'] and trains[identifier]['engine'] == row['engine']
                for identifier, row in old_trains.items()), 'An existing FOOD/grain consist or order changed')
    fronts = [row for row in state['trains'] if row['front']]
    require(len(fronts) <= 6 and len(state['industries']) == len(frozen['industries']),
            'The authorized fleet or industry scope expanded')
    if services is not None:
        require(len(fronts) == 6 and len(services) == 4, 'All six services are not retained')
        for service in services:
            train = trains[service['train']]
            require(train['front'] and train['owner'] == 0 and train['engine'] == service['locomotive']['id'],
                    'Material service front is invalid')
            expected_orders = copy.deepcopy(service['orders'])
            expected_orders[0][1], expected_orders[1][1] = service['pickup'], service['drop']
            require(train['orders'] == expected_orders, 'Material service ordinary orders changed')
            visited = {train['id']}
            count = 0
            while train['next'] != 4294967295:
                require(train['next'] in trains and train['next'] not in visited, 'Broken or cyclic material consist')
                train = trains[train['next']]
                visited.add(train['id'])
                count += 1
                require(train['cargo_type'] == service['cargo'] and train['engine'] == service['wagon']['id'] and
                        train['capacity'] == service['wagon']['capacity'], 'Material wagon engine/refit/capacity changed')
            require(1 <= count <= 3 and count == service['wagons'], 'Material consist exceeds three wagons')
        for service in (services[1], services[3]):
            receiver = next(row for row in state['stations'] if row['id'] == service['drop'])
            require(service['drop'] == 3 and receiver['consumer'] and receiver['town'] == 0 and not receiver['warehouse'] and
                    any(h['town'] == 0 and h['world'] == 0 for h in receiver['catchment_houses']),
                    'Material payment receiver is absent, diverted or catches another town')


def verify_built_services(services, plan, state):
    """The paid consists, costs, station identities and orders match the frozen quote."""
    require(len(services) == len(plan['services']) == 4, 'The paid service set differs from the frozen quote')
    stations = {row['id']: row for row in state['stations']}
    fronts = {row['id']: row for row in state['trains'] if row['front']}
    for service, expected in zip(services, plan['services']):
        require({key: service[key] for key in expected} == expected, 'Paid service metadata/consist/cost differs from the frozen plan')
        for role in ('pickup', 'drop'):
            station = stations[service[role]]
            require(station['owner'] == 0 and not station['warehouse'] and station['tile'] == expected[role + '_tile'],
                    'Paid station identity or route footprint differs from the frozen plan')
        orders = copy.deepcopy(expected['orders'])
        orders[0][1], orders[1][1] = service['pickup'], service['drop']
        require(fronts[service['train']]['orders'] == orders, 'Paid native orders differ from the frozen ordinary order plan')


def verify_plan(plan, layout, labels):
    require(plan['version'] == 1 and plan['layout'] == layout and plan['preview_unchanged'],
            'Native plan identity or query non-mutation failed')
    require(len(plan['search']) <= 8, 'More than eight endpoint searches in one layout')
    for search in plan['search']:
        require(search['cap'] == 30000 and 0 <= search['states'] <= 30000 and
                0 <= search['candidates'] <= 16, 'Endpoint search exceeded a binding shared budget')
        if search['exhausted']:
            raise CampaignStop('Native endpoint search exhausted its shared 30000-state/16-candidate budget')
    if not plan['legal']:
        return False
    require(plan['frozen'] and plan['total_quote'] == plan['construction_quote'] + plan['vehicle_quote'] and
            plan['total_quote'] >= 0, 'Complete legal frozen quote is inconsistent')
    require(len(plan.get('services', [])) == 4, 'Frozen quote does not contain all four material services')
    for service, label in zip(plan['services'], LABELS):
        require(service['label'] == label and service['cargo'] == labels[label] and 1 <= service['wagons'] <= 3,
                'Frozen service cargo/order/consist scope differs')
    require(plan['services'][1]['drop_tile'] == plan['services'][3]['drop_tile'] == 64278,
            'Frozen STEL/BALL routes do not use the actual Core receiver')
    for command in plan['commands']:
        if command['kind'] == 3:
            start, end = command['tile'], command['end']
            require(abs(start % 1024 - end % 1024) + abs(start // 1024 - end // 1024) <= 16,
                    'Frozen bridge exceeds the authorized 16-tile span')
    return True


def verify_native_accounting(before, result):
    audit_data = result['audit']
    after = result['state']
    entries = audit_data['cash_transactions']
    require(all(len(row) == 3 and all(isinstance(n, int) for n in row) for row in entries),
            'Malformed native transaction history')
    require(result['cash_conserved'] and not result['cargo_errors'] and
            sum(row[2] for row in entries) == audit_data['cash_debits'] and
            after['money'] == before['money'] - sum(row[2] for row in entries),
            'Native cash/cargo accounting or transaction reconciliation failed')
    # Check every cargo, including inherited services and invisible raw buffers.
    fields = ('produced', 'raw_produced', 'raw_removed', 'unallocated', 'discarded', 'consumed')
    require(len(before['all_held']) == len(after['all_held']) == CARGO_COUNT and
            all(len(audit_data[field]) == CARGO_COUNT for field in fields), 'Incomplete native all-cargo ledger')
    for cargo in range(CARGO_COUNT):
        expected = before['all_held'][cargo] + audit_data['produced'][cargo] + audit_data['raw_produced'][cargo]
        expected -= sum(audit_data[field][cargo] for field in ('raw_removed', 'unallocated', 'discarded', 'consumed'))
        require(expected == after['all_held'][cargo], f'Physical cargo {cargo} custody does not reconcile')


def verify_adapter_response_contract(value):
    """Reject missing native response fields before treating an operation as progress."""
    fields = ('version', 'operation', 'actual_ticks', 'state', 'observation', 'audit',
              'cash_conserved', 'cargo_errors', 'services', 'failed', 'reason')
    require(isinstance(value, dict) and all(key in value for key in fields),
            'Incomplete native basket response envelope')
    audit_fields = ('cash_transactions', 'cash_debits', 'produced', 'raw_produced',
                    'raw_removed', 'unallocated', 'discarded', 'consumed',
                    'deliveries', 'payments', 'cash_payments', 'arrivals',
                    'processor_cargo', 'basket_months')
    require(isinstance(value['audit'], dict) and all(key in value['audit'] for key in audit_fields) and
            isinstance(value['audit']['basket_months'], list), 'Incomplete native basket audit response')
    require(isinstance(value['state'], dict) and isinstance(value['observation'], dict) and
            isinstance(value['services'], list) and isinstance(value['cargo_errors'], list) and
            isinstance(value['actual_ticks'], int) and isinstance(value['failed'], bool),
            'Malformed native basket response types')


def paid_visits(audits, service, context=(), before_tick=None):
    """The existing functional runner's arrival/packet/cash proof, with tick ordering."""
    arrivals = sorted(row for data in (*context, *audits) for row in data['arrivals']
                      if row[1] == service['train'] and row[2] == service['drop'])
    delivered, booked = {}, {}
    for data in audits:
        for row in data['payments']:
            prior = [arrival[0] for arrival in arrivals if arrival[0] <= row[0]]
            if (row[1:4] == [service['train'], service['drop'], service['cargo']] and
                    row[4] > 0 and row[5] > 0 and prior and (before_tick is None or row[0] < before_tick)):
                delivered.setdefault(max(prior), []).append(row)
        for row in data['cash_payments']:
            prior = [arrival[0] for arrival in arrivals if arrival[0] <= row[0]]
            if (row[1:3] == [service['train'], service['drop']] and row[3] > 0 and prior and
                    (before_tick is None or row[0] < before_tick)):
                booked.setdefault(max(prior), []).append(row)
    return [{'arrival_tick': tick, 'packet_payments': delivered[tick], 'completed_native_cash': booked[tick]}
            for tick in sorted(delivered.keys() & booked.keys())]


def verify_month(event, labels):
    require(event['town'] == 0 and event['native_growth_observed'], 'Monthly event lacks the actual native Core hook')
    demand, before, after, consumed = (pairs(event[key]) for key in ('demand', 'before', 'after', 'consumed'))
    food, steel, ballast = (labels[label] for label in ('FOOD', 'STEL', 'BALL'))
    population = event['population']
    require(demand[food] == max(50, (population + 19) // 20) and
            demand[steel] == demand[ballast] == max(20, (population + 99) // 100),
            'Live monthly FOOD/STEL/BALL demand differs from native population')
    require(all(before.get(cargo, 0) - after.get(cargo, 0) == consumed.get(cargo, 0)
                for cargo in before.keys() | after.keys() | consumed.keys()),
            'Monthly event custody does not equal actual consumption')
    enough_food = before.get(food, 0) >= demand[food]
    complete = enough_food and all(before.get(cargo, 0) >= demand[cargo] for cargo in (steel, ballast))
    require(event['food_sufficient'] == enough_food and event['construction_complete'] == complete,
            'Monthly basket sufficiency flags disagree with captured native reserves')
    require(consumed.get(food, 0) == (demand[food] if enough_food else 0) and
            all(consumed.get(cargo, 0) == (demand[cargo] if complete else 0) for cargo in (steel, ballast)),
            'Monthly construction basket was not consumed atomically')
    require(event['passengers'] == (1 if enough_food else 0.5) or event['passengers'] == 1.5,
            'Native monthly passenger multiplier differs')
    if not complete:
        require(event['growth'] == 0 and not event['native_growth_enabled'],
                'Missing construction material enabled native expansion')
    return demand, before, after, consumed


def missing_ball_month(event, labels):
    demand, before, after, consumed = verify_month(event, labels)
    food, steel, ballast = (labels[label] for label in ('FOOD', 'STEL', 'BALL'))
    return (event['food_sufficient'] and before.get(steel, 0) >= demand[steel] and
            before.get(ballast, 0) < demand[ballast] and consumed.get(food, 0) == demand[food] and
            after.get(steel, 0) == before.get(steel, 0) and consumed.get(steel, 0) == 0 and
            consumed.get(ballast, 0) == 0 and event['growth'] == 0 and not event['native_growth_enabled'])


def complete_basket_month(event, labels):
    verify_month(event, labels)
    return (event['construction_complete'] and event['growth'] == event['passengers'] == 1 and
            event['native_growth_enabled'] and event['native_growth_hook_enabled'] and
            event['native_growth_rate'] != 65535)


def verify_conversions(audits, state, labels):
    result = {}
    for industry, incoming, outgoing, output_per_batch in ((7, 'IRON', 'STEL', 1), (4, 'SILC', 'BALL', 2)):
        rows = [row for data in audits for row in data['processor_cargo'] if row[1] == industry]
        used = -sum(row[3] for row in rows if row[2] == labels[incoming])
        made = sum(row[3] for row in rows if row[2] == labels[outgoing])
        batches = next(row[5] for row in state['economy']['factories'] if row[0] == industry)
        require(used == 2 * batches and made == output_per_batch * batches and
                all(row[2] in (labels[incoming], labels[outgoing]) and
                    (row[3] < 0 if row[2] == labels[incoming] else row[3] > 0) for row in rows),
                f'Industry {industry} exact physical recipe does not reconcile')
        result[outgoing] = {'industry': industry, 'input_label': incoming, 'input_used': used,
                            'output_produced': made, 'completed_batches': batches}
    return result


class DurableEngine(OfflineEngine):
    """Use the existing paused native Engine and preserve every dispatch/result."""
    def __init__(self, runner, folder, name, save):
        self.runner = runner
        with environment(profile_environment(folder)):
            super().__init__(runner.binary, folder / 'functional.cfg', folder, name,
                             save=save, deadline=runner.active_deadline)

    def command(self, command, marker):
        sequence = self.runner.dispatch(command, marker)
        self.deadline = self.runner.active_deadline
        try:
            response = super().command(command, marker)
            self.runner.receive(sequence, response)
            return response
        except BaseException as error:
            if command.startswith('connected_economy basket-') and not command.endswith('basket-status') and 'basket-plan ' not in command:
                self.runner.report['native_ledger_complete'] = False
            self.runner.journal('command_failed', sequence=sequence, error=str(error))
            self.runner.persist()
            raise


class Campaign:
    def __init__(self, binary, output):
        safe_console_path(output)
        self.binary, self.output = binary.resolve(), output.resolve()
        preparation = json.loads((output / 'preparation.json').read_text())
        self.report = json.loads((output / 'evidence.json').read_text())
        validate_execution_admission(self.report, preparation, pinned_inputs(self.binary), self.output)
        self.preparation = preparation
        self.frozen = json.loads((output / 'source/frozen-state.json').read_text())
        self.resume_state = json.loads((output / 'source/resume-state.json').read_text())
        self.resume_observation = json.loads((output / 'source/resume-observation.json').read_text())
        self.journal_stream = (output / 'operation-journal.jsonl').open('x')
        # Exclusive creation prevents a second launch even after a startup crash.
        write_json(output / 'execution-attempt.json', {'started_at_utc': utc_now().isoformat(),
                                                     'preparation_sha256': sha256(output / 'preparation.json')}, exclusive=True)
        self.sequence = 0
        self.engine = None
        self.current = None
        self.last_mutation_before = None
        self.services = self.report['services']
        self.frozen_plan = json.loads((output / 'source/resume-plan.json').read_text())
        self.built_footprint = json.loads((output / 'source/resume-footprint.json').read_text())
        self.labels = None
        self.phase = 'initial'
        preflight_started = utc_now()
        self.active_deadline = time.monotonic() + max(0, (datetime.fromisoformat(RECOVERY_INITIAL_STOP) - preflight_started).total_seconds())
        self.outer_deadline = time.monotonic() + max(0, (datetime.fromisoformat(RECOVERY_CLOCK['stop_utc']) - preflight_started).total_seconds())
        self.audits = {'initial': [], 'cold': []}
        self.report.update(status='PARTIAL', stage='preflight', preflight_start_utc=preflight_started.isoformat(),
                           preflight_stop_utc=RECOVERY_INITIAL_STOP, initial_stop_utc=RECOVERY_INITIAL_STOP,
                           prior_failed_attempts={'gross_debits': PRIOR_GROSS_DEBITS,
                                                  'requested_advances': PRIOR_REQUESTED_ADVANCES,
                                                  'actual_ticks': PRIOR_ACTUAL_TICKS})
        self.persist()

    def persist(self):
        state = {} if self.current is None else self.current.get('state', {})
        if not isinstance(state, dict):
            state = {}
        money = state.get('money') if isinstance(state.get('money'), int) else None
        tick = state.get('tick') if isinstance(state.get('tick'), int) else None
        new = self.report['advance_counts']
        self.report['new_advances_total'] = sum(new.values())
        self.report['aggregate_advances_total'] = HISTORICAL_ADVANCES['total'] + sum(new.values())
        self.report['requested_native_ticks'] = TICKS_PER_ADVANCE * sum(new.values())
        self.report['actual_native_ticks'] = (None if tick is None or not self.report['native_state_current'] else tick - START_TICK)
        self.report['aggregate_basket_actual_ticks'] = (None if tick is None or not self.report['native_state_current'] else
                                                        PRIOR_ACTUAL_TICKS + max(0, tick - CHECKPOINT_TICK))
        self.report['aggregate_gross_debits'] = HISTORICAL_PRIMARY_DEBITS + HISTORICAL_CONTROL_DEBITS + PRIOR_GROSS_DEBITS + self.report['native_debits']
        self.report['financial_reconciliation'] = {
            'starting_cash': START_CASH, 'new_native_receipts': self.report['native_receipts'],
            'new_native_debits': self.report['native_debits'], 'new_assistance': 0, 'debt': DEBT,
            'expected_ending_cash': START_CASH + self.report['native_receipts'] - self.report['native_debits'],
            'observed_cash': None if not self.report['native_state_current'] else money,
            'last_reported_cash': money,
            'ledger_complete': self.report['native_ledger_complete'],
        }
        self.report['whole_history_financial_reconciliation'] = {
            'original_starting_cash': 100000, 'historical_assistance': 6000000, 'new_assistance': 0,
            'historical_primary_receipts': 367827, 'total_primary_receipts': 367827 + self.report['native_receipts'],
            'historical_primary_debits': HISTORICAL_PRIMARY_DEBITS,
            'total_primary_debits': HISTORICAL_PRIMARY_DEBITS + self.report['native_debits'],
            'historical_disposable_control_debits': HISTORICAL_CONTROL_DEBITS,
            'aggregate_gross_debits': self.report['aggregate_gross_debits'],
            'expected_primary_cash': START_CASH + self.report['native_receipts'] - self.report['native_debits'],
        }
        write_json(self.output / 'evidence.json', self.report)

    def journal(self, kind, **values):
        self.journal_stream.write(json.dumps({'utc': utc_now().isoformat(), 'kind': kind, **values}, sort_keys=True) + '\n')
        self.journal_stream.flush()
        os.fsync(self.journal_stream.fileno())

    def check_wall(self):
        if time.monotonic() >= self.active_deadline:
            raise CampaignStop(f'{self.phase} native wall bound exhausted')
        if self.outer_deadline is not None:
            stop = datetime.fromisoformat(self.report['simulation_clock']['stop_utc'])
            if time.monotonic() >= self.outer_deadline or utc_now() >= stop:
                raise CampaignStop('Immutable first-advance plus 75-minute outer wall bound exhausted')

    def dispatch(self, command, marker):
        self.check_wall()
        require(authorized_command(command), 'A command outside the authorized basket proof was refused')
        if command.startswith(('connected_economy basket-stop ', 'connected_economy basket-restart ')):
            require(self.services is not None and int(command.rsplit(' ', 1)[1]) == self.services[3]['train'],
                    'Only the fixed BALL train may receive this proof control')
        self.sequence += 1
        self.journal('command_dispatch', sequence=self.sequence, phase=self.phase, command=command, marker=marker,
                     advance_counts=self.report['advance_counts'], native_debits=self.report['native_debits'])
        native_offset = None
        native_path = None
        if self.engine is not None:
            native_path = self.engine.console_path
            native_offset = native_path.stat().st_size
        self.report['last_command'] = {'sequence': self.sequence, 'command': command, 'status': 'dispatched',
                                       'native_log': None if native_path is None else str(native_path),
                                       'native_offset': native_offset}
        self.persist()
        return self.sequence

    def receive(self, sequence, response):
        path = self.output / 'operations' / f'{sequence:04d}-response.txt'
        with path.open('x') as stream:
            stream.write(response + '\n')
            stream.flush()
            os.fsync(stream.fileno())
        self.journal('command_response', sequence=sequence, path=str(path.relative_to(self.output)), sha256=sha256(path))
        self.report['last_command']['status'] = 'response_retained'
        self.persist()

    def json_command(self, operation, marker):
        response = self.engine.command('connected_economy ' + operation, marker)
        value = json.loads(response)
        path = self.output / 'operations' / f'{self.sequence:04d}-result.json'
        write_json(path, value, exclusive=True)
        self.report['operations'].append({'sequence': self.sequence, 'operation': operation,
                                          'result': str(path.relative_to(self.output)), 'sha256': sha256(path)})
        self.persist()
        return value

    def status(self, expected_state=None, expected_observation=None):
        value = self.json_command('basket-status', 'CONNECTED basket-state ')
        if expected_state is not None:
            exact_paused_equality(value, expected_state, expected_observation)
        verify_observation(value['state'], value['observation'], self.frozen, self.services,
                           self.frozen_plan, self.built_footprint)
        self.current = value
        self.last_mutation_before = None
        self.report['native_state_current'] = True
        if expected_state is not None:
            require(value['state']['money'] == START_CASH + self.report['native_receipts'] - self.report['native_debits'],
                    'Paused load/status cash differs from the retained native ledger')
            self.report['native_ledger_complete'] = True
        self.labels = value['observation']['cargo_labels']
        self.report['last_native_state'] = self.report['operations'][-1]['result']
        self.persist()
        self.check_wall()
        return value

    def mutate(self, operation):
        self.check_wall()
        before = self.current['state']
        self.last_mutation_before = before
        self.report['native_ledger_complete'] = False
        self.report['native_state_current'] = False
        self.persist()
        value = self.json_command(operation, 'CONNECTED basket-result ')
        verify_adapter_response_contract(value)
        require(value['version'] == 1 and value['operation'] == operation.split()[0],
                'Unsupported basket result protocol or operation identity')
        entries = value['audit']['cash_transactions']
        # Retain all charges and partial native results before checking any claim.
        self.report['native_transactions'].extend(entries)
        self.report['native_debits'] += sum(max(row[2], 0) for row in entries)
        self.report['native_receipts'] += sum(max(-row[2], 0) for row in entries)
        self.report['accounted_sequences'].append(self.sequence)
        self.audits['cold' if self.phase == 'cold' else 'initial'].append(value['audit'])
        self.current = value
        self.report['native_state_current'] = True
        self.report['last_native_state'] = self.report['operations'][-1]['result']
        self.persist()
        verify_native_accounting(before, value)
        self.report['native_ledger_complete'] = True
        self.persist()
        require(not value['failed'], f'{operation}: native paid command or integrity guard failed: ' + value.get('reason', 'unknown native reason'))
        require(value['state']['money'] == START_CASH + self.report['native_receipts'] - self.report['native_debits'],
                'Cumulative retained-checkpoint cash equation failed')
        require(value['observation']['adapter']['gross_debits'] == self.report['native_debits'],
                'Native and wrapper cumulative gross debits differ')
        verify_observation(value['state'], value['observation'], self.frozen, self.services,
                           self.frozen_plan, self.built_footprint)
        require(value['state']['tick'] - before['tick'] == (TICKS_PER_ADVANCE if operation == 'basket-advance' else 0),
                'A paused command ran background ticks or an advance differs from 2048 ticks')
        for event in value['audit']['basket_months']:
            if event['town'] == 0:
                verify_month(event, self.labels)
        verify_conversions(self.audits['initial'] + self.audits['cold'], value['state'], self.labels)
        self.check_wall()
        return value

    def diagnostic_checkpoint_if_safe(self, error):
        """Save only a verified paused result after a wrapper assertion, never native corruption."""
        if isinstance(error, CampaignStop) or self.engine is None or self.last_mutation_before is None:
            return
        try:
            value = self.current
            require(isinstance(value, dict) and not value.get('failed') and
                    value.get('observation', {}).get('paused'), 'Native result is not a safe paused state')
            verify_adapter_response_contract(value)
            verify_native_accounting(self.last_mutation_before, value)
            verify_observation(value['state'], value['observation'], self.frozen,
                               self.services, self.frozen_plan, self.built_footprint)
            self.save_paused(self.output / 'diagnostic', 'wrapper-stop')
            self.report['diagnostic_checkpoint'] = 'SAVED; diagnostic only, not an authorized continuation source'
        except Exception as diagnostic_error:
            self.report['diagnostic_checkpoint'] = 'NOT SAVED; safety check: ' + str(diagnostic_error)
        self.persist()

    def start(self, name, save):
        self.check_wall()
        require(self.report['load_attempts'][name] == 0, 'A second native process attempt is forbidden')
        self.report['load_attempts'][name] = 1
        self.report['stage'] = f'{name}_load_attempted'
        self.report['native_state_current'] = False
        self.report['native_ledger_complete'] = False
        self.persist()
        self.journal('native_process_attempt', phase=name, binary=str(self.binary), save=str(save), save_sha256=sha256(save))
        self.engine = DurableEngine(self, self.output / name, name, save)
        self.check_wall()

    def close(self):
        if self.engine is not None:
            self.journal('native_process_close', phase=self.phase)
            engine = self.engine
            self.engine = None
            try:
                engine.close()
            except BaseException as error:
                self.journal('native_close_failed', error=str(error))
                self.report.setdefault('cleanup_errors', []).append(str(error))
            code = engine.process.poll()
            self.report.setdefault('native_exit_codes', {})[self.phase] = code
            if code != 0 and self.report['stage'] != 'stopped':
                self.report.update(status='FAIL', stage='stopped', error=f'Native process closed with exit code {code}',
                                   error_type='NativeProcessExit')

    def recover_flushed_failed_result(self):
        """Inspect already-written logs only; never issue diagnosis after a defect."""
        if self.report['native_ledger_complete']:
            return
        command = self.report.get('last_command', {})
        if command.get('sequence') in self.report['accounted_sequences']:
            return
        native_path, offset = command.get('native_log'), command.get('native_offset')
        if native_path is None or offset is None:
            return
        path = Path(native_path)
        if not path.is_file():
            return
        with path.open('rb') as stream:
            stream.seek(offset)
            tail = stream.read().decode(errors='replace')
        marker = 'CONNECTED basket-result '
        candidates = [line.split(marker, 1)[1] for line in tail.splitlines() if marker in console_payload(line)]
        if len(candidates) != 1:
            return
        try:
            value = json.loads(candidates[0])
            audit_data = value['audit']
            entries = audit_data['cash_transactions']
            require(value['version'] == 1 and value['operation'] == command['command'].split()[1],
                    'Flushed partial result has the wrong operation identity')
            require(all(len(row) == 3 and all(isinstance(n, int) for n in row) for row in entries),
                    'Flushed partial result has malformed native cash entries')
        except (KeyError, ValueError, RuntimeError):
            return
        retained = self.output / 'operations' / f'{command["sequence"]:04d}-failed-flushed-result.json'
        write_json(retained, value, exclusive=True)
        self.report['native_transactions'].extend(entries)
        self.report['native_debits'] += sum(max(row[2], 0) for row in entries)
        self.report['native_receipts'] += sum(max(-row[2], 0) for row in entries)
        self.report['accounted_sequences'].append(command['sequence'])
        self.report['flushed_failed_result'] = {'path': str(retained.relative_to(self.output)), 'sha256': sha256(retained)}
        before = None if self.current is None else self.current['state']
        self.current = value
        self.report['native_state_current'] = True
        try:
            require(before is not None, 'No confirmed pre-command state')
            verify_native_accounting(before, value)
            self.report['native_ledger_complete'] = True
        except (KeyError, RuntimeError) as error:
            self.report['partial_accounting_error'] = str(error)
        self.persist()

    def start_simulation_clock(self):
        require(self.report['simulation_clock'] is None and self.outer_deadline is None and
                self.report['advance_counts'] == {'initial': 0, 'cold': 0}, 'Simulation clock cannot be reset')
        started = utc_now()
        monotonic = time.monotonic()
        clock = {'start_utc': started.isoformat(), 'stop_utc': (started + timedelta(seconds=OUTER_SECONDS)).isoformat(),
                 'trigger': 'Immediately before the first native basket-advance dispatch',
                 'duration_seconds': OUTER_SECONDS}
        write_json(self.output / 'simulation-clock.json', clock, exclusive=True)
        self.report['simulation_clock'] = clock
        self.outer_deadline = monotonic + OUTER_SECONDS
        self.phase = 'initial'
        self.active_deadline = min(self.outer_deadline, monotonic + PHASE_SECONDS['initial'])
        self.report['initial_start_utc'] = clock['start_utc']
        self.report['preflight_end_utc'] = clock['start_utc']
        self.report['initial_stop_utc'] = (started + timedelta(seconds=PHASE_SECONDS['initial'])).isoformat()
        self.persist()
        self.journal('simulation_clock_started', **clock)

    def advance(self):
        self.check_wall()
        require(self.report['simulation_clock'] == RECOVERY_CLOCK, 'Retained simulation clock changed')
        if self.report['advance_counts'][self.phase] >= ADVANCE_CAPS[self.phase]:
            raise CampaignStop(f'{self.phase} phase advance ceiling exhausted; unused phase ticks cannot transfer')
        if sum(self.report['advance_counts'].values()) >= 360:
            raise CampaignStop('Aggregate advance bound exhausted')
        self.report['advance_counts'][self.phase] += 1
        self.report['stage'] = f'{self.phase}_operation'
        self.persist()  # Requested advances count even if dispatch/result fails.
        value = self.mutate('basket-advance')
        self.save_paused(self.output / 'checkpoints', f'advance-{sum(self.report["advance_counts"].values()):03d}')
        return value

    def months(self, value):
        return [event for event in value['audit']['basket_months'] if event['town'] == 0]

    def save_paused(self, folder, stem, cold_envelope=False):
        self.check_wall()
        state, observation = self.current['state'], self.current['observation']
        write_json(folder / f'{stem}-state.json', state, exclusive=True)
        write_json(folder / f'{stem}-observation.json', observation_projection(observation), exclusive=True)
        if cold_envelope:
            write_json(folder / 'cold-arm.json', {'state': state, 'observation': observation_projection(observation), 'basket': {
                'source_sha256': SOURCE_SHA256, 'initial_advances': self.report['advance_counts']['initial'],
                'gross_debits': self.report['native_debits'], 'services': self.services,
                'quoted_total': self.report['selected_quote']}}, exclusive=True)
        path = folder / f'{stem}.sav'
        require(not path.exists(), 'An existing save must never be replaced')
        self.engine.save(path)
        require(path.is_file() and path.stat().st_size > 0, 'Native synchronous save was not written')
        save_hash = sha256(path)
        self.report.setdefault('saves', {})[str(path.relative_to(self.output))] = save_hash
        self.persist()
        self.status(state, observation)
        require(sha256(path) == save_hash, 'Saved file hash changed after synchronous native save')
        return path

    def terrain_audit(self, key):
        before = self.current
        result = audit(self.engine)  # Existing raw native command parser/checks.
        path = self.output / 'operations' / f'{self.sequence:04d}-terrain-audit.json'
        write_json(path, result, exclusive=True)
        self.report[key] = {'result': str(path.relative_to(self.output)), 'sha256': sha256(path)}
        self.persist()
        self.status(before['state'], before['observation'])

    def choose_plan(self):
        before = self.current
        for layout in range(2):
            value = self.json_command(f'basket-plan {layout}', 'CONNECTED basket-plan ')
            eligible = verify_plan(value, layout, self.labels)
            if value.get('failed', False):
                require(value.get('preview_unchanged', False), 'Failed native plan changed persisted state')
                require(observation_projection(value['observation']) == observation_projection(before['observation']),
                        'Failed native quote changed the extended paused observation')
                raise CampaignStop('Native complete material quote failed: ' + value.get('reason', 'unknown native reason'))
            # Adapter search counters may change; every persisted native field must not.
            self.status(before['state'], before['observation'])
            if eligible:
                write_json(self.output / 'frozen-plan.json', value, exclusive=True)
                self.report['selected_layout'] = layout
                self.frozen_plan = value
                self.report['selected_quote'] = value['total_quote']
                self.report['frozen_plan_sha256'] = sha256(self.output / 'frozen-plan.json')
                self.report['phases']['plan'] = 'PASS'
                self.persist()
                return layout
        raise CampaignStop('No complete legal frozen material layout within two stable candidates')

    def material_proof(self, audits, context=(), before_tick=None):
        visits = {service['label']: paid_visits(audits, service, context, before_tick) for service in self.services}
        conversions = verify_conversions(self.audits['initial'] + self.audits['cold'], self.current['state'], self.labels)
        return visits, conversions

    def run(self):
        try:
            if self.preparation.get('resume_checkpoint_sha256') != CHECKPOINT_SHA256:
                raise CampaignStop('Pinned checkpoint continuation metadata differs')
            self.start('initial', self.output / 'source/resume.sav')
            self.status(self.resume_state, self.resume_observation)
            self.report['phases']['admission'] = 'PASS; exact paused checkpoint and built network'
            self.persist()
            self.terrain_audit('resumed_terrain_cargo_text_audit')
            self.mutate('basket-arm ' + safe_console_path(self.output / 'source/resume-arm.json'))
            exact_paused_equality(self.current, self.resume_state, self.resume_observation)
            require(next(row for row in self.current['state']['trains'] if row['id'] == self.services[3]['train'])['stopped'],
                    'Retained missing-BALL control train is not stopped')
            while True:
                value = self.advance()
                controls = [event for event in self.months(value) if missing_ball_month(event, self.labels)]
                if controls:
                    self.report['missing_ball_control'] = controls[0]
                    self.report['phases']['missing_ball'] = 'PASS'
                    self.persist()
                    break
            self.mutate(f'basket-restart {self.services[3]["train"]}')
            require(not next(row for row in self.current['state']['trains'] if row['id'] == self.services[3]['train'])['stopped'],
                    'The normal BALL restart control did not release its train')
            initial_event = None
            while initial_event is None:
                value = self.advance()
                for event in self.months(value):
                    if not complete_basket_month(event, self.labels):
                        continue
                    visits, conversions = self.material_proof(self.audits['initial'], before_tick=event['tick'])
                    live = next(row for row in value['observation']['towns'] if row['id'] == 0)
                    if (all(visits[label] for label in LABELS) and all(row['completed_batches'] > 0 for row in conversions.values()) and
                            live['growth'] == live['passengers'] == 1 and live['native_growth_enabled'] and live['native_growth_hook_enabled']):
                        initial_event = event
                        self.report['initial_material_visits'] = visits
                        self.report['exact_conversions'] = conversions
                        break
            self.report['initial_complete_basket'] = initial_event
            self.report['phases']['initial_basket'] = 'PASS'
            self.persist()
            checkpoint = self.save_paused(self.output / 'initial', 'full-basket', cold_envelope=True)
            retained = self.current
            self.close()
            require(self.report['stage'] != 'stopped', self.report.get('error', 'Initial native process failed on close'))
            self.check_wall()  # Initial stopped/close time cannot spill into the cold allowance.
            self.phase = 'cold'
            started = utc_now()
            self.active_deadline = min(self.outer_deadline, time.monotonic() + PHASE_SECONDS['cold'])
            self.report['cold_start_utc'] = started.isoformat()
            self.report['cold_stop_utc'] = min(started + timedelta(seconds=PHASE_SECONDS['cold']),
                                                datetime.fromisoformat(self.report['simulation_clock']['stop_utc'])).isoformat()
            self.persist()
            require(pinned_inputs(self.binary) == self.preparation['inputs'], 'Frozen inputs changed before the one cold load')
            self.start('cold', checkpoint)
            self.status(retained['state'], retained['observation'])
            self.report['phases']['cold_equality'] = 'PASS'
            self.persist()
            self.mutate('basket-arm ' + safe_console_path(self.output / 'initial/cold-arm.json'))
            exact_paused_equality(self.current, retained['state'], retained['observation'])
            cold_event = None
            while cold_event is None:
                value = self.advance()
                for event in self.months(value):
                    if event['tick'] <= initial_event['tick'] or not complete_basket_month(event, self.labels):
                        continue
                    visits, conversions = self.material_proof(self.audits['cold'], self.audits['initial'], event['tick'])
                    live = next(row for row in value['observation']['towns'] if row['id'] == 0)
                    if (visits['STEL'] and visits['BALL'] and live['growth'] == live['passengers'] == 1 and
                            live['native_growth_enabled'] and live['native_growth_hook_enabled']):
                        cold_event = event
                        self.report['cold_material_visits'] = {label: visits[label] for label in ('STEL', 'BALL')}
                        self.report['exact_conversions'] = conversions
                        break
            self.report['cold_complete_basket'] = cold_event
            self.report['phases']['cold_basket'] = 'PASS'
            self.persist()
            self.terrain_audit('final_terrain_cargo_text_audit')
            self.report['phases']['final_audit'] = 'PASS'
            final_save = self.save_paused(self.output / 'cold', 'owner-observation')
            self.report['final_save'] = str(final_save.relative_to(self.output))
            self.report['final_save_sha256'] = sha256(final_save)
            self.report['final_town_observation'] = next(row for row in self.current['observation']['towns'] if row['id'] == 0)
            self.report['phases']['final_save'] = 'PASS'
            require(pinned_inputs(self.binary) == self.preparation['inputs'], 'Frozen inputs changed during the native campaign')
            self.check_wall()
            self.report.update(status='PASS', stage='completed')
        except BaseException as error:
            self.report.update(status='PARTIAL' if isinstance(error, CampaignStop) else 'FAIL', stage='stopped',
                               error=str(error), error_type=type(error).__name__)
            self.journal('campaign_stopped', error=str(error), error_type=type(error).__name__)
            self.diagnostic_checkpoint_if_safe(error)
        finally:
            self.close()
            self.recover_flushed_failed_result()
            self.finish_wall_check()
            self.report['finished_at_utc'] = utc_now().isoformat()
            self.persist()
            self.journal_stream.close()
            artifacts = {str(path.relative_to(self.output)): {'sha256': sha256(path), 'bytes': path.stat().st_size}
                         for path in self.output.rglob('*') if path.is_file() and not path.is_symlink() and
                         path.name not in ('evidence.json', 'artifacts.json', 'evidence.sha256')}
            write_json(self.output / 'artifacts.json', artifacts, exclusive=True)
            self.report['artifact_manifest_sha256'] = sha256(self.output / 'artifacts.json')
            self.finish_wall_check()
            self.report['finished_at_utc'] = utc_now().isoformat()
            self.persist()
            write_text_exclusive(self.output / 'evidence.sha256', sha256(self.output / 'evidence.json') + '  evidence.json\n')
        print(f'{self.report["status"]}: Core construction basket; '
              f'new advances {self.report["advance_counts"]}, gross debits GBP{self.report["native_debits"]}; '
              f'human UAT Pending, ordinary economics/fresh replay NOT RUN. Evidence: {self.output / "evidence.json"}', flush=True)
        if 'error' in self.report:
            print('Stopped at first blocker: ' + self.report['error'], flush=True)
        return 0 if self.report['status'] == 'PASS' else 1

    def finish_wall_check(self):
        if self.report['status'] == 'PASS':
            try:
                self.check_wall()
            except CampaignStop as error:
                self.report.update(status='PARTIAL', stage='stopped', error=str(error), error_type=type(error).__name__)


class AdmissionGuardTests(unittest.TestCase):
    """No native process, advance, grant, cargo injection or disposable replay."""
    @classmethod
    def setUpClass(cls):
        _, cls.frozen, _, _, _ = source_payload()

    def test_all_response_fields_are_required_before_accounting(self):
        audit = {key: [] for key in ('cash_transactions', 'produced', 'raw_produced',
                 'raw_removed', 'unallocated', 'discarded', 'consumed', 'deliveries',
                 'payments', 'cash_payments', 'arrivals', 'processor_cargo', 'basket_months')}
        audit['cash_debits'] = 0
        response = {'version': 1, 'operation': 'basket-dev-money', 'actual_ticks': 0,
                    'state': {}, 'observation': {}, 'audit': audit, 'cash_conserved': True,
                    'cargo_errors': [], 'services': [], 'failed': False, 'reason': ''}
        verify_adapter_response_contract(response)
        for key in tuple(response):
            broken = copy.deepcopy(response)
            del broken[key]
            with self.subTest(envelope=key), self.assertRaisesRegex(RuntimeError, 'Incomplete native basket response'):
                verify_adapter_response_contract(broken)
        for key in tuple(audit):
            broken = copy.deepcopy(response)
            del broken['audit'][key]
            with self.subTest(audit=key), self.assertRaisesRegex(RuntimeError, 'Incomplete native basket audit'):
                verify_adapter_response_contract(broken)

    def test_unbuilt_neutral_tree_growth_preserves_rail_integrity_guard(self):
        before = {'rails': [{'tile': 10, 'type': 0, 'owner': 16, 'world': 2, 'height': 3,
                             'slope': 0, 'expected_tracks': 1}]}
        tree = copy.deepcopy(before)
        tree['rails'][0]['type'] = 4
        self.assertEqual(normalize_unbuilt_vegetation(before, tree), before)
        for field, value in (('owner', 0), ('height', 4), ('type', 6)):
            changed = copy.deepcopy(tree)
            changed['rails'][0][field] = value
            self.assertNotEqual(normalize_unbuilt_vegetation(before, changed), before)
        rail = copy.deepcopy(before)
        rail['rails'][0].update(type=1, tracks=1, railtype=0)
        removed = copy.deepcopy(rail)
        removed['rails'][0].update(type=4)
        self.assertNotEqual(normalize_unbuilt_vegetation(rail, removed), rail)

    def test_unused_zone_vegetation_does_not_replace_route_checks(self):
        state = copy.deepcopy(self.frozen)
        state['zones'][6]['rails'][10]['type'] = 4
        verify_protected_topology(state, self.frozen)
        state['zones'][6]['head']['owner'] = 0
        with self.assertRaisesRegex(RuntimeError, 'zone identity'):
            verify_protected_topology(state, self.frozen)

    def test_world_development_can_progress_without_changing_role_or_phase(self):
        state = copy.deepcopy(self.frozen)
        state['worlds'][1]['development'] += 81
        verify_protected_topology(state, self.frozen)
        state['worlds'][1]['phase'] += 1
        with self.assertRaisesRegex(RuntimeError, 'world identity'):
            verify_protected_topology(state, self.frozen)

    def test_full_snapshot_rejects_unrelated_rng_difference(self):
        value = {'version': 1, 'state': copy.deepcopy(self.frozen), 'observation': {'paused': True}}
        value['state']['rng'][0] += 1
        with self.assertRaisesRegex(RuntimeError, 'Full paused'):
            exact_paused_equality(value, self.frozen)

    def test_full_snapshot_requires_paused(self):
        with self.assertRaisesRegex(RuntimeError, 'not paused'):
            exact_paused_equality({'version': 1, 'state': self.frozen, 'observation': {'paused': False}}, self.frozen)

    def test_extended_cold_equality_retains_growth_and_custody(self):
        observation = {'paused': True, 'custody': {'total': [1]}, 'native_growth_rate': 37,
                       'private_rail': [{'reservation': 8, 'signal_state': 7}],
                       'private_structures': [{'reservation': True}], 'adapter': {'cold': False}}
        changed = copy.deepcopy(observation)
        changed['adapter']['cold'] = True
        exact_paused_equality({'version': 1, 'state': self.frozen, 'observation': changed}, self.frozen, observation)
        changed['native_growth_rate'] = 38
        with self.assertRaisesRegex(RuntimeError, 'extended'):
            exact_paused_equality({'version': 1, 'state': self.frozen, 'observation': changed}, self.frozen, observation)
        changed = copy.deepcopy(observation)
        changed['private_rail'][0]['reservation'] = 0
        with self.assertRaisesRegex(RuntimeError, 'extended'):
            exact_paused_equality({'version': 1, 'state': self.frozen, 'observation': changed}, self.frozen, observation)

    def test_no_second_attempt_or_transferred_clock(self):
        report = initial_report({})
        report['load_attempts']['initial'] = 1
        with self.assertRaisesRegex(RuntimeError, 'already attempted'):
            validate_execution_admission(report, {'version': 1, 'inputs': {}}, {}, ROOT)

    def test_closed_commands_exclude_funding_generation_and_research(self):
        for command in ('connected_economy functional-grant', 'loan 200000', 'newgame',
                        'connected_economy functional-build 0 1 2 3 4', 'research 302',
                        'connected_economy basket-advance 4096', 'connected_economy basket-plan 2'):
            self.assertFalse(authorized_command(command), command)
        for command in ('connected_economy basket-plan 0', 'connected_economy basket-advance',
                        'connected_economy basket-arm "/tmp/frozen.json"', 'save "/tmp/proven"'):
            self.assertTrue(authorized_command(command), command)

    def test_prepared_inputs_must_remain_exact(self):
        report = initial_report({'binary': 'old'})
        with self.assertRaisesRegex(RuntimeError, 'inputs changed'):
            validate_execution_admission(report, {'version': 1, 'inputs': {'binary': 'old'}}, {'binary': 'new'}, ROOT)

    def test_all_cargo_conservation_detects_hidden_nonbasket_loss(self):
        before = {'money': 100, 'all_held': [0] * CARGO_COUNT}
        result = {'state': copy.deepcopy(before), 'cash_conserved': True, 'cargo_errors': [],
                  'audit': {name: [0] * CARGO_COUNT for name in
                            ('produced', 'raw_produced', 'raw_removed', 'unallocated', 'discarded', 'consumed')}}
        result['audit'].update(cash_transactions=[], cash_debits=0)
        verify_native_accounting(before, result)
        result['state']['all_held'][63] = 1
        with self.assertRaisesRegex(RuntimeError, 'cargo 63'):
            verify_native_accounting(before, result)

    def test_native_cash_requires_every_elapsed_charge(self):
        before = {'money': 100, 'all_held': [0] * CARGO_COUNT}
        result = {'state': {'money': 98, 'all_held': [0] * CARGO_COUNT}, 'cash_conserved': True, 'cargo_errors': [],
                  'audit': {name: [0] * CARGO_COUNT for name in
                            ('produced', 'raw_produced', 'raw_removed', 'unallocated', 'discarded', 'consumed')}}
        result['audit'].update(cash_transactions=[[1, 3, 1]], cash_debits=1)
        with self.assertRaisesRegex(RuntimeError, 'accounting'):
            verify_native_accounting(before, result)

    def event(self, ball=0, food=120):
        enough_food = food >= 60
        complete = enough_food and ball >= 20
        return {'tick': 169000, 'date': 713900, 'town': 0, 'population': 1191, 'house_count': 29,
                'demand': [[9, 20], [30, 20], [33, 60]], 'before': [[9, 20], [30, ball], [33, food]],
                'after': [[9, 0 if complete else 20], [30, ball - 20 if complete else ball],
                          [33, food - 60 if enough_food else food]],
                'consumed': ([[9, 20], [30, 20]] if complete else []) + ([[33, 60]] if enough_food else []),
                'food_sufficient': enough_food, 'construction_complete': complete,
                'growth': 1 if complete else 0, 'passengers': 1 if enough_food else 0.5,
                'native_growth_observed': True, 'native_growth_enabled': complete,
                'native_growth_hook_enabled': complete, 'native_growth_rate': 37 if complete else 65535}

    def test_missing_ball_control_requires_food_and_steel(self):
        labels = {'FOOD': 33, 'STEL': 9, 'BALL': 30}
        self.assertTrue(missing_ball_month(self.event(), labels))
        self.assertFalse(missing_ball_month(self.event(food=0), labels))

    def test_atomicity_rejects_steel_consumption_without_ball(self):
        event = self.event()
        event['consumed'].append([9, 20])
        event['after'][0][1] = 0
        with self.assertRaisesRegex(RuntimeError, 'atomically'):
            verify_month(event, {'FOOD': 33, 'STEL': 9, 'BALL': 30})

    def test_native_growth_hook_and_actual_month_are_required(self):
        event = self.event(ball=20)
        labels = {'FOOD': 33, 'STEL': 9, 'BALL': 30}
        self.assertTrue(complete_basket_month(event, labels))
        event['native_growth_hook_enabled'] = False
        self.assertFalse(complete_basket_month(event, labels))
        event['native_growth_observed'] = False
        with self.assertRaisesRegex(RuntimeError, 'actual native'):
            complete_basket_month(event, labels)

    def test_demand_is_recomputed_from_month_population(self):
        event = self.event()
        event['population'] = 1201
        with self.assertRaisesRegex(RuntimeError, 'population'):
            verify_month(event, {'FOOD': 33, 'STEL': 9, 'BALL': 30})

    def test_paid_visit_requires_native_cash_before_distinct_event(self):
        service = {'train': 12, 'drop': 3, 'cargo': 30}
        data = {'arrivals': [[1, 12, 3]], 'payments': [[2, 12, 3, 30, 20, 100]], 'cash_payments': [[4, 12, 3, 100]]}
        self.assertFalse(paid_visits([data], service, before_tick=4))
        self.assertTrue(paid_visits([data], service, before_tick=5))
        data['cash_payments'] = []
        self.assertFalse(paid_visits([data], service))

    def test_shared_endpoint_cap_exhaustion_stops(self):
        plan = {'version': 1, 'layout': 0, 'preview_unchanged': True,
                'search': [{'cap': 30000, 'states': 30000, 'candidates': 16, 'exhausted': True}], 'legal': False}
        with self.assertRaises(CampaignStop):
            verify_plan(plan, 0, {})
        plan['search'][0].update(exhausted=False, states=30001)
        with self.assertRaisesRegex(RuntimeError, 'shared budget'):
            verify_plan(plan, 0, {})

    def test_recipes_require_exact_physical_batches(self):
        labels = {'IRON': 17, 'STEL': 9, 'SILC': 16, 'BALL': 30}
        rows = [[1, 7, 17, -4], [1, 7, 9, 2], [1, 4, 16, -4], [1, 4, 30, 4]]
        state = {'economy': {'factories': [[7, 16, 100, 0, [], 2], [4, 16, 100, 0, [], 2]]}}
        self.assertEqual(verify_conversions([{'processor_cargo': rows}], state, labels)['BALL']['output_produced'], 4)
        rows[-1][-1] = 5
        with self.assertRaisesRegex(RuntimeError, 'exact physical recipe'):
            verify_conversions([{'processor_cargo': rows}], state, labels)

    def test_legitimate_stellar_admission_is_dynamic_but_policy_is_protected(self):
        state = copy.deepcopy(self.frozen)
        state['stellar']['admitted'] = [[0, 54004]]
        verify_protected_topology(state, self.frozen)
        state['stellar']['policies'][0][3] += 1
        with self.assertRaisesRegex(RuntimeError, 'Static stellar'):
            verify_protected_topology(state, self.frozen)

    def test_stellar_admission_cannot_name_a_wagon_or_other_tile(self):
        state = copy.deepcopy(self.frozen)
        state['stellar']['admitted'] = [[3, 54004]]
        with self.assertRaisesRegex(RuntimeError, 'invalid front'):
            verify_protected_topology(state, self.frozen)
        state['stellar']['admitted'] = [[0, 68378]]
        with self.assertRaisesRegex(RuntimeError, 'invalid front'):
            verify_protected_topology(state, self.frozen)

    def test_private_join_requires_declared_paid_track_and_preserves_old_bits(self):
        state = copy.deepcopy(self.frozen)
        join = state['terminals'][0]['join']
        old_bits = join['tracks']
        join['tracks'] |= 1
        plan = {'commands': [{'kind': 2, 'tile': join['tile'], 'track': 0}]}
        verify_protected_topology(state, self.frozen, plan)
        with self.assertRaisesRegex(RuntimeError, 'declared paid footprint'):
            verify_protected_topology(state, self.frozen)
        join['tracks'] = 1
        self.assertNotEqual(old_bits, 0)
        with self.assertRaisesRegex(RuntimeError, 'declared paid footprint'):
            verify_protected_topology(state, self.frozen, plan)

    def test_neutral_head_protected_and_signal_state_can_change(self):
        state = copy.deepcopy(self.frozen)
        state['terminals'][0]['signals'][0]['state_bits'] = 0
        verify_protected_topology(state, self.frozen)
        state['terminals'][0]['head']['owner'] = 0
        with self.assertRaisesRegex(RuntimeError, 'Neutral terminal'):
            verify_protected_topology(state, self.frozen)

    def test_source_cold_comparison_never_normalizes_live_admission_or_signal(self):
        state = copy.deepcopy(self.frozen)
        state['stellar']['admitted'] = [[0, 54004]]
        with self.assertRaisesRegex(RuntimeError, 'Full paused'):
            exact_paused_equality({'version': 1, 'state': state, 'observation': {'paused': True}}, self.frozen)

    def bounded_campaign(self, initial, cold, phase):
        campaign = object.__new__(Campaign)
        campaign.report = initial_report({})
        campaign.report['simulation_clock'] = copy.deepcopy(RECOVERY_CLOCK)
        campaign.report['advance_counts'] = {'initial': initial, 'cold': cold}
        campaign.phase = phase
        campaign.output = Path('/tmp/unused-basket-test-output')
        campaign.check_wall = lambda: None
        campaign.persist = lambda: None
        campaign.mutate = lambda operation: operation
        campaign.save_paused = lambda folder, stem: None
        return campaign

    def test_phase_advance_caps_cannot_transfer_unused_ticks(self):
        for initial, cold, phase in ((240, 0, 'initial'), (1, 120, 'cold')):
            campaign = self.bounded_campaign(initial, cold, phase)
            with self.assertRaisesRegex(CampaignStop, 'cannot transfer'):
                campaign.advance()
            self.assertEqual(campaign.report['advance_counts'], {'initial': initial, 'cold': cold})
        campaign = self.bounded_campaign(240, 119, 'cold')
        self.assertEqual(campaign.advance(), 'basket-advance')
        self.assertEqual(campaign.report['advance_counts'], {'initial': 240, 'cold': 120})
        with self.assertRaises(CampaignStop):
            campaign.advance()

    def test_first_advance_clock_cannot_be_reset(self):
        campaign = self.bounded_campaign(1, 0, 'initial')
        campaign.outer_deadline = 1
        with self.assertRaisesRegex(RuntimeError, 'cannot be reset'):
            campaign.start_simulation_clock()

    def test_failed_paid_result_keeps_actual_cash_entries_before_stopping(self):
        campaign = self.bounded_campaign(0, 0, 'preflight')
        campaign.current = {'state': {'money': START_CASH, 'all_held': [0] * CARGO_COUNT}}
        campaign.sequence = 7
        campaign.report['operations'] = [{'result': 'partial-native-result.json'}]
        campaign.audits = {'initial': [], 'cold': []}
        result = {'version': 1, 'operation': 'basket-build', 'failed': True,
                  'reason': 'native-command-failed', 'actual_ticks': 0,
                  'observation': {}, 'services': [],
                  'state': {'money': START_CASH - 17, 'all_held': [0] * CARGO_COUNT},
                  'cash_conserved': True, 'cargo_errors': [],
                  'audit': {name: [0] * CARGO_COUNT for name in
                            ('produced', 'raw_produced', 'raw_removed', 'unallocated', 'discarded', 'consumed')}}
        result['audit'].update(cash_transactions=[[START_TICK, 0, 17]], cash_debits=17)
        result['audit'].update({key: [] for key in ('deliveries', 'payments', 'cash_payments',
                               'arrivals', 'processor_cargo', 'basket_months')})
        dispatched = []
        def command(operation, marker):
            dispatched.append(operation)
            return result
        campaign.json_command = command
        with self.assertRaisesRegex(RuntimeError, 'native paid command'):
            Campaign.mutate(campaign, 'basket-build 0')
        self.assertEqual(dispatched, ['basket-build 0'])
        self.assertEqual(campaign.report['native_debits'], CHECKPOINT_DEBITS + 17)
        self.assertEqual(campaign.report['native_transactions'], [[START_TICK, 0, 17]])
        self.assertEqual(campaign.current['state']['money'], START_CASH - 17)
        self.assertTrue(campaign.report['native_ledger_complete'])

    def test_development_money_does_not_waive_four_service_scope(self):
        plan = {'version': 1, 'layout': 0, 'preview_unchanged': True, 'search': [],
                'legal': True, 'frozen': True, 'total_quote': 600001,
                'construction_quote': 600000, 'vehicle_quote': 1}
        with self.assertRaisesRegex(RuntimeError, 'all four material services'):
            verify_plan(plan, 0, {})


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
                                     epilog='Bounds are fixed by the owner handoff. No retry, fresh game, funding, loan or research option exists.')
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/openttd', help='Final frozen native binary (preparation pins this exact path/hash)')
    parser.add_argument('--output', type=Path, help='New directory for preparation; the same prepared directory for --execute')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--execute', action='store_true', help='Launch the one initial load and one planned cold process from unchanged prepared inputs')
    mode.add_argument('--self-test', action='store_true', help='Run admission/accounting/event guard tests without launching a native process')
    args = parser.parse_args()
    if args.self_test:
        require(args.output is None, '--self-test does not create campaign output')
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(AdmissionGuardTests)
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    require(args.output is not None, '--output is required for preparation or execution')
    output = args.output.resolve()
    if args.execute:
        require(output.is_dir(), 'Run preparation first; execution requires a prepared output directory')
        return Campaign(args.binary, output).run()
    prepare(args.binary, output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
