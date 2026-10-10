# Core basket repeat and cold proof — 10 October 2026

Automated, offline `infinite_money` functional evidence. **Human UAT pending.**
Reproduce with `python3 scripts/verify_core_basket_repeat.py`; raw results are in
[verification.json](verification.json).

## What ran

Source: the preserved checkpoint (tick 304,384, SHA256
`89e94d49dc95b5df0277679dcaa7c52f95b39c2bc0b90b840f5d4484e346663f`, verified
before load). Through the PR #75 adapter the run built a 2-tile Core station at
(795,62)-(796,62), three connector rail tiles (792..794,62) and retargeted
Train 4's FOOD delivery order. Native cost £1,020 (£750 station + £270 link);
quote and paid amounts matched. Train 4's loading pickup order was untouched.

The basket session was armed, Train 28 restarted, then 36 advances (2,048 ticks
each) ran with zero command, conservation or integrity failures.

## Results

| Check | Result |
| --- | --- |
| Train 4 FOOD delivered to the new station | Yes: advance 9 (tick 321,926) and advance 36 (tick 376,083) |
| STEL + BALL arrival | STEL reserve 20; BALL delivered by Train 28 at tick 370,372 |
| Second atomic consumption | Month tick 377,347: 20 STEL + 20 BALL + 55 FOOD; all three reserves cut together |
| Growth multiplier / native hook | growth 1.0, passengers 1.0, hook enabled, native growth observed (rate 7,419) |
| Population / houses | **1,096 / 29. No house growth observed.** Population had fallen from 1,191 earlier in the run during FOOD shortage |
| Fresh-process reload | Tick 378,112, money, RNG, total cargo custody, town reserves, population and all train orders (including Train 4 -> new station) equal the saved checkpoint |

FOOD consumed is 55, not 60, because the monthly demand scales with the
then-current population (1,096). Saved checkpoint SHA256
`957f169d2583530a6f9be0ed3874dbb18e75042ea96f38f901eddc0c4ab3416f`
(local build output, not committed).

## Limits

- Native growth being enabled is not house growth; the town did not gain a house.
- The basket took 36 months because supply was not sustained: FOOD was short
  in months 12-35 and population fell. Sustained service is still unproven.
- The reload proof compares persisted state. A basket session is intentionally
  not persisted, so a post-reload advance was not attempted.
- Uses `infinite_money`; no ordinary-start economics are claimed.
- Two fixes were needed: the adapter now recognises Station 3's platform as the
  straight approach and builds a 3-tile connector, and basket admission
  accepts Train 4 pointing at the new Core consumer station. A pre-existing test
  fixture gap (stale viewport sign k-d tree in `SetupCommandAuthorityWorld`)
  that aborted one unrelated test in the default suite was also fixed.
