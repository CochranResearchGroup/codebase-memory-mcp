# Plan 0002 | Workspace Auto-Index Policy

State: CLOSED

## Objective

Implement an opt-in workspace auto-index path that can discover and maintain eligible sibling Git repositories under explicit workspace roots while avoiding artifact-heavy and non-code storage.

## Current State

- Per-session auto-indexing still exists behind `auto_index`.
- Broad workspace indexing now exists as an opt-in path behind `workspace_auto_index` and `workspace_roots`.
- Eligibility is gated by tracked, indexable source files, not raw repository size or all tracked file count.
- A cache-local `_workspace_index.lock` prevents concurrent broad workspace applies across multiple MCP server processes.
- The installed runtime has `workspace_roots=/home/ecochran76/workspace.local` and `workspace_auto_index=false`. Broad startup auto-indexing remains implemented but disabled because real startup smoke still showed multi-GB MCP growth; manual preview/apply is the safe current operating mode.
- Manual workspace apply populated the current cache: preview shows 73 repositories discovered, 72 already indexed, and 1 skipped because it has no indexable source.

## Scope

- Add durable policy for workspace auto-indexing.
- Add configuration keys for broad workspace auto-indexing.
- Add a preview/apply CLI command for workspace indexing.
- Add MCP startup wiring that applies the broad workspace policy only when explicitly enabled.
- Use tracked, indexable source file count rather than raw disk size or all tracked file count as the eligibility gate.
- Add a cache-local cross-process workspace-index lock so multiple MCP server processes cannot broad-index the workspace concurrently.

## Non-Goals

- Do not recursively scan arbitrary home-directory contents.
- Do not enable workspace auto-indexing by default.
- Do not rewrite existing single-session `auto_index` semantics beyond sharing safer eligibility logic where practical.
- Do not open upstream PRs from this fork-local slice.

## Acceptance Criteria

- `AGENTS.md` points to the installed selector-recommended policies and the new workspace auto-index policy.
- `codebase-memory-mcp workspace-index --root <path> --json` previews repositories with tracked count, indexable source count, status, and reason.
- `workspace_auto_index`, `workspace_roots`, and `workspace_index_limit` are documented config keys.
- MCP initialize starts broad workspace indexing only when `workspace_auto_index=true` and `workspace_roots` is set.
- Background workspace indexing does not block MCP startup and respects the global pipeline lock.
- Background workspace indexing respects a cross-process workspace lock so only one broad workspace apply can run at a time across agent-launched MCP processes.
- Existing indexed repositories discovered through workspace roots are not registered with every MCP process watcher; per-session watcher behavior remains, and broad watcher ownership is deferred until a singleton owner exists.
- Validation includes build, focused tests or smokes, and an isolated-home workspace preview/apply smoke.

## Definition Of Done

- Policy artifacts, implementation, docs, and validation evidence are committed or otherwise ready for review.
- The installed runtime is updated only after validation, and real-home workspace auto-indexing is enabled only after preview evidence and explicit operator intent.

## Validation

- `make -f Makefile.cbm cbm`
- `git diff --check`
- `make -f Makefile.cbm test-foundation`
- `make -f Makefile.cbm test`
- isolated workspace smoke: artifact-only repo skipped, code repo indexed
- isolated MCP initialize smoke: `workspace_auto_index=true` with temp config created the expected repo DB
- lock smoke: live PID lock reports `workspace_index_lock_busy`; stale PID lock is removed and apply succeeds
- real workspace preview after manual population: `repositories=73`, `already_indexed=72`, `no_indexable_files=1`, `lock_busy=false`
- real startup auto-index smoke after cross-process lock and watcher hardening still showed unsafe multi-GB MCP growth, so installed `workspace_auto_index` was disabled pending a stricter singleton or batch policy

## Required Follow-Up | Memory Management Safeguards

Startup workspace auto-indexing must remain disabled in the installed runtime until a follow-up slice implements and validates explicit memory safeguards.

Required safeguards:

- Add a singleton workspace-index owner, not merely a per-apply lock, so ordinary agent-launched MCP server processes cannot each perform broad workspace maintenance.
- Add a batch limit for startup auto-indexing, for example a small maximum number of new repositories per MCP start, with continuation deferred to later starts or an explicit operator command.
- Add a memory gate before each repository index:
  - check available RAM from the platform/cgroup view
  - check swap pressure where available
  - skip or abort broad indexing when available memory is below a conservative threshold
- Add a per-repository memory-risk score based on indexable source count and previous observed RSS when available.
- Prefer manual `workspace-index --apply` for initial population and keep startup auto-index focused on small incremental catch-up.
- Release `_workspace_index.lock` reliably on normal exits and clean stale locks on restart, which is already implemented but must stay covered by tests.
- Do not broadly register all workspace repositories with every MCP watcher. Broad watcher behavior needs a singleton watcher owner, per-repo lease, or a separate managed daemon before it can be enabled.
- Add operator-facing status output that distinguishes:
  - indexed
  - eligible but deferred by batch cap
  - skipped by memory gate
  - skipped by active lock
  - skipped by source-count policy
- Add validation that starts multiple MCP server processes with `workspace_auto_index=true` under an isolated cache and proves:
  - at most one workspace maintenance job runs
  - memory remains below the configured threshold
  - the second process exits or skips cleanly
  - no stale lock remains

Acceptance criteria for re-enabling installed startup auto-index:

- Real `~/workspace.local` startup smoke with at least two concurrent MCP initializes keeps each MCP process small after initialization.
- No process grows into multi-GB RSS during idle startup when all eligible repositories are already indexed.
- `free -h` shows no new swap pressure after the smoke.
- `workspace_auto_index=true` is enabled only after the above smoke passes on the installed binary.
