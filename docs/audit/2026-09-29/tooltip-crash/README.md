# Empty-tooltip crash repair

Reported crash: `crash20260929060130.json.log`, 29 September 2026 06:01:30 UTC.
Assertion: `src/gfx.cpp:718: maxw > 0`.
Stack: `GuiShowTooltips` → `TooltipsWindow::UpdateWidgetSize` → `GetStringHeight`.
The screenshot shows the prompt-generator window. Its preset/Generate buttons
use `STR_EMPTY` as a tooltip; HQ, facilities and trade tabs have the same defect.
An encoded string ID is nonempty even when it decodes to empty text.

The repair rejects decoded zero-width help before window construction and declares
absent button help as `STR_NULL`. It preserves the renderer assertion and all
simulation/save behavior. The regression aborts before the guard, then passes with
the guard, including empty parameters, newline-only text, normal multiline help,
replacement/closure, hover and right-click, at 100%, 150% and 200% scale.

## Verification

- Fresh main-based default CMake/Ninja build succeeds with debug assertions.
- Full unit suite: **473 cases / 67,508 assertions**, [log](unit.log).
- CTests: **486/486**, including the isolated GUI regression, [log](ctest.log).
- Tooltip regression: **60 assertions**, [passing log](tooltip.log),
  [expected pre-fix abort](before-fix.log).
- File-description and unused-string linters, plus `git diff --check`, pass.
- User crash save loads, advances from 1955-04-23 to 1955-04-24, writes a
  separate save and reloads at 1955-04-24. Verified with both the
  [working-folder binary](save-evidence.json) and
  [clean main-based binary](clean-save-evidence.json).
- Original crash artifacts remain unchanged; [SHA-256 values](original-artifact-hashes.json).

Reproduce the automated GUI check with:

```sh
cmake -B build -G Ninja
ninja -C build
./build/openttd_test 'Tooltips reject empty rendered text and preserve visible help'
./build/openttd_test
ctest --test-dir build --output-on-failure
```

The tooltip case is intentionally isolated because its null video driver and GUI
state must not leak into other Catch cases. CTest explicitly registers it.
For the save check, load a copy of the reported save in a dedicated server, use
`getdate` before/after `unpause`, save to a new path with threaded saves disabled,
then load that path in a fresh process and compare `getdate`.

The fresh default CMake build enables debug assertions. It exposed a pre-existing
terrain regression fixture that called `MakeClear` on outer map edges, violating
`SetTileType`'s invariant. The fixture now makes those edges void; its existing
recovery assertions are unchanged. No production terrain code was changed.

## Scope and human acceptance

Human graphical UAT remains **Pending**. The null-driver tests construct real
native tooltip windows but do not establish pixel/layout acceptance on the user's
NVIDIA/SDL-OpenGL desktop. The optional desktop tool was unavailable. Retest steps
are in [UAT results](../../../../demo/UAT-RESULTS.md).

The user's original crash log, screenshot and save remain untouched. Their
working folder also contains pre-existing uncommitted scenario-generator changes.
A full-suite run there failed in Sprint 48 scenario synthesis and stalled in the
prefab-world fixture. The crash fix is therefore delivered and fully tested in a
separate worktree based on main `bc18c2c8fc`, excluding that work. The regular
working-folder game binary also contains the tooltip repair.
