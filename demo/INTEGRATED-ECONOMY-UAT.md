# Integrated Commonwealth economy — human acceptance

Status: **Pending**. Automated tests do not establish graphical acceptance.
Use the binary and content hashes in the evidence manifest. Keep existing saves
with their original content. Two new saves are supplied:

- `OpenSpaceTTD-Integrated-CST-Sector.sav`: fresh seven-world sector, empty city reserves.
- `OpenSpaceTTD-Integrated-Economy-UAT.sav`: operating all-chain economy after the
  verified progression, soak and food recovery; an authored test network.

From the repository root, after building:

```sh
./build/openttd -c demo/integrated_economy.cfg -g demo/OpenSpaceTTD-Integrated-Economy-UAT.sav
```

Alternatively start a new game using integrated v1, rail v3 and equipment v1,
with the Mito–Merredin seven-world preset.

1. Open the star map. Confirm Mito is Core, Merredin/Tandil Industrial and the
   other worlds Frontier. Open a gate and found a colony separately. Promote a
   frontier colony and confirm its raw industries remain legal.
2. Survey a copper/silica site on an established frontier. Fund Industry must
   explain role, research and materials; an unsurveyed mine must be rejected.
   Build a conventional rail service from a raw producer to a physical factory.
3. Locate the same factory in Facilities. Stop its feeder: missing inputs halt
   batches. Stop collection: its output fills and production stops. An unrelated
   warehouse must gain nothing until cargo is delivered there.
4. Deliver steel/chips to an owned warehouse on Mito. Check its storage-priority
   notice if consumers are nearby. Deposits earn no final-delivery income. At HQ,
   research Tier III: cash alone cannot finish; the complete kit reserves once.
   Switch projects and confirm the unused reservation returns. Acceleration starts
   off and never consumes reserved project goods.
5. In Cash mode, preview electric construction without materials, then with them.
   Check missing-material text, successful deductions, conversion, blueprint and
   vehicle replacement. Conventional rail and diesel remain cash-buildable.
6. Inspect the core city. Deliver food, then steel/ballast, then chips/consumer
   crystals. Read each reserve and blocker. Stop food for over three months:
   growth halts and passenger production falls; resume service and observe recovery.
7. Commission a gate with physically delivered steel/machine modules and retain
   room for later frontier rare-mineral expansion. Check star-map window links.
8. With three registered hosts and one mapped company, research only at its home.
   Confirm unlocks arrive remotely, remain during authority outage, and persist
   through a coordinated checkpoint/restart. Local money and stock stay separate.

Record binary/save/content hashes, language, UI scale, observed results and any
screenshots in the UAT results register. Check both standard and larger UI scales;
English custom panel text and wrapping still need a human readability pass.
