# OpenSpaceTTD Alien World Art Direction

Status: **PROCEDURAL BIOME BEHAVIOUR IMPLEMENTED; BESPOKE ART PENDING**  
Original specification: **2026-09-13**  
Status reconciled: **2026-09-14**

## Visual thesis

OpenSpaceTTD should read first as a connected interstellar railway civilisation and second as an OpenTTD-derived simulation. The world remains clear at gameplay scale, but familiar terrain is transformed through unusual ecology, atmospheric colour, monumental fixed gateways and infrastructure built for hundred-car freight consists.

The art must be original and Commonwealth-inspired rather than a reproduction of protected book covers, maps or screen adaptations. Keep the OpenTTD projection, tile footprint, palette constraints and zoom-level legibility unless an asset pipeline change is approved separately.

## Shared visual language

- **CST infrastructure:** dark graphite structure, pale ceramic armour, cyan transit light and violet portal energy. Repeated pylons, service ribs and numbered safety bands make the network feel centrally engineered.
- **Core construction:** dark glass, smooth metal, enclosed guideways, ordered plazas and cool white/cyan light.
- **Industrial construction:** concrete, weathered alloy, exposed catenary, cranes, pipe racks and amber hazard lighting.
- **Frontier construction:** modular pressure shells, corrugated panels, fabric or composite domes, field repairs and warm navigation lights.
- **Readability:** rails retain strong parallel edges; signals remain high-contrast; portal mouths have an unmistakable open/active centre; buildable ground differs visibly from void and water; cargo/loading animation never hides signal aspects.
- **Scale cues:** repeat service doors, inspection lights, containers and people-scale details around otherwise monumental gateway and arcology forms.

## Biome identities

Biome controls ecology and ground; World Phase controls settlement and infrastructure. Any phase may appear on any biome without losing either identity.

| Biome | Ground and atmosphere | Flora and landmarks | Gameplay silhouette |
|---|---|---|---|
| Temperate | Blue-green soil shadows, teal grass and mauve mineral cuts | Broad fan canopies, luminous reed clusters, pale fungal shelves | Soft organic masses around precise Core infrastructure |
| Arid Desert | Rust-red flats, violet shadow, salt-white ridges | Black spines, glassy succulents, wind-carved stone fins | Long open sightlines and stark industrial yards |
| Sub-Arctic | Blue-grey permafrost, lilac ice and turquoise melt channels | Low coral shrubs, dark thermal vents, crystalline groves | Sparse frontier structures against bright cold ground |
| Sub-Tropic | Saturated jade soil, ochre wetlands and humid blue haze | Tall segmented fronds, hanging bulb growth, dense spore crowns | Dense vertical vegetation broken by cleared rail corridors |
| Volcanic | Charcoal crust, ember fissures and sulphur-green deposits | Heat-resistant red fans, mineral chimneys, obsidian needles | Hard angular terrain with hot high-contrast seams |
| Oceanic | Deep cobalt water, turquoise shelves and pale reef flats | Floating mats, giant cup corals, mangrove-like root towers | Small land clusters joined by engineered causeways |

All six biome behaviours are implemented as of Sprint 30, including procedural terrain selection, substitutions from existing tree sprites and persistent tile loops. The original terrain, flora and landmark silhouettes described in this table have not been authored as bespoke sprite sets.

## Showcase world composition

### Phase 1 Core — Oaktree Core

Use a Temperate alien ecology disciplined by arcology planning: sparse specimen groves, paved plazas, dark-glass multi-tile towers and enclosed cyan-lit CST guideways. Portal approaches should feel civic and permanent, with clean geometry and integrated passenger concourses.

### Phase 2 Developed — Merredin Industrial

Use an Arid Desert setting with large paved logistics fields, catenary forests, alloy works, container stacks and heat-stained structures. Cyan CST routing lights and pale portal ceramics provide visual continuity against rust terrain and amber industry light.

### Phase 3 Frontier — Calyx Frontier

Use Sub-Arctic terrain with modular colony shells, rough ballast, temporary gantries, thermal utilities and isolated extraction sites. Infrastructure should look expandable and maintained under difficult conditions, while portal hardware remains unmistakably CST-standard.

## Asset delivery rules

These rules remain acceptance criteria for planned Sprint 38. The repository currently has no dedicated OpenSpaceTTD art source/export package.

- Author source sheets and export settings live with the packaged assets; generated sprites are reproducible.
- Each asset records author, licence, source and palette/zoom variants.
- Test normal, snow/desert or climate variants where applicable, all company colours and active/inactive portal states.
- Never encode simulation state using colour alone. Pair colour with silhouette, animation or a UI indicator.
- Validate at normal play zoom before reviewing close-up detail.
- Ship fallback sprites for missing optional content and emit a clear content-manifest diagnostic in federation mode.

## Outstanding visual acceptance

Sprint 24 supplied procedural differentiation and portal palette states, but the specified comparison captures and signed visual review are not present. Sprint 38 must capture comparable overview and close gameplay views of all six biomes, including terrain, a junction, a station, an industry, a settlement and active/inactive portal gates. Acceptance requires immediate world recognition without reading the label, clear track and signal states, and no visible seams.
