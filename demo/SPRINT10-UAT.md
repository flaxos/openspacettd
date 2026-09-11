# Sprint 10 UAT: Planetary Operations

Launch the prepared three-world save:

```sh
./build/openttd -g demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.3.sav
```

The same file is visible in the normal load dialog under
`OpenSpaceTTD-Demos`.

## 1. Spaceport controls and status

1. Open the Story Book and select **UAT - Planetary Operations**.
2. Click **Phase 1 Spaceport candidate airport**. At the signed small airport,
   click the **Phase 1 Spaceport Candidate** station name.
3. Confirm the ordinary station window contains a compact Spaceport panel and
   a **Designate Spaceport** button.

Pass: Spaceport controls appear on this owned airport, but do not appear on
ordinary rail-only stations. The button is enabled for the owning company.

Fail: controls are missing, appear on non-airport stations, overlap existing
station controls, or are available to a non-owner.

4. Click **Designate Spaceport**.

Pass: the panel immediately shows Tier 1, the Phase 1 world name,
life-support supplies, projected monthly off-world cargo, and cumulative
output. No extra window is required.

Fail: the command errors, the station closes, values are absent, or the status
does not update.

5. Click **Upgrade Spaceport to Tier 2**, then **Upgrade Spaceport to Tier 3**.

Pass: each click advances exactly one tier. Existing supply and cumulative
counters are retained. At Tier 3 the button reads **Spaceport Tier 3
(maximum)** and is disabled.

Fail: a tier is skipped, counters reset, upgrades continue past Tier 3, or the
button remains active at Tier 3.

6. Unpause and cross a month boundary with the station window open.

Pass: cumulative off-world cargo increases and the panel refreshes without
being reopened.

Fail: cargo is not generated or the visible counters remain stale.

## 2. Edge Conduit construction, inspection and removal

1. Return to **UAT - Planetary Operations** and click the **Phase 3 signed Edge
   Conduit construction tile** location.
2. Open the Railway Construction toolbar. Hover its final tunnel-style button.

Pass: its tooltip says it builds an **Edge Conduit** beside the void. The
adjacent Portal Gate button has its own Portal Gate tooltip.

Fail: the two controls still present the ordinary railway-tunnel tooltip or
cannot be distinguished by their tooltips.

3. Select the Edge Conduit tool and click an ordinary interior Phase 3 tile.

Pass: construction is rejected with an explanation that the tile must be on a
world boundary directly beside the void.

Fail: an interior conduit is constructed or only an unrelated tunnel/slope
error is shown.

4. With the tool still selected, click the signed empty boundary tile beside
**Frontier Edge Minerals**.

Pass: a tunnel-head-style Edge Conduit is constructed facing the void, even
though the prepared site is flat. It connects to the short rail lead and costs
money.

Fail: it asks for a slope/orientation, builds in the void, creates an ordinary
tunnel, or fails on the signed tile.

5. Open **Other** (question-mark toolbar menu), choose **Land Area
Information**, and click the new conduit.

Pass: the existing window identifies **Edge Conduit**, its Phase 3 world,
mineral cargo, effective monthly extraction of 100 units (the +100% Frontier
bonus), and total extracted.

Fail: the structure is shown only as an ordinary tunnel, reports 50 units, or
omits its live counters.

6. Unpause and cross a month boundary.

Pass: minerals appear at **Frontier Edge Minerals**, total extraction rises,
and an open Land Area Information window refreshes.

Fail: no minerals arrive or the information window remains stale.

7. Select the Edge Conduit tool again and click the existing conduit.

Pass: the conduit is removed. The same removal also works with ordinary
OpenTTD dynamite, and neither route leaves a ghost conduit after save/reload.

Fail: removal affects another portal/tunnel, fails ownership checks
incorrectly, or the conduit status/production survives demolition.

## 3. Persistence

Designate or upgrade the Spaceport and build the Edge Conduit, then save under
a new name and reload it.

Pass: Spaceport tier, supplies and cumulative output persist; the Edge Conduit
returns on the correct tile with its world, cargo, production rate, owner and
total extracted intact.

Fail: either structure disappears, duplicates, resets its tier/counters, or
loads as a broken ordinary tunnel.
