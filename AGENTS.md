# Codebase Memory MCP

## Repo Context

- `codebase-memory-mcp` is a local-first C knowledge-graph MCP server and CLI for codebase indexing, structural search, tracing, ADR storage, installer/config management, and optional graph UI.
- Treat this repo as a `library-cli` surface: release and installer behavior affect downstream users and agent startup configs, so changes need focused validation and conservative scope.
- Bounded execution plans live under `docs/dev/plans/`. This repo does not currently use `ROADMAP.md` or `RUNBOOK.md`; do not introduce those unless the planning model changes intentionally.

## Repo-Specific Guidance

- Main build: `make -f Makefile.cbm cbm`. If local `libgit2` auto-detection is under investigation, compare default build behavior against `make -f Makefile.cbm cbm LIBGIT2_FLAGS= LIBGIT2_LIBS=`.
- Full test entrypoint: `scripts/test.sh`. Targeted checks include `make -f Makefile.cbm test-foundation`, `scripts/smoke-test.sh`, and direct CLI/MCP smokes with an isolated `HOME` and `CBM_CACHE_DIR`.
- Never run `codebase-memory-mcp install`, `uninstall`, or `update` against the real home directory during review. Use `--dry-run`, `--plan`, or an isolated temporary `HOME`.
- Treat `build/`, local caches, generated DBs, and installed agent config files as derived local state. Do not stage them.
- Installer, MCP startup, checksum, update, and shell-command paths are security-sensitive. Validate them with isolated-home or temporary-cache smokes before claiming them fixed.
- Keep README and packaging claims aligned with verifiable behavior, especially for static/dynamic linkage, language counts, checksum verification, and supported agents.

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

## Scope

- `AGENTS.md` includes repo-local guidance plus the policy entry section.
- The durable policy body lives under `docs/dev/policies/`.
- Keep repo-specific commands, environment details, and operational caveats in this file or adjacent local docs.
