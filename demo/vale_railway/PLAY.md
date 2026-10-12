# The Vale Railway — owner play mission

This is Vale Railway's founding day in Aster Vale, a Phase 3 Frontier valley.
Greybank is the regional city, Stonecross the quarry town, and Merefield a farming
village. Their homes and roads are already there; your company builds the useful
railway. The opening is paused. A playable save is available only after native
preparation succeeds and closes with a passing receipt. Prior local native preparation and service/reload proof passed. A fresh checkout
still needs its own successful preparation receipt; private saved games are not
bundled with this source draft.

From the repository root, launch that verified preparation directory with:

```sh
python3 scripts/play_vale_railway.py --opening /absolute/path/to/verified-preparation
```

The launcher checks the save, executable, content and configuration hashes, then
copies the opening into a new local profile. Keep the original opening and
evidence. Use Save As for your continuing game.

1. Read the Story Book and use its location links. The quarry supplies silica
   (SILC); Stonecross Works turns it into ballast (BALL); Greybank Yard stores
   finished ballast. Your £750,000, imported 80 STEL, Materials I, industries,
   four platforms, depot and track stubs, and empty Pioneer with two SILC hoppers
   are founding assistance. Both through routes are unfinished; all SILC and BALL
   stores start empty.
2. Complete the rail link from **Stonecross Quarry** to **Stonecross Works Input**.
   Give the starter Pioneer recurring station orders: full load at the quarry,
   unload/no load at Works Input. Start the train and unpause. The first useful
   signal is SILC accepted at the works, followed by BALL production. The first
   goal needs 80 SILC accepted and 40 subsequent native ballast batches.
3. Extend rail from the separate **Stonecross Works Output** platform to
   **Greybank Yard**. Build an output-side depot, then buy a short train with
   **Commonwealth Modular Freight Cars**. Filter **Ballast and Concrete**, click
   the car row, then Buy Vehicle. New cars already carry40 BALL; the starter SILC
   hoppers cannot refit to BALL. Order it to load BALL
   at Works Output and unload/no load at Greybank Yard; keep the orders recurring.
   The second goal needs 40 crusher-made BALL deposited at the yard. This is
   company material storage and earns no sale income.
4. Leave the export service running: the **same export train** must deposit BALL,
   return to Works Output, then make a second positive deposit at Greybank Yard.
   The closing page says “Vale Railway is established”; your company, cash,
   trains and materials remain available for free play.
5. Save and reload your copy. Stop one train for a cycle, then restart it. Check
   that the settlements remain playable and the service resumes.

Ordinary pioneer rail is cash-built at first; fabrication starts off. Later, BALL
and the finite imported STEL can support optional fabrication. Empty works input
means send SILC; full output means collect BALL. There is no mission deadline or
starvation loop. Passenger service between the settlements is optional free play.

Report pass/fail and the first unexpected or unclear step. First feedback is
targeted at 5–8 minutes and the full chapter at 15–25 minutes; neither timing nor
enjoyment has been verified by human play.
