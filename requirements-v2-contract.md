# Advanced Timer V2 Requirements Contract

Date: February 26, 2026
Version: 2.0.0-draft
Status: Authoritative baseline for production rewrite
Audience: Firmware, Portal, QA, DevOps

## 1. Purpose

This document is the single source of truth for Advanced Timer V2 behavior.
It defines the functional and non-functional contract for all card families and runtime services.

This contract is intentionally self-contained. Implementation must not depend on legacy code or prior documents to infer behavior.

## 2. Product Intent and Non-Goals

## 2.1 Product intent

Advanced Timer V2 is a deterministic, field-configurable controller with:

- fixed scan-based evaluation,
- schema-driven configuration,
- safe runtime control tools (force/mask/step/breakpoint),
- auditable configuration lifecycle,
- predictable behavior under load and faults.

## 2.2 Non-goals

- No text scripting language.
- No runtime code execution.
- No arbitrary user-defined expressions.
- No dynamic card reordering at runtime.
- No portal/network dependency for deterministic scan execution.

## 3. Terms

- `Card`: Deterministic processing unit with typed inputs/outputs.
- `Scan`: One complete ordered evaluation pass of all enabled cards.
- `Logical state`: Result of card logic before force/mask output policies.
- `Effective state`: Final state after force/mask policies.
- `Staged config`: Candidate config pending validate/commit.
- `Active config`: Running config currently applied.
- `LKG`: Last known good committed config.

## 4. System Architecture Contract

## 4.1 Core ownership

- Core0 owns deterministic kernel state and scan execution.
- Core1 owns portal/network/filesystem/control transport.
- Core1 communicates with Core0 only through bounded command/snapshot channels.

## 4.2 Layer boundaries

Required logical boundaries:

- `kernel`: card evaluation, topology, scan scheduler.
- `runtime`: snapshots, metrics, fault state.
- `control`: command validation and dispatch.
- `storage`: config schema persistence, migration, commit/rollback.
- `portal`: HTTP/WebSocket endpoints and UI payload mapping.
- `platform`: board IO adapters, time source, watchdog primitives.

Forbidden coupling:

- Kernel must not directly call portal/network/filesystem functions.

## 5. Determinism and Timing Contract

## 5.1 Scan timing targets

- Nominal production scan interval: `10 ms`.
- Allowed configurable scan interval range: `10..1000 ms`.
- Jitter and overrun budgets must be explicitly configured and exposed in diagnostics.

## 5.2 Deterministic execution rules

- Card evaluation order is fixed by ascending card ID.
- One scan produces one immutable snapshot revision.
- Evaluation topology is built at validate/commit time, never rebuilt during scan.
- All per-scan work is bounded; unbounded loops/allocations are prohibited in kernel path.

## 5.3 Run modes

Supported run modes:

- `RUN_NORMAL`: evaluate all enabled cards every scan.
- `RUN_SLOW`: evaluate as normal using slower configured scan interval.
- `RUN_STEP`: evaluate exactly one card evaluation step per user step command.
- `RUN_BREAKPOINT`: halt at configured breakpoint boundary; continue on command.

## 6. Card Families and Shared Contract

## 6.1 Mandatory card families

- `DI` (Digital Input)
- `AI` (Analog Input)
- `SIO` (Soft IO / virtual channel)
- `DO` (Digital Output)
- `MATH` (deterministic numeric compute)
- `RTC` (time and schedule source)

## 6.2 Common card fields

Every card config must include:

- `cardId` (stable integer ID)
- `cardType` (enum)
- `enabled` (bool)
- `label` (display string)
- `faultPolicy` (enum)
- card-specific parameter block

Every card runtime state must include:

- `cardId`
- `health` (`OK|WARN|FAULT`)
- `logicalState` (typed by card)
- `effectiveState` (typed by card)
- `lastEvalUs`
- `faultCounters` (map)

## 7. Card-Specific Requirements

## 7.1 DI (Digital Input)

Config requirements:

- physical channel mapping
- filter/debounce parameters
- optional edge mode configuration
- optional set/reset condition blocks when latch behavior is enabled

Runtime requirements:

- raw physical read
- filtered state
- logical state
- effective state (after force)
- edge/trigger indicators

Force modes:

- `REAL`
- `FORCED_HIGH`
- `FORCED_LOW`

Rules:

