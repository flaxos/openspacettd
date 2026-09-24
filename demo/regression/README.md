# Connected UAT regression fixture

`connected-legacy-terrain.sav` is the previously published v2.0 connected demo,
with its original v3 content dependencies and invalid terrain boundaries. It is
retained to test recovery, not as the player starting save. Use the current build.

```sh
python3 scripts/test_connected_uat_recovery.py --save demo/regression/connected-legacy-terrain.sav --output build/legacy-connected-recovery
```

This is the public generated demo, not the player’s crash save or personal profile.
The normal player save in `demo/` has corrected terrain and industry v4 content.
