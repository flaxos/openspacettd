# The Hollowfrost Survey — play guide

Hollowfrost is a frozen frontier world pocked by impact craters. Through a CST gate
lies Kestrel Forge, a black basalt world of mesas. Your Frostline Survey Company must
turn Hollowfrost's rare earths and silica into quantum crystals for the Commonwealth
Survey Institute. Expect about 20–30 minutes at normal speed with one train.

From the repository root, after building:

```sh
python3 scripts/alien_worlds.py play --profile /path/to/a/new/folder
```

This copies `demo/alien_worlds/Hollowfrost-Survey.sav` into a fresh profile and
opens it. Use Save As for your own progress. The game starts paused; read the Story
Book, then unpause.

1. **Lay the line.** A graded line runs along row 128: from Hollow Mine inside the
   Hollow, through **The Breach** in its rim, past Glass Flats and Rimehold
   Observatory, to the gate terminal. On Kestrel Forge it continues from the gate
   past Kestrel Fab to the Survey Institute. Build ordinary track along it and join
   each gate terminal's open end.
2. **Order the survey train** (Hollow Mine depot): Hollow Mine (no unloading) →
   Glass Flats (no unloading) → Kestrel Fab (no loading) → Kestrel Survey Institute
   (no loading) → Kestrel Fab (no unloading) → Rimehold Observatory. The Institute
   stores *everything* unloaded there, so the train must arrive with only
   crystals. Start it.
3. **Watch the chain.** The fab makes two blanks from two silica and one rare earth
   each month; the observatory turns blanks into quantum crystals. News items
   announce the first ore through the gate, the observatory's aurora and the first
   stored crystals.
4. **Choose a contract** when the Institute's offer page opens: standard (40
   crystals, £250,000) or priority (100 crystals, £1,000,000). Priority rewards a
   second train, which needs passing loops and signals.
5. **Finish**: the closing page appears when the target is stored. Keep playing,
   for example with a passenger line between Kestrel and Ochre Point.

Success looks like the three goals filling and completing in the Goals window. Report
anything that blocks you, and whether the craters, lakes and basalt read as alien
worlds at your zoom level.