- Debounce/filter behavior must be deterministic across scans.
- Force transitions must not corrupt edge detection counters.

## 7.2 AI (Analog Input)

Config requirements:

- physical channel mapping
- input range and engineering units
- optional EMA/filter
- optional hysteresis thresholds
- out-of-range policy (`CLAMP` or `FAULT_DEFAULT`)

Runtime requirements:

- raw sample
- scaled value
- filtered value (if enabled)
- quality flag (`GOOD|CLAMPED|INVALID`)
- logical threshold result when applicable

Force modes:

- `REAL`
- `FORCED_VALUE`

Rules:

- Type and unit metadata must be preserved through snapshots.
- Invalid forced values must be rejected by command validation.

## 7.3 SIO (Soft IO)

Config requirements:

- initial default value
- write policy (which roles/commands may write)

Runtime requirements:

- current value
- source of last write (`KERNEL|COMMAND|RESTORE`)
- write timestamp

Rules:

- SIO is virtual and has no physical pin dependency.
- Unauthorized writes are rejected and audited.

## 7.4 DO (Digital Output)

Config requirements:

- physical channel mapping
- optional delay/min-on/min-off parameters
- optional latch/reset blocks

Runtime requirements:

- logical output result
- masked output result
- physical drive state
- set/reset active indicators
- reset-dominance indicator

Mask modes:

- `DIRECT`
- `MASKED`

Rules:

- Reset precedence is mandatory: if set and reset are both true in same evaluation, reset wins.
- Masking suppresses physical drive only; logical evaluation still executes.

## 7.5 MATH

Config requirements:

- operator enum from bounded set:
  - `ADD`, `SUB`, `MUL`, `DIV`, `MIN`, `MAX`, `CLAMP`
- ordered input references (typed)
- constants where operator requires literals
- divide-by-zero fallback policy and fallback value

Runtime requirements:

- resolved input values
- computed value
- validity state
- last error code (if any)

Rules:

- Only numeric types allowed.
- Divide-by-zero must never produce undefined output.
- Invalid operation sets fault and applies configured fallback deterministically.

## 7.6 RTC

Config requirements:

- timezone identifier
- DST policy mode
- schedule table (bounded entry count)
- invalid-time fallback mode

Runtime requirements:

- current normalized time
- time source status (`SYNCED|UNSYNCED|INVALID`)
- active schedule window flags
- next schedule transition timestamp (if available)

Rules:

- Time-source loss must produce deterministic fallback behavior.
- DST transitions must follow explicit configured policy; no implicit assumptions.

## 8. Variable Assignment Contract

Variable assignment is a typed binding system for card parameters.
It is not a scripting engine.

## 8.1 Binding source modes

- `CONSTANT`
- `VARIABLE_REF`

## 8.2 Allowed references

Reference types:

- `BOOL`
- `NUMBER`
- `TIME_WINDOW`
- `STATE`

Bindings must satisfy:

- type compatibility
- range compatibility
- unit compatibility (for numeric values)
- availability in topology (no forward-cycle dependency)

## 8.3 Binding lifecycle

- Defined in staged config.
- Fully validated before commit.
- Applied atomically on successful commit.
- Any invalid binding aborts whole commit transaction.

## 9. Dependency Topology Contract

- Graph is directed and acyclic.
- Nodes are card outputs and bindable parameters.
- Edges represent typed dependencies.

Validation must detect and report:

- dependency cycles
- missing references
- type mismatch
- disallowed cross-family reference
- unsupported operator input shape

On failure:

- commit blocked
- structured error list returned
- active config unchanged

## 10. Configuration and Schema Contract

## 10.1 Model separation

Required separate models:

- persistent config model
- in-memory runtime state model
- exported snapshot model

## 10.2 Schema versioning

- Config payload must include `schemaVersion`.
- Migration path must exist for every supported previous version.
- Migration must be deterministic and auditable.

## 10.3 Config lifecycle states

- `ACTIVE`
- `STAGED`
- `VALIDATED`
- `COMMITTED`
- `ROLLED_BACK`

## 11. Persistence and Power-Loss Safety

## 11.1 Storage artifacts

Required slots:

- active config
- staged config
- LKG config
- rollback slots (minimum 3)
- factory default config

## 11.2 Commit protocol (transactional)

