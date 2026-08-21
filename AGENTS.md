# Codebase Memory Mcp

## Repo Context

- Describe this repo's purpose, canonical planning surfaces, and operating model here.

## Repo-Specific Guidance

- Add the exact commands, constraints, and local conventions this repo expects.

## Policy Loading Contract

- `AGENTS.md` is a routing surface, not a one-time pointer.
- Re-read the relevant policy files under `docs/dev/policies/` at the start of any non-trivial turn.
- Re-read the relevant policy files when task scope changes mid-session.
- When behavior is ambiguous, prefer re-reading policy over improvising from stale assumptions.

## Policy Re-read Triggers

- re-read planning-related policy before opening, revising, or closing a substantive plan
- re-read documentation-related policy before changing docs, contracts, or canonical authorities
- re-read validation and closeout policy before claiming work complete

## Policy Entry

This repo keeps its durable repo-local policy under `docs/dev/policies/`.

Read and follow:
- `docs/dev/policies/0001-planning-discipline.md`
- `docs/dev/policies/0002-policy-management.md`
- `docs/dev/policies/0003-policy-upgrade-management.md`
- `docs/dev/policies/0004-policy-adoption-feedback-loop.md`
- `docs/dev/policies/0005-graph-backed-memory-usage.md`
- `docs/dev/policies/0006-codegraph-usage.md`
- `docs/dev/policies/0007-git-worktree-hygiene.md`
- `docs/dev/policies/0008-commit-history-discipline.md`
- `docs/dev/policies/0009-branch-and-integration-strategy.md`
- `docs/dev/policies/0010-commit-and-push-cadence.md`
- `docs/dev/policies/0011-versioning-and-release.md`
- `docs/dev/policies/0012-turn-closeout.md`
- `docs/dev/policies/0013-subagent-workflow-optimization.md`
- `docs/dev/policies/0014-validation-and-handoff.md`
- `docs/dev/policies/0015-subagent-runtime-governance.md`
- `docs/dev/policies/0016-upstream-fork-maintenance.md`
- `docs/dev/policies/0017-workspace-auto-index-governance.md`
- `docs/dev/policies/0018-goal-execution-governance.md`
- `docs/dev/policies/0019-notes-and-memories.md`
- `docs/dev/policies/0020-policy-management.md`
- `docs/dev/policies/0021-policy-upgrade-management.md`
- `docs/dev/policies/0022-policy-adoption-feedback-loop.md`
- `docs/dev/policies/0023-graph-backed-memory-usage.md`
- `docs/dev/policies/0024-planning-discipline.md`
- `docs/dev/policies/0025-codegraph-usage.md`
- `docs/dev/policies/0026-git-worktree-hygiene.md`
- `docs/dev/policies/0027-commit-history-discipline.md`
- `docs/dev/policies/0028-branch-and-integration-strategy.md`
- `docs/dev/policies/0029-commit-and-push-cadence.md`
- `docs/dev/policies/0030-versioning-and-release.md`
- `docs/dev/policies/0031-turn-closeout.md`
- `docs/dev/policies/0032-goal-execution-governance.md`
- `docs/dev/policies/0033-subagent-workflow-optimization.md`
- `docs/dev/policies/0034-parallel-plan-design.md`
- `docs/dev/policies/0035-validation-and-handoff.md`
- `docs/dev/policies/0036-subagent-runtime-governance.md`
- `docs/dev/policies/0037-notes-and-memories.md`
- `docs/dev/policies/0038-active-lane-coordination.md`
- `docs/dev/policies/0039-upstream-fork-maintenance.md`

## Scope

- `AGENTS.md` includes repo-local guidance plus the policy entry section.
- The durable policy body lives under `docs/dev/policies/`.
- Keep repo-specific commands, environment details, and operational caveats in this file or adjacent local docs.
