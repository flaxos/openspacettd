---
name: ost-plan
description: Reassess OpenSpaceTTD roadmap dependencies or prepare the single current execution slice without implementing gameplay.
---

# OpenSpaceTTD planning

Run `python3 scripts/agent_context.py` once per task. Inspect the relevant current
sections of `AGENTS.md`, the canonical roadmap, architecture, limitations, sprint
ledger and workflow; spot-check owning source/tests where claims need confirmation.
Current source/tests outrank stale prose; dated evidence stays historical.

Decide whether the request is a roadmap reassessment or active-slice plan. Keep one
canonical roadmap (`docs/PROJECT_STATUS_AND_ROADMAP.md`) with dependency-ordered,
detailed Horizon A, moderately specified Horizon B and strategic Horizon C. Avoid
speculative sprint numbers. Keep exactly one implementation-authoritative
`docs/ACTIVE_EXECUTION_PLAN.md`; replace it only when the next slice is agreed.
Move delivered evidence into durable project/domain/sprint/UAT records rather than
accumulating completed plans as competing backlogs.

Define a player-visible outcome, normal starting contract, existing implementation,
smallest in/out scope, failure paths, deterministic proof, human mission and replan
triggers. Separate implemented behavior, automated verification, playable balance
and human acceptance. Preserve one-map world regions, deterministic simulation,
server-authoritative commands and the current integrated economy path. Never
silently redefine vision, hard architecture, development phase versus economic role,
or the roadmap order. Stop for owner approval at a material product/architecture
fork. Implementation agents may update evidence/status but may not silently
reorder the roadmap.

Stop after the requested plan and handoff; do not implement the slice. Following
`ost-deliver`, reassess dependencies from delivered evidence before planning the
next approved slice.
