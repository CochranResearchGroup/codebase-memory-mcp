# Plan 0001 | Audit Mitigation

State: CLOSED
Date: 2026-06-16

## Scope

Mitigate the concrete local audit findings from the initial source/build/install smoke of `codebase-memory-mcp`:

- default source build fails when `pkg-config` detects an incompatible `libgit2`
- production binary links with an executable stack because `vendored/nomic/code_vectors_blob.S` lacks a non-executable stack note
- `install.sh` allows checksum verification to fail open when `checksums.txt` cannot be fetched or matched
- install/update process detection uses `pgrep -x codebase-memory-mcp`, which is ineffective and noisy on Linux
- documented `test-foundation` target fails to link

## Non-Goals

- Do not install this MCP into the operator's real agent config.
- Do not run broad release publishing, update, or package-manager workflows.
- Do not rework the product architecture, graph schema, language coverage, or benchmark claims beyond direct documentation corrections if required by the fixes.

## Current State

- Policy adoption is a first clean adoption using the `standalone-library` profile.
- A production binary builds successfully only when `LIBGIT2_FLAGS=` and `LIBGIT2_LIBS=` are forced empty on this workstation.
- Isolated indexing/search/trace smoke passes on a tiny Python repo.
- Full test coverage has not been run; the advertised `test-foundation` target currently fails at link time.

## Work Items

1. Harden optional `libgit2` detection so incompatible libgit2 installations do not break the default source build.
2. Mark the embedded Nomic vector assembly blob as non-executable-stack on ELF platforms.
3. Make installer checksum verification fail closed for missing checksum files, missing archive entries, or unavailable local hash tools.
4. Replace ineffective Linux process matching with a long-command-safe process lookup.
5. Repair the `test-foundation` target or its test runner composition so the documented fast test target works.
6. Record any direct documentation correction needed for the changed behavior.

## Acceptance Criteria

- `make -f Makefile.cbm clean-c && make -f Makefile.cbm cbm` succeeds without manually disabling libgit2.
- `readelf -W -l build/c/codebase-memory-mcp` shows `GNU_STACK` without execute permission.
- A local installer negative test proves missing or mismatched checksums stop installation before extraction.
- Isolated `HOME` install no longer emits the Linux `pgrep -x` warning.
- `make -f Makefile.cbm test-foundation` builds and runs successfully.
- Tiny isolated repo smoke still indexes, searches, and traces successfully.

## Validation Commands

```bash
make -f Makefile.cbm clean-c
make -f Makefile.cbm cbm
readelf -W -l build/c/codebase-memory-mcp | grep GNU_STACK
make -f Makefile.cbm test-foundation
TMP_HOME=$(mktemp -d) HOME="$TMP_HOME" ./build/c/codebase-memory-mcp install --dry-run -y
```

Use additional isolated `CBM_DOWNLOAD_URL` and tiny-repo smokes for checksum and MCP behavior.

## Definition Of Done

All acceptance criteria pass or any remaining failure is documented as out of scope with concrete evidence and a follow-up plan.

## Closeout

Closed on 2026-06-16 after executing the mitigation items in scope.

Validation passed:

- `make -f Makefile.cbm clean-c && make -j"$(nproc)" -f Makefile.cbm cbm`
- `readelf -W -l build/c/codebase-memory-mcp | rg 'GNU_STACK'` showed `GNU_STACK ... RW`
- `make -f Makefile.cbm test-foundation` ran 198 passing tests
- isolated `HOME` dry-run install completed without the prior `pgrep -x` warning
- local `CBM_DOWNLOAD_URL` negative test with an archive but no `checksums.txt` exited 1 and left no extracted binary
- isolated tiny Python repository smoke indexed, searched for `compute`, and traced inbound caller `handler`

Additional validation on 2026-06-18 after tightening checksum entry matching:

- `git diff --check`
- `bash -n install.sh`
- `make -f Makefile.cbm clean-c && make -j"$(nproc)" -f Makefile.cbm cbm`
- `readelf -W -l build/c/codebase-memory-mcp | rg 'GNU_STACK'` showed `GNU_STACK ... RW`
- `make -f Makefile.cbm test-foundation` ran 198 passing tests
- isolated `HOME` dry-run install completed without the prior `pgrep -x` warning
- isolated shell installer rejected a checksum line for `codebase-memory-mcp-linux-amd64-portable.tar.gz.extra`
- isolated shell installer accepted an exact checksum line and installed a fake local test binary
- isolated C updater rejected a checksum line for `codebase-memory-mcp-linux-amd64-portable.tar.gz.extra`
- isolated C updater accepted an exact checksum line and installed a fake local test binary
- isolated tiny Python repository smoke indexed, searched for `compute`, and traced inbound caller `handler`

PowerShell syntax was not executed locally because `pwsh`/`powershell` is not installed in this environment.
