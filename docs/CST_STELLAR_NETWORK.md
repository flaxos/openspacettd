# CST star map and gate expansion

Implemented on `codex/cst-star-map-expansion`, based on main `8464e9374a`
(the player-built resource economy). This is an opt-in new-game sector. Existing
saves keep their previous direct-link rules unless they contain stellar metadata.

## Player loop

Choose **Mito–Merredin sector** in new-game generation. This selects at least a
1024×1024 map and activates the installed Commonwealth industry v4, rail v3 and
new equipment v1 packs. Install the packs built by this repository first. The
preset adds a bounded set of starter processors to the player-built economy;
other processors remain player construction. Primary sites and company-specific
prospecting retain the resource-economy rules.

Open **Universe Directory → Star map**. The initial public CST network joins
Mito–Merredin, Merredin–Clonclurry and Merredin–Valvida. Any company may use it
without Portal research, at a fixed 100 currency units per admitted train.
Opening a frontier gate does not found a colony, promote the world's development
phase or discover its minerals.

1. Research Portal I, II, III or IV for a commissioning range of **10, 25, 50 or
   100 stellar units**. Range is Euclidean distance between authored star
   coordinates, not distance between terrain tiles.
2. Build an unlinked departure gate on an open world. Register an owned logistics
   hub station within 32 Manhattan tiles in that same world.
3. Select the source gate, target star and one of its three arrival zones. The
   map shows reachable destinations and the construction quote. New links may
   connect already open worlds as shortcuts.
4. Start the project. Each started ten-unit distance band requires **200 steel
   and 40 machine modules**; even a zero-distance link uses one band. A reservation
   fee of ten tunnel-price units per band is charged immediately.
5. Deliver the equipment to the chosen hub using ordinary cargo transport. Project
   inventory receives only its remaining bill of materials; excess retains the
   usual stockpile handling. The equipment recipe produces one `MACH` from two
   steel, one wiring and one chip at a developed-world station facility.
6. Activate a ready project. The normal arrival-gate construction cost is charged,
   the equipment is consumed once, and the physical endpoints become a permanent
   usable link. Cancel before activation to return delivered equipment to the
   source-world company stockpile. The reservation fee is not returned.
7. New links are private. Select their source gate to make them public and set a
   per-train toll. Owners travel free; guests must have cash. CST links remain
   neutral public infrastructure. Access is checked on the terminal approaches
   as well as the wormhole. For cross-server projects, change access at the
   commissioning end; its policy is replicated to the arrival end.

The additive equipment pack preserves the published industry/rail pack hashes
and native cargo slots. It adds `MACH` in slot 29 and a refittable CST Gate Equipment
Carrier. See [the content manifest](../pkg/commonwealth_manifest.json).

## Authored geography

| World | Coordinates | Initial access |
|---|---|---|
| Mito | 0, 0 | CST backbone |
| Merredin | 8, 0 | CST backbone |
| Clonclurry | 11, 5 | CST backbone |
| Valvida | 12, −5 | CST backbone |
| Chelva | 18, 0 | Closed frontier |
| Tandil | 35, 10 | Closed frontier |
| Pioneer Reach | 65, 10 | Closed frontier |

These are game coordinates, not an assertion of astronomical canon. Existing
catalogue entries are referenced by ID; Pioneer Reach is scenario geography.
The larger lore catalogue remains metadata in Universe Directory. It does not
implicitly generate hundreds of playable maps. Resources depend on native terrain,
biome and industry-placement rules; every individual world is not guaranteed every
resource type. The fixed regions never expand or move after generation.

## Connected hosts and remote schedules

**Connected worlds** lists worlds, stations, gates, arrival zones, projects and
train locations published by actual hosts in one registered universe. Native train
orders gain **Remote stop**. Select an owned station to append a native station
order, or a public/owned gate to pin a route. Standard load/unload controls still
apply. Automatic routing chooses fewest hops, then lowest advertised toll, then a
stable key order. Offline, incompatible or inaccessible routes hold the train.

Select a world and **Visit** from a multiplayer client. The client uses the normal
network join and company authentication, loads the target host's map and restores
its camera for that world. Only one host is loaded by a client at once. A dedicated
server or local single-player simulation cannot be abandoned by this action.
Select a train's directory row to visit its current host and reopen its native
train/orders windows. The last destination-picker selection survives the switch.
The directory refresh is periodic, so a moving train may briefly have a stale
location; refresh and select it again if it has already departed.

