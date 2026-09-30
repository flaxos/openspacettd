# Working on OpenSpaceTTD with agents

Use a small player-visible objective, enough evidence to reproduce it, and a clear
finish line. [AGENTS.md](../AGENTS.md) holds shared contracts; the three
[project skills](../.agents/skills/) load procedures only when relevant. This guide
is optional reading, not another startup document or project tracker.

## What changed

The old guidance required four long canonical documents before every task, repeated
status bookkeeping, and full engine suites after each implementation pass. It also
embedded a dated main-branch snapshot. Now agents use live excerpts from
`python3 scripts/agent_context.py`, read relevant sections, and run focused checks
during development. Full unit + CTest verification remains a code/content delivery
gate. Prose changes do not trigger engine builds or save regeneration.

Recent development demonstrates why the safeguards remain: headless saves can fail
native rendering; moving trains may not deliver; a manager implementation may not
be reachable from the player's UI. Preserve repros, command-path evidence and
human acceptance. This is a workflow review, not a measured attribution of quota
usage or a promised percentage saving.

## Shared skills

| Skill | Use | Finish line |
|---|---|---|
| `ost-plan` | Roadmap assessment or current slice | One approved-slice plan; no implementation |
| `ost-dev` | Bug or one gameplay slice | Regression passes; repair is runnable |
| `ost-uat` | Short mission or native scenario | Evidence plus explicit human status |
| `ost-deliver` | Final checks and publishing | Scoped commit/PR with acceptance gaps |

