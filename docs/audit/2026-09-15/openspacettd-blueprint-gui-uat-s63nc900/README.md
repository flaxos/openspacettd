# Requested graphical Blueprint UAT — blocked

Requested: UAT-00,04a–c,09 using the rebuilt game, a copy of the original crash
save and a fresh clear pad. This attempt performed no GUI interaction, placement,
visual comparison or save/reload. No case is marked Pass.

## Current evidence

- `orca-ide skills get computer-use --json`: FileNotFoundError, no such executable.
- Native-app APIs are disabled in the available computer-control tool.
- `xrandr --query`: exit1, `Can't open display :0.0`.
- Explicit SDL launch of the rebuilt game on the crash-save copy stayed alive
  for the10-second bounded probe. The probe stopped it afterward. Its only output
  was a shader-cache directory warning. A live process is not proof of a visible
  window, loaded UI, placement correctness or human UAT.
- All original/copy save hashes were verified unchanged afterward. See `run.json`
  for binary SHA256, exact launch command, filenames and individual statuses.
- The fresh clear pad has **not** been prepared or inspected; M1 is only copied.

## Prepared inputs

- Crash save copy: `/tmp/openspacettd-blueprint-gui-uat-s63nc900/blueprint-crash-copy.sav`
- M1 copy: `/tmp/openspacettd-blueprint-gui-uat-s63nc900/uat-v1.1-copy.sav`
- Isolated configuration: `/tmp/openspacettd-blueprint-gui-uat-s63nc900/openttd.cfg`
- `launch-copy.sh crash` or `launch-copy.sh m1` launches these copies from a desktop
  session. Configuration writes are disabled with `-x`; save to a new case filename.

## Resume

Provide a working native desktop-control session, then follow the authoritative
`demo/ALL-FEATURES-UAT.md` UAT-00,04a–c,09, including all eight previews/placements,
four separate rotations and two flips, clear-pad and obstruction checks, money/
materials, and actual save/reload. Existing automated/headless evidence does not
satisfy these graphical cases. The fresh pad must be prepared and recorded then.

The computer-use skill requires: “If the selected executable cannot run, report
its exact error and stop.” No fallback Orca executable or substitute GUI pass was used.