A remote gate project uses the same delivered equipment and research rules as a
local project. The target host first reserves a valid arrival zone and publishes
its construction quote. Activating commits that price once at the source. The
receiver builds once, the source links once, and the receiver releases admission
only after the source acknowledgement. An interrupted handshake resumes from its
persisted state. A committed project cannot be cancelled. Permanent remote links
cannot yet be demolished: distributed unlinking is a separate lifecycle operation.

### Host setup

Use the same engine build, NewGRFs and content parameters on all hosts. Keep the
Universe Authority persistent and run the game hosts as dedicated servers.
Before generating each host, assign disjoint world-ID ranges with
`OPENSPACETTD_WORLD_ID_BASE` (for example 0 and 100). This is generation-only:
loading a save preserves its IDs and immutable rectangles. Do not clone a live
save as a second host. The authority rejects colliding worlds and a live namespace
advertised at a different address.

Configure these environment variables on the game servers:

| Variable | Purpose |
|---|---|
| `OPENSPACETTD_AUTHORITY_URL` | Shared authority URL, such as `http://127.0.0.1:8080` |
| `OPENTTD_UNIVERSE_HOST_TOKEN` | Host-registration secret, also configured on the authority |
| `OPENTTD_UNIVERSE_GAME_ADDRESS` | Reachable native game address, such as `game-host:3979` |
| `OPENSPACETTD_WORLD_ID_BASE` | First generated world ID; default 0 |

Start the authority with `scripts/universe_authority.py --state-file PATH` and
its normal `--host` / `--port` arguments. Protect the host credential and authority
service using the deployment's existing private network/security controls. It is
host registration, not a replacement for native game/company authentication.

Create the player's local company on each host. The source server console command
`universe_company_map` lists its current `namespace:sequence` identity. On a new
remote company, use `universe_company_map 1 HEX:HEX SEQUENCE` to map company number
1 to that same identity. This is a server-only replicated command. Provision before
building trains or starting projects; remapping an active transfer/project host
is rejected. It neither copies the treasury nor bypasses company passwords. Use
the native company credentials when visiting. Local cash ledgers stay separate.

### Persistence and boundaries

`STLR` stores geography, zones, local projects, access policies and admissions.
`UNET` stores the replicated directory, gate-order proxies and remote project
state. Existing identity, cargo, schedule and transfer-journal chunks continue to
carry trains. Load restores state without constructing anything from stale saved
directory rows. Native replicated updates drive subsequent remote reconciliation.
Post-load validation checks project references and proxy stations.

Coordinated recovery requires **all participating native saves plus authority
state and the matching binary**, after pausing and draining both freight transport
and directory requests. `scripts/federation_checkpoint.py` now validates the full
registered world set, including three-server runs. Old two-world checkpoints
retain their format. Independent old-save rollback, partially restored clusters
and arbitrary power-loss recovery are not accepted by this feature.

Native shared order pools do not span hosts. Remote schedules reject sharing and
edits during a prepared departure; stale remote edits carry a schedule revision
check. The verified route uses ordinary station orders and load/unload flags.
General conditional/timetable/depot-refit schedule migration, cross-host company
mergers, account-wide wallets, federated single sign-on, large-directory scaling
and administrative star-coordinate editing are outside this delivery.

## Verification and reproduction

[Retained results](audit/2026-09-28/stellar-network/README.md) distinguish automated
checks from [human graphical UAT](../demo/CST-STELLAR-UAT.md). Run from the repo root:

```sh
ninja -C build
./build/openttd_test
ctest --test-dir build --output-on-failure
python3 scripts/test_stellar_generation.py --output build/stellar-generation
python3 scripts/test_stellar_multihop.py --output build/stellar-multihop
```

The generation runner creates a real new map, audits all height edges and reloads
in a fresh process. The freight runner uses three dedicated servers and three
joined SDL dummy clients, real station loading, four physical hops per round trip,
identity/order checks, native cargo/cash accounting and a full coordinated restart.
It does not dispatch trains through a test-only transfer shortcut. The loopback
run requires installed OpenGFX and takes roughly twenty minutes at normal speed.
Passing that runner does not establish the star-map layout or seamless-visit UI's
human acceptance.
