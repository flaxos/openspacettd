# Portal traversal regression and UAT

Repro save: `Confingway Transport, 1950-05-26.sav`.

## Faults corrected

The saved locomotive (vehicle 10, UAT Wormhole Demonstrator) was driving
backwards while hidden in gateway tile 348592, with transit progress 165.
Assigning its facing direction at emergence reversed its intended movement,
sending it back into the destination portal. Emergence now uses
`SetMovingDirection`, preserving the consist's backwards-driving state.

The rail track follower also retained the entry direction at the remote
portal. It now follows the exit head's outward direction, including when
ordinary 90-degree rail turns are prohibited. Portal virtual length remains
32 in this save; physical transit remains 16 movement units.

## Verification

- The backwards-driving regression failed before the direction correction.
- Three-car trains clear the exit with their spacing intact in all 16 pairs
  of endpoint orientations, both link directions, and both driving modes
  (64 combinations).
- Full track-follower tests cover both directions for all 16 orientation
  pairs, including continuing onto the exit approach track.
- Transit tests cover fresh entry, progress 15, and saved legacy progress 165.
- The full suite passed: 149 tests.
- The actual executable replayed the repro save using its normal game loop
  with the null video driver. At 200 loop iterations, vehicle 10 was visible
  on tile 176361, six tiles beyond the exit at 176367, travelling at internal
  speed 64 towards its station at 176357. Its hidden flag was cleared.
- After 3,000 iterations it had traversed the portal five times, recorded a
  station visit, and was still moving visibly. The original save is preserved.

## Human acceptance check

Fully quit any running game process, then launch the rebuilt executable:

```sh
./build/openttd -g 'demo/Confingway Transport, 1950-05-26.sav'
```

Unpause and follow UAT Wormhole Demonstrator (Train 1). It should emerge at
the linked gateway and continue along the track to the station. Observe a
return trip too. Pass requires visible movement away from each gate, no
rapid re-entry/flickering, and continued execution of its station orders.
The 16 movement-unit transition is speed-dependent, not a fixed wall-clock
duration. A train already beyond that counter emerges on its next movement.

Native graphical observation was unavailable in the agent session, so human
visual acceptance remains to be confirmed in the rebuilt game.
