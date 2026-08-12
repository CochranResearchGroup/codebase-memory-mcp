# Policy | Workspace Auto-Index Governance

## Policy

- Treat broad workspace indexing as an opt-in operator feature, separate from per-session auto-indexing.
- Do not recursively index arbitrary home-directory contents. Workspace auto-indexing must use explicit allowlisted roots such as `~/workspace.local`.
- Discover Git repositories, not generic directories. A root may itself be a repository, and direct child repositories under an allowlisted root may be discovered.
- Gate eligibility by tracked, indexable source files rather than repository disk size. Storage-heavy resources, artifacts, media, databases, model files, archives, fixtures, and generated data should not by themselves make a repository ineligible.
- Count only tracked files that pass the same source-oriented filtering used for indexing mode decisions:
  - ignore hard-skip directories such as `.git`, dependency directories, build outputs, caches, virtual environments, coverage, vendor, and generated artifact trees
  - ignore binary, media, archive, database, model, and temporary suffixes
  - require a supported source/config/documentation language classification before counting a file as indexable
- Keep a conservative default per-repository indexable source limit. Repositories above the limit must be skipped with an explicit reason unless the operator raises the limit intentionally.
- Provide a preview path that reports discovered repositories, tracked count, indexable source count, current index status, and skip reason before broad apply.
- Background workspace auto-indexing must be non-blocking for MCP startup and must respect both in-process and cross-process indexing locks. If another indexing pipeline or workspace apply is active, skip or defer rather than competing for memory.
- Broad workspace auto-indexing must assume multiple MCP server processes may be launched by different agents. The lock must be stored in shared runtime/cache state, not only in process memory.
- Existing indexed repositories discovered by the workspace policy must not all be registered with every agent-launched MCP process by default. Broad watcher registration can make multiple MCP processes reindex dirty repositories in parallel and exhaust RAM.
- Use per-session watcher registration for the active repository. Broad workspace watcher ownership requires a singleton owner or equivalent cross-process lease before it can be enabled safely.
- New repositories selected by broad workspace auto-indexing should use fast indexing by default to reduce resource impact. A user-initiated explicit index command may still use full mode.
- Do not enable real-home workspace auto-indexing in repo changes or tests without explicit operator intent and preview evidence.
- Do not leave `workspace_auto_index=true` in the installed runtime if startup smokes show multi-GB MCP server growth, retained locks, or swap pressure. In that case, keep `workspace_roots` configured and use manual preview/apply until startup auto-indexing has a stricter singleton or batch design.
- If broad indexing or broad watcher registration pressures RAM or swap, disable `workspace_auto_index`, terminate active indexing processes, and resume through preview/apply after adding a tighter concurrency, batch, or singleton-watcher policy.
- Give each parallel lane a clear owner, bounded scope, and expected write surface.
- Keep the critical path visible so parallel work does not hide the real blocker.
- Prefer plan slices that minimize cross-lane file overlap and reconciliation cost.
- Call out integration points explicitly when multiple lanes must converge before completion.
- Express non-trivial execution as inspectable work units and dependency edges,
  including fan-out, join, review, retry, and terminal transitions. A table or
  plan section is sufficient; a graph framework is not required.
- Do not open parallel lanes just because tools allow delegation; open them only when the work can move independently.
- If a lane becomes coordination-heavy, collapse it back into the critical path or redefine the lane boundary.
- Declare the intended active-agent concurrency before spawning many subagents or parallel workers.
- Cap active subagents per plan lane unless the repo explicitly optimizes for `max-dev-speed` and has strong reconciliation rules.
- Avoid nested subagents by default.
- Use nested or orchestrator subagents only when the plan names the parent orchestration role, child scopes, result-flow path, and synthesis responsibility.
- Treat high fan-out as a plan smell unless the subtasks are independent, low-conflict, and cheap to verify.
- Put a semantic exit condition and a hard bound on every review, retry, repair,
  or agent-handoff edge that can cycle back to prior work.
- Reaching a local loop bound ends or reframes that loop; it does not create a
  user-approval gate by itself. Continue another safe in-scope route when one is
  available, and escalate only when no meaningful route remains or an exact
  action-specific boundary requires a user decision.
- When a work unit cannot be bounded or has too many coupled write surfaces,
  return it for split/reframe before spawning workers.

## Adoption Notes

Use this module for codebase-memory-mcp workspace-wide indexing, especially when indexing sibling repositories for cross-repo knowledge.

This policy intentionally distinguishes storage-heavy repositories from source-heavy repositories. Resource and artifact volume is handled by source filters and indexable-file counts, not by raw `du` size.
