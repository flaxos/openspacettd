# Sprint 11 UAT: High-Capacity Portal Operations

Use a newly generated OpenSpaceTTD game so its pre-linked gateways are created
with the Sprint 11 terminal layout. Existing saves retain their existing track
exactly and do not grow terminals during load.

## 1. Generated gateway terminals

1. Start a new OpenSpaceTTD game and jump between its logical worlds.
2. Locate both ends of a generated Portal Gate link.
3. Inspect the track behind each gate.

Pass: every head has a two-lane terminal extending 18 tiles behind it, including
a 14-tile parallel holding lane, merge/diverge switches, and two one-way path
signals. The entire terminal remains inside its local world.

Fail: a head has only a one-tile lead, crosses the void, has a broken switch,
or lacks its path signals.

## 2. Player construction and atomic rejection

1. Open Railway Construction and select the Portal Gate tool in Build mode.
2. Hover over a clear, level site within a world.
3. Confirm the preview extends from the head to the terminal connection tile,
   then build the gate.

Pass: one action constructs the gate and complete two-lane terminal. The cost
estimate includes clearing, all rail pieces, both signals, and the gate.

4. Try another build whose terminal would cross a world boundary or overlap
   existing infrastructure.

Pass: construction is rejected with the terminal-footprint error and neither
the gate nor any part of its terminal is built or cleared.

## 3. Traffic control and inspection

1. Run a long train toward a linked gate while another train exits it.
2. Observe the path signals and holding lane.
3. Open Land Area Information on the portal head.

Pass: the entering train can wait completely off the exit lane, the exiting
train clears the head without conflict, and the information window shows the
gate's link state, local world, remote world and coordinates, and 14-tile
holding capacity.

Fail: either train routes through a closed/unlinked head, blocks while spanning
the portal, or the portal appears only as an unidentified tunnel.

## 4. Demolition and persistence

1. Save and reload a game containing a newly built gate and terminal.
2. Confirm all terminal rails and signals return unchanged.
3. Demolish the gate head.

Pass: the gate registry entry is removed while the ordinary rail terminal is
preserved for the player to reuse or alter.

Fail: reload changes the topology/signals, or gate demolition unexpectedly
removes the terminal.
