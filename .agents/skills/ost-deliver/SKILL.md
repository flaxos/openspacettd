---
name: ost-deliver
description: Finish an authorized OpenSpaceTTD implementation with proportionate verification, delivery records, a scoped commit and GitHub PR. Use at completion or for a commit/PR request, not read-only questions.
---

# OpenSpaceTTD delivery

Inspect status/diff and the actual branch base. Preserve unrelated edits and PR
work. If stacked on an unmerged branch, target that branch and state the dependency;
do not quietly include another feature in a main-targeted PR.

## Verify once after the last relevant change

Code/content: rebuild affected outputs, then run `./build/openttd_test` and
`ctest --test-dir build --output-on-failure`. Both are required: CTest contains
isolated GUI cases excluded from default Catch execution. Include native/save/network
proof relevant to the claim, following `ost-uat` when needed.

Guidance/prose only: inspect links/instructions, validate skill frontmatter/TOML,
and execute changed helpers. Do not rebuild/retest the engine merely because prose
changed. For every delivery run:

```sh
python3 .github/file-descriptions.py
python3 .github/unused-strings.py
git diff --check
```

Keep long logs in `build/agent-logs/`; check command exit codes before summarizing.
A successful `tail` is not a successful test. Do not repeat green suites after
metadata-only edits. Distinguish inherited failures from new regressions using
evidence; do not claim success or repair unrelated systems opportunistically.

## Record and publish

Features/sprints update `docs/PROJECT_STATUS_AND_ROADMAP.md`,
`docs/SPRINT_LEDGER.md`, `docs/KNOWN_LIMITATIONS.md`, and `demo/UAT-RESULTS.md`.
Update `docs/CURRENT_ARCHITECTURE.md` only for architecture changes. Small tooling
changes need one linked record with verification/limitations. Preserve historical
evidence; never invent human acceptance or current test counts.

Stage task-owned files, inspect the staged diff, make a conventional commit, push
the descriptive `codex/...` branch to `openspace`, and open/update its PR. Use a
body file for multiline `gh` text. Describe the problem, resulting behavior, checks,
acceptance gaps and relevant issue IDs. Do not merge or launch an unrequested CI
monitor. If publishing fails, preserve the commit and explain why.

Finish with outcome, PR, decisive checks and remaining human acceptance. Consult
current priorities for roadmap questions; read historical recovery/sprint registers
only when relevant. Offer at most 2–3 useful next choices, recommend one, and do
not begin the next feature without a request.