1. Save staged payload.
2. Validate staged payload (schema + topology + semantic checks).
3. Write commit candidate with checksum/version/hash.
4. Rotate LKG/rollback slots atomically.
5. Promote candidate to active.
6. Publish commit result event.

If any step fails:

- active remains previous valid config,
- failure is logged and surfaced via API,
- rollback path remains valid.

## 12. Command and API Contract

## 12.1 Command classes

- Runtime control: run mode, step, breakpoint operations.
- IO control: force input, mask output.
- Config lifecycle: load, save staged, validate, commit, restore.
- Admin control: reboot, diagnostics reset, session management.

## 12.2 API behavior guarantees

- Every mutating command returns structured success/error payload.
- Validation errors return machine-readable codes and human message.
- Snapshot API always returns the latest complete snapshot revision.
- WebSocket event ordering must preserve revision sequence.

## 12.3 Minimum response metadata

- `requestId`
- `timestamp`
- `status`
- `errorCode` (on failure)
- `snapshotRevision` (where relevant)

## 13. Security and Authorization Contract

## 13.1 Role model

Minimum roles:

- `VIEWER`
- `OPERATOR`
- `ENGINEER`
- `ADMIN`

## 13.2 Protected operations

Authentication and authorization required for:

- config commit/restore
- force/mask writes
- variable assignment edits
- schedule/RTC policy edits
- firmware/OTA actions

## 13.3 Audit requirements

Audit record fields:

- actor identity
- role
- action
- target resource
- before/after hash or value summary
- timestamp
- result (`SUCCESS|FAILURE`)

## 14. Fault Handling and Degraded Mode

## 14.1 Fault classes

- validation fault
- runtime math fault
- RTC/time source fault
- IO read/write fault
- queue overflow/backpressure fault
- scan overrun fault
- watchdog fault

## 14.2 Fault policies

Each fault class must have configurable severity:

- `INFO`
- `WARN`
- `CRITICAL`

Critical fault behavior:

- deterministic scan continues where safe,
- unsafe outputs move to configured safe state,
- fault latched until acknowledged or cleared by policy.

## 15. Observability and Diagnostics

Minimum metrics exposed:

- scan duration current/max/avg
- jitter current/max
- overrun counter
- queue depth and high-water marks
- command latency current/max
- per-card eval time
- free memory headroom
- reboot reason

Diagnostics persistence:

- reboot reason and cumulative fault counters survive reboot.

## 16. Performance and Resource Contract

- Kernel path must avoid dynamic heap allocation per scan.
- Snapshot generation must be bounded and non-blocking to kernel.
- Portal/network load must not violate scan timing budget.
- Memory and queue limits must be explicit and monitored.

## 17. UI/Portal Contract (Protocol-level)

- Portal presents cards in deterministic ID order.
- UI never recomputes authoritative logical results.
- Runtime controls reflect actual kernel-accepted state, not optimistic local state.
- Config edits are staged until explicit commit.

## 18. Acceptance Test Contract

Each requirement in this document must map to at least one acceptance test ID.

Minimum suites:

- `AT-CORE-*`: deterministic scan and run modes.
- `AT-DI/AI/SIO/DO-*`: legacy family behavior and precedence.
- `AT-MATH-*`: operator correctness and fault fallback.
- `AT-RTC-*`: timezone/DST and unsynced-time behavior.
- `AT-BIND-*`: variable assignment type/range/unit validation.
- `AT-CFG-*`: staged/validate/commit/restore transaction safety.
- `AT-SEC-*`: authz/authn and audit trail coverage.
- `AT-REL-*`: power-loss recovery and fault-containment tests.

Release gate:

- No release candidate is accepted unless all mapped acceptance tests pass in CI and HIL smoke suite.

## 19. Backward Compatibility Policy

- DI/AI/SIO/DO behavior is default-compatible unless explicitly version-gated in schema.
- New features are opt-in and inert when unused.
- Migration must preserve equivalent behavior for legacy configs.

## 20. Change Control

- Contract changes require version bump and change log entry.
- Breaking behavior changes require major version increment.
- Any ambiguity discovered during implementation must be resolved by editing this contract before coding continues.

## 21. Immediate Follow-up Artifacts

Create and maintain:

- `docs/acceptance-matrix-v2.md`
- `docs/schema-v2.md`
- `docs/dependency-topology-rules.md`
- `docs/timing-budget-v2.md`
- `docs/fault-policy-v2.md`
- `docs/api-contract-v2.md`
