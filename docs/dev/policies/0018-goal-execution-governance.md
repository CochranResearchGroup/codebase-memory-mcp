# Policy | Goal Execution Governance

## Policy

- Apply this policy when autonomous work is expected to span multiple bounded
  slices, context windows, sessions, or human/runtime gates.
- Preserve the user-approved objective as the stable goal contract. Do not
  silently narrow, expand, or rewrite it to match the work already completed.
- Treat that approved goal as standing authority for ordinary in-envelope
  implementation, validation, repair, retest, worker replacement, integration,
  and bounded successor packets. A packet hard stop ends that execution window;
  it does not revoke the approved goal or create a new approval gate by itself.
- Ask for new authorization only for a significant departure: changing the
  objective, acceptance criteria, or non-goals; adding a new system, tenant, or
  private-data class; widening mutation scope; taking a destructive, external,
  legal, financial, publication, release, or public action; materially raising
  cost or resource ceilings; weakening a safety control; or continuing after
  repeated no-progress evidence. Preserve any stricter explicit human,
  runtime, security, provider, or live-operation gate.
- Allow the campaign plan to stay high-level and derive bounded execution
  packets just in time under `planning-discipline`.
- Model execution as explicit states and transitions even when no graph
  framework is used. At minimum distinguish ready, active, awaiting-review,
  awaiting-gate, blocked, complete, failed, and cancelled states.
- Use `parallel-plan-design` to make dependencies, fan-out, joins, and retry
  edges inspectable. Every feedback cycle that can repeat model calls, tool
  calls, agent runs, mutations, or context growth must have one named
  controller, a semantic exit condition, and a hard bound.
- Treat material replanning as a new plan version or bounded successor packet.
  Preserve what changed and why instead of mutating execution history in place.
- Before execution, record the current authority, unmet acceptance criteria,
  owned worktree scope, current evidence, ready work units, blocked units,
  delegation plan, checkpoint cadence, and human/runtime/security gates.
- Choose concrete bounds before starting: work-unit attempts, review/rework
  cycles, consecutive hardening/no-progress checkpoints, and maximum time,
  slices, or available runtime budget between durable checkpoints. If one
  metric is unavailable, another observable bound must still cover the loop.
  Treat these as renewable execution windows when the latest checkpoint proves
  outcome progress or blocker reduction and the approved envelope is unchanged.
  Bounds prevent runaway work; they are not consumable approval tokens.
- Keep one primary orchestrator responsible for authority, the critical path,
  work-unit selection, integration, progress classification, and the final
  completion claim.
- Apply `subagent-workflow-optimization` at each execution packet and record the
  delegation decision. Apply `validation-and-handoff` for independent review
  and final outcome verification.
- At every durable checkpoint, compare the current state with the prior
  checkpoint and classify movement as:
  - `outcome_progress`: current evidence advances an acceptance criterion
  - `blocker_reduction`: a verified blocker or material risk was removed
  - `hardening`: resilience improved without changing acceptance state
  - `no_progress`: the goal state did not materially change
  - `regression`: evidence, safety, or alignment worsened
- Checkpoint after each validated execution packet and before context handoff,
  risky mutation, independent audit, human gate, or closeout. Record owned
  changes, validation evidence, state transitions, remaining criteria, and the
  next ready unit or exact stop reason in a durable repo artifact.
- Stop autonomous execution when any configured drift guard fires, including:
  repeated hardening without outcome movement; repeated failure on the same
  invariant; stale evidence being reused for a current claim; an oversized or
  an unsafe or unowned dirty worktree; a required human/runtime/security gate;
  or remaining work that is unbounded polish rather than goal capability.
- A failed closed-world verification of an accepted blocking finding transitions
  the unit to split, reframe, block, or escalation; it does not silently reopen
  an unbounded review cycle.
- Allow one broad fresh-context drift-discovery pass per approved goal
  objective. After the primary adjudicates its candidate findings, verification
  is closed-world against the accepted finding ledger plus critical regressions
  introduced by remediation. Plan versions and successor packets inherit this
  discovery budget rather than resetting it.
- Stop autonomous execution when any configured drift guard fires, including:
  repeated hardening without outcome movement; repeated failure on the same
  invariant; stale evidence being reused for a current claim; an oversized or
  cyclic unit without a covering bound; an unresolved adjudicated blocking finding;
  an unsafe or unowned dirty worktree; a required human/runtime/security gate;
  or remaining work that is unbounded polish rather than goal capability.
- Continue automatically under standing authority when the latest checkpoint
  shows outcome progress or verified blocker reduction and names a bounded
  ready unit inside the approved envelope. Otherwise close, block, cancel, or
  request authorization while citing the exact significant departure or
  pre-existing gate that requires it.
- Completion requires current evidence for every acceptance criterion. Token
  spend, elapsed time, test count, schema growth, documentation volume, and
  completed slice count are not completion evidence by themselves.

## Adoption Notes

Use this module for repos that run `/goal`, unattended campaigns, multi-session
agent work, or other long-horizon autonomous execution.

Before calling adoption complete, adopting repos must define concrete checkpoint
and drift thresholds plus the required checkpoint-record fields in repo-local
policy. When a deterministic planning/runbook audit exists, extend it to verify
goal-plan versioning, checkpoint identifiers, progress classification, and the
configured bounds. Keep exact token counters, time windows, command names, and
runbook schemas repo-local.

Use a machine-checkable repo-local section such as:

```text

## Local Goal Bounds
max_work_unit_attempts: 2
max_review_rework_cycles: 1
max_hardening_checkpoints: 2
checkpoint_interval: 1 slices
authorization_gate: significant_departure_only
retry_budget_mode: renewable_execution_window
review_discovery_passes: 1
review_verification_mode: closed_world
review_finding_fields: criterion, evidence, consequence, reproducer, confidence, suggested_disposition
review_disposition_values: blocking | nonblocking_backlog | rejected | needs_evidence
checkpoint_record_fields: plan_version, state_transition, progress_classification, evidence, subagent_status, authority_classification, review_disposition_summary, next_action_or_stop_reason
```

The selector bundle's planning auditor supports `--goal-only` to verify this
contract without requiring roadmap/runbook governance.

Recommended companion modules:

- `planning-discipline`
- `parallel-plan-design`
- `subagent-workflow-optimization`
- `validation-and-handoff`
- `commit-and-push-cadence`
