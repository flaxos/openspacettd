# OpenSpaceTTD Sprint 11 — High-Capacity Portal Operations

Status: **COMPLETE**

Sprint 11 resumed on 2026-09-13 after the Sprint 10 stabilisation gate. Its
scope is the safe, deterministic rail infrastructure needed to turn Portal
Gates into usable high-throughput network endpoints. The Sprint 10 crash fixes,
placement rules, and savegame compatibility remain in force.

## Player outcome

- Building a Portal Gate also builds a level, two-lane terminal behind it.
- Each terminal provides a 14-tile holding lane, native rail switches, and
  one-way path signals so long entering and exiting consists do not foul the
  portal head.
- Generated worlds receive the same terminal layout on every pre-linked gate.
- The construction preview shows the terminal's longitudinal footprint.
- Land Area Information identifies linked and unlinked gates, their local and
  remote worlds, and terminal capacity.
- Spaceports are clearly identified in viewport signs and station captions.

## Implementation

- `PortalTerminal::Plan()` creates one orientation-independent layout for all
  four diagonal gate directions and rejects physical or logical boundary
  crossings.
- Portal construction preflights the head and complete terminal before making
  any map change. Terrain clearing, rail, signals, and the gate are priced as
  one command, and failure is atomic for both single gates and pairs.
- `PortalTerminal::Build()` uses ordinary OpenTTD rail tiles and PBS state, so
  track, signals, ownership, routing, and persistence continue to use native
  engine systems.
- Multi-world generation searches deterministically for a level footprint,
  rotates the preferred direction when necessary, and builds shared neutral
  terminals for generated links.
- Signal propagation accepts neutral OpenSpace infrastructure. YAPF, vehicle
  entry, signal autofill, and rail conversion reject stale portal endpoints and
  safely handle intentional one-ended heads.
- Demolishing a Portal Gate preserves its ordinary rail terminal for reuse.

## Automated acceptance

- Incremental build: `ninja -C build openttd_test openttd` — passed.
- Focused portal suite: 10 test cases and 282 assertions — passed.
- Full `openttd_test`: 158 test cases and 14,156 assertions — passed.
- CTest: 162/162 tests — passed with zero failures.
- Executable smoke test: `./build/openttd -h` — passed.
- `git diff --check` — passed.

Coverage includes all gate orientations, terminal topology and PBS direction,
atomic footprint rejection, generated terminals, unsuitable terrain, neutral
signal propagation, stale endpoints, one-ended rail conversion, portal
construction/demolition, and savegame persistence of the underlying native
rail infrastructure.

## Acceptance status

- [x] Sprint 11 explicitly resumed.
- [x] Long-consist holding capacity is at least 14 tiles per gate.
- [x] Entering and exiting traffic receive separate controlled lanes.
- [x] All four gate orientations use the same deterministic planner.
- [x] Player and generated gates share the terminal implementation.
- [x] Construction failure leaves no partial gate or terminal.
- [x] Neutral generated infrastructure is signal-safe and routable.
- [x] Closed and stale portal heads cannot be entered or routed through.
- [x] Existing Sprint 10 stabilisation behavior remains covered.
- [x] Full automated build and regression gates pass.

Sprint 11 is complete. No Sprint 12 scope has been started or inferred.
