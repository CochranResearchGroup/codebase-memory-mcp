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
- Existing indexed repositories discovered by the workspace policy should be registered with the watcher so git changes can keep them current while the MCP server is running.
- New repositories selected by broad workspace auto-indexing should use fast indexing by default to reduce resource impact. A user-initiated explicit index command may still use full mode.
- Do not enable real-home workspace auto-indexing in repo changes or tests without explicit operator intent and preview evidence.
- If broad indexing pressures RAM or swap, disable `workspace_auto_index`, terminate active indexing processes, and resume through preview/apply after adding a tighter concurrency or batch policy.

## Adoption Notes

Use this module for codebase-memory-mcp workspace-wide indexing, especially when indexing sibling repositories for cross-repo knowledge.

This policy intentionally distinguishes storage-heavy repositories from source-heavy repositories. Resource and artifact volume is handled by source filters and indexable-file counts, not by raw `du` size.