Codex and Antigravity 2.0 both support `.agents/skills/` and root `AGENTS.md`.
Avoid duplicate `GEMINI.md` instructions or copied skill directories. Sources:
[Codex skills](https://developers.openai.com/codex/skills),
[Antigravity skills](https://antigravity.google/docs/skills/),
[Antigravity rules](https://antigravity.google/docs/rules/).

Start a fresh session and confirm the four names in the skill selector or
customizations. Invoke `$ost-dev` in Codex or `/ost-dev` in Antigravity 2.0.
If discovery fails, ask the agent to read `.agents/skills/ost-dev/SKILL.md` directly.
These local workflows cover this game's needs without installing a large generic
skill collection. Skill bodies use repo-root-relative paths.

## Planning and execution loop

Codex with `ost-plan` reassesses the one [canonical roadmap](PROJECT_STATUS_AND_ROADMAP.md),
then writes the one [active execution plan](ACTIVE_EXECUTION_PLAN.md). The owner
approves that concrete slice. Antigravity uses `ost-dev`, `ost-uat` where needed,
and `ost-deliver` to implement, prove and publish it. Codex reviews delivery
evidence and reassesses dependencies before the next plan. Horizon A is detailed
and ordered, B moderately specified, C strategic. Only the active plan authorizes
implementation; agents may update evidence/status but must not silently reorder
the roadmap or change vision/hard architecture. Material findings return to the
planner. Automated proof and human acceptance are separate. Delivered evidence
belongs in durable project/sprint/domain/UAT records; replace the active plan
after the next slice is approved, without collecting competing active backlogs.
Historical records remain dated; source and tests outrank stale prose.

The [1 October functional-first direction](PROJECT_STATUS_AND_ROADMAP.md#functional-first-direction--1-october-2026)
requires five distinct statuses: implemented systems, functional end-to-end proof,
ordinary-start viability, human UAT and balance/pacing. A disclosed cash-assisted
functional pass is useful, but it is not ordinary-start economic evidence.
Command legality, cargo/money accounting, deterministic behavior and save integrity
stay mandatory. Keep assistance in its own ledger field, preserve failed ordinary
runs, and stop at the active slice's budget. Extra money never implicitly grants
tech/cargo, authorizes rule changes or removes a human acceptance gap.

## Codex effort

[Project config](../.codex/config.toml) defaults to medium reasoning and preserves
the selected model. Two optional CLI profiles change only reasoning effort:

```sh
codex                       # medium project default
codex --profile ost-quick   # low: bounded prose or mechanical fixes
codex --profile ost-deep    # high: desyncs, migrations, difficult invariants
```

Project configuration requires a trusted workspace; explicit session/CLI settings
can override it. Existing desktop tasks may retain their chosen effort; use their
model/effort controls. See [config basics](https://developers.openai.com/codex/config-basic/)
and [profiles](https://developers.openai.com/codex/config-advanced/).

Select available models in the client. Published API availability does not prove
account/client access. Lower effort is a starting choice for bounded work; escalate
when diagnosis stalls or simulation risk warrants it. No profile guarantees quota
savings. Approvals, sandboxing, network access and account billing are unchanged.

## Copyable prompts

Fill in only what you know; natural language is enough. Do not paste the whole
roadmap or long raw logs into each task.

**Plan/replan (Codex)**

> Reassess the OpenSpaceTTD roadmap after the last delivered slice. Use `ost-plan`
> to prepare the next `docs/ACTIVE_EXECUTION_PLAN.md` for owner approval. Do not
> implement it.

**Execute (Antigravity)**

> Implement the current `docs/ACTIVE_EXECUTION_PLAN.md` exactly within scope.
> Load relevant skills, use `ost-uat` for playable proof and `ost-deliver` for
> verification and PR. Stop at a listed material replan trigger.

**Fix a play problem**

> Fix [failure]. Steps: [actions]. Expected: [result]; observed: [error].
> Binary/save/content: [paths, if known]. Done: reproduce, smallest repair,
> relevant regression, final delivery checks and PR. Give me one short retest.

**Investigate first**

> Trace why [action] fails in [save]. Return the command path, likely cause and
> smallest next change. Investigation only; no implementation yet.

**One playable feature**

> Build [one action → visible outcome]. Keep [constraints]. Done when [observable
> acceptance]. Use existing systems; park adjacent ideas. End with a short mission.

**Continue cheaply**

> Continue [named task/PR] from [verified point]. Remaining: [one outcome]. Reuse
> unchanged evidence; rerun checks affected by new changes. Stop after [finish line].

**Switch tools/models**

> Branch/commit: [values]. Goal: [outcome]. Changed: [paths]. Proven: [commands +
> log paths]. Open: [failure/human check]. Next: [one action].

Name the outcome instead of only “do 1 and 2” when the previous list included broad
work. Stay in the same task for one bug; start fresh after a milestone or subject
change with a short handoff. Update existing authoritative records when necessary;
do not establish another permanent backlog.

## Make the result fun to try

End with launch/save, objective, a few actions and a visible success signal.
Example: **First profitable ore run** — follow one consist through the gate,
see iron reach the factory, reload and observe another delivery. Aim for useful
feedback within five minutes when game timing allows; disclose longer waits.
Ask for pass/fail and the first surprise. Record results in
[UAT results](../demo/UAT-RESULTS.md); pending checks stay pending.

Updates should state the current finding, remaining uncertainty and next proof.
Offer choices at real design forks. Keep celebrations brief and tied to verified
milestones; elaborate roleplay, extra agents, generated art and recurring monitors
are optional activities when requested.

## Delivery record — 30 September 2026

Scope: shared instructions, three local skills, Codex effort profiles and a
read-only context helper. No game source, content or save changes.

Verified: all three skills pass the skill validator; local links/source references
resolve; the helper runs from both repository root and another working directory;
TOML parses and both CLI profiles load. Codex CLI 0.159.2's local app-server discovers
all three as enabled repository skills and resolves the project to the selected
model with medium reasoning, without starting a model turn.

AGENTS.md is 4,306 bytes, down from 8,674 (50.4% smaller). This measures instruction
size, not billed tokens or weekly quota savings. The default helper output is about
3.4 KB in this checkout and explicitly labels its excerpts as historical evidence.

Antigravity fresh-client discovery and interactive usefulness remain Pending;
its documented shared paths were verified. No new human gameplay acceptance is
claimed. Compare similar bounded tasks using actual usage reports before claiming
numerical quota savings.

Engine verification for this transition follows the previous mandatory delivery
policy once; future guidance-only edits use the lighter checks in AGENTS.md.
Unit suite: **473 cases / 67,547 assertions passed**. CTest: **486/486 passed**.
Repository file-description and unused-string linters pass. Local verification logs
are in `build/agent-logs/workflow-*` (not portable audit fixtures).
