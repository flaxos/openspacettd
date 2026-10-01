# PR66 main integration and nightly portability — 1 October 2026

PR66 now carries the original Money-comparison fix, the reviewed PR69 Core
delivery and a non-destructive merge of current main. The additional test/CI
repair addresses the nightly MinGW failure without changing production gameplay,
blueprint storage rules, simulation, saves, balance or the approved assistance.
This is integration delivery, not a new functional campaign or slice.

## Preserved ancestry and proof

- Main at intake: `85c423b4a2dd57e5a2c2cf1f6d29bc2e4667845d` (reviewed PR67 plan).
- PR66 at intake: `eead18f1b4cc0f67b2a8aba6c2615f4d8069c175` (PR69 merged into
  the dependency, not into main).
- Original compiler fix: `d1bb617664dcbb122aaefc1d4175c34ae1a17e2e`.
- Reviewed functional delivery: `3630d490304916eb16c812b69e0aed92bfdd0f96`.
- Shared earlier main: `91846666f69929443eab04cea63f9f8cfb3bb0be` (PR65), with
  the existing art and maintenance ancestry preserved.

The integration merge retains the reviewed functional tree exactly. Its four
documentation conflict resolutions preserve the completed records and the common
reviewed-main text. No failed PR62 branch was imported, and the existing signed
Money conversion was retained rather than duplicated. The active plan, functional
archives, checkpoint saves and published content remain unchanged.

[Frozen functional evidence](../functional-core/README.md) remains **ASSISTED
FUNCTIONAL requested outcome PASS across authorized handoffs, complete acceptance
PARTIAL, ordinary-start economics unproven, human UAT Pending**. The sole
£6,000,000 virtual allowance is already consumed, with £0 remaining. This work
starts no functional campaign and issues no campaign grant, loan, operating proof,
continuation or replay. The campaign is closed; remaining counters are not new
execution authority.

## Diagnosed nightly failure and bounded repair

[Original nightly run 36845031443](https://github.com/flaxos/openspacettd/actions/runs/36845031443)
failed on main, before these fixes were integrated:

- Intel Mac x64 compilation rejected the ambiguous comparison between an
  `int64_t` JSON quote and `Money`. The existing PR66 compiler fix casts Money
  explicitly to `int64_t`; this integration makes no further production edit.
- MinGW compiled, then its combined symlink/oversized blueprint case threw
  `filesystem error: cannot create symlink: Function not implemented`. Its
  oversized-file assertions were never reached. That historical result remains
  FAIL (498/499 CTests passed), not a result for this new delivery.

The blueprint regression now has two independent cases. Oversized-file scan and
import rejection run unconditionally and verify the external bytes and the
oversized-file size remain unchanged. Symlink creation uses the nonthrowing filesystem
API and permits only unsupported-operation or denied-permission capability errors.
Those hosts emit **Symlink rejection coverage NOT RUN**; unexpected errors fail.
Supported hosts must create a real symlink and pass the original scan/import
rejection and unchanged-target checks, including preserving the symlink itself.

The existing nightly workflow also runs on PRs that change these regression inputs
or the affected platform workflows. It reuses the same Intel x64 Mac and MinGW
jobs, cron and manual dispatch. The MinGW step logs the two focused cases verbosely
before running its unchanged full CTest suite, so a capability omission is visible
even when the suite passes. No platform is removed or privileged trigger added.

## Preserved merge history and the commit gate

Running the upstream commit checker on the actual integration range exposed a
second tooling defect: it rejects both the preserved GitHub PR69 merge subject
and the conventional `Merge: ...` reconciliation subject. The original failure
is retained in `build/agent-logs/integration-commit-check-before.log`. Neither
commit was rewritten.

The local CI adapter preserves the upstream `HEAD^..HEAD^2` PR range and checks
every commit's first-parent diff with the same upstream validators. Only actual
two-parent merges with a standard GitHub PR subject or a `Merge: <description>`
subject receive a `Change: ` prefix in a temporary validation copy. The complete
original subject and body remain subject to upstream ASCII/whitespace checks;
all other subjects retain the upstream format rules. No SHA-specific exception,
commit exclusion or diff waiver is introduced. The upstream tools still come
from `OpenTTD/OpenTTD-git-hooks@main`, as before.

Ten real-Git fixtures pass: both accepted merge forms, an ordinary conventional
commit and the exact synthetic-merge CI range; both nonmerge spoofs, an unexpected
merge subject, non-ASCII subject, trailing body whitespace and bad source diff
produce their required failures. The repaired actual integration range also
passes. These expected negative probes are not passing-invalid-message evidence.

## Verification and delivery

| Check | Result | Evidence |
|---|---|---|
| Native configure/build | PASS | Clean isolated Ninja Debug build with assertions; LibLZMA and normal detected dependencies enabled |
| Supported-host regression | PASS | Real Linux symlink and independent oversized rejection: 17 assertions / 2 cases |
| Simulated ENOSYS | PASS with symlink coverage NOT RUN | 13 assertions / 2 cases; oversized scan/import checks still run |
| Unexpected I/O error | PASS expected-negative probe | EIO causes the required assertion failure and exit 1; it is not silently skipped |
| Full native suite | PASS | 498 cases / 306,402 assertions on this local build |
| Sequential isolated CTest | PASS | 513/513 cases, including isolated GUI, command-authority and regression checks; 67.73 seconds |
| Commit-validator graph fixtures | PASS | All 10 positive/expected-negative cases; actual integration commit range also passes |
| Repository linters, mode, YAML and diff | PASS | Both linters, script-mode enforcement, all three changed workflows parsed and diff checked |
| Protected-file preservation | PASS | 235 tracked proof, plan, content and save files byte-identical to reviewed delivery; all earlier roadmap sections unchanged |
| Independent source review | CLEAR | Astra maximum: test, workflow, merge-adapter and preservation checks |
| Exact-head remote CI | PENDING at publication | Terminal results will be reported on [PR66](https://github.com/flaxos/openspacettd/pull/66), including Intel x64 Mac and MinGW |

Native logs remain in the isolated worktree's `build/agent-logs/`; the older
functional build and its evidence are preserved. The ENOSYS injection is a local
capability-branch probe, not a result for the real MinGW runner. Passing suite
status on an unsupported runner does not establish real symlink-rejection proof.
Pending CI is not passing evidence and historical green runs do not substitute
for the new exact head.

PR66 remains an open draft targeting main. Publication does not merge it or
establish human acceptance. A fresh checkout of its exact reviewed head is the
safe way to build the integrated source without disturbing personal games or
profiles. After compiling, an owner can open the blueprint library, import a
normal blueprint, export it, and verify it appears after restarting; symlink and
oversized-file rejection are automated storage checks, not human UAT results.
