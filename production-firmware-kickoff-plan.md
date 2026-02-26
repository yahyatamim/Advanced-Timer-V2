# Production Firmware Kickoff Plan

Date: February 26, 2026
Scope: Start of product-grade firmware program for Advanced Timer V2
Input references: `firmware-rewrite-foundations.md`, `README.md`, current `src/main.cpp` state

## Program Objective

Move from PoC-capable firmware to production-ready firmware without freezing feature delivery, while preserving deterministic scan behavior and field reliability.

## Current Baseline (as of February 26, 2026)

- Codebase is concentrated in `src/main.cpp` with dual-core split already active.
- Runtime + portal APIs exist and are functional for local operation.
- Config staged/commit/restore flow exists, but hardening/atomic guarantees are incomplete.
- Default scan interval is currently 500 ms for bring-up; production target remains 10 ms.

## Governance: Definition of Done for "Production Ready"

All must be true:

- Deterministic timing: 10 ms scan target sustained within jitter budget under network/portal load.
- Persistence safety: power-loss-safe config commit with verified rollback/LKG recovery.
- Fault containment: watchdogs, reboot reasons, degraded mode, and health telemetry implemented.
- Test quality: automated unit/integration/HIL smoke tests passing in CI.
- Security minimum: authenticated portal sessions, protected config/command paths, signed OTA plan defined.
- Operability: logs/counters/snapshots adequate for field diagnosis without recompilation.

## Phase Plan

## Phase 0: Contract Freeze + Baseline Instrumentation (Week 1)

Goal: lock behavior and make regressions measurable.

Deliverables:

- Freeze product behavior contract from `README.md` + `firmware-rewrite-foundations.md` into acceptance test matrix.
- Add runtime metrics struct for:
  - scan duration
  - max scan duration
  - queue depth/high-water mark
  - command processing latency
- Define explicit jitter/overrun budget document (`docs/timing-budgets.md`).
- Establish issue labels/milestones (`P0-safety`, `P1-reliability`, `P2-feature`).

Exit criteria:

- Acceptance matrix approved.
- Metrics available in snapshot/API.
- Baseline timing report captured on target hardware.

## Phase 1: Architecture Refactor Skeleton (Weeks 2-3)

Goal: reduce coupling by introducing boundaries before large feature growth.

Deliverables:

- Introduce directory skeleton:
  - `src/kernel/`
  - `src/runtime/`
  - `src/control/`
  - `src/storage/`
  - `src/portal/`
  - `src/platform/`
- Move only interfaces first (no behavior changes):
  - card model types
  - command DTOs
  - snapshot DTOs
- Keep `main.cpp` as composition root only.

Exit criteria:

- Firmware behavior unchanged versus baseline acceptance tests.
- `main.cpp` reduced to startup/wiring.
- Deterministic loop has no direct web/filesystem dependencies.

## Phase 2: Persistence Hardening (Weeks 3-4)

Goal: make config writes safe across reset/power loss.

Deliverables:

- Two-phase commit implementation:
  - stage file
  - validation pass
  - atomic commit/slot rotate
- Checksums + schema version on persisted payload.
- Recovery rules on boot:
  - prefer valid active
  - fallback LKG
  - fallback slots
  - fallback factory
- Fault injection tests for reset during each commit step.

Exit criteria:

- No corrupt final state across forced-reset test matrix.
- Deterministic recovery path documented and tested.

## Phase 3: Reliability + Fault Containment (Weeks 4-5)

Goal: keep control loop stable under real-world faults.

Deliverables:

- Per-core watchdog integration and heartbeat cross-check.
- Reboot reason logging and fault counters persisted.
- Degraded mode policy when portal/network task misbehaves.
- Queue backpressure policy (drop/defer rules) with metrics.

Exit criteria:

- Network or portal stress cannot destabilize scan budget.
- Fault counters and reboot reasons visible in API.

## Phase 4: Test and CI Gates (Weeks 5-6)

Goal: block regressions automatically.

Deliverables:

- Host-side unit tests for operator evaluation and set/reset precedence.
- Invariant/property tests (e.g., reset dominance always enforced).
- HIL smoke suite:
  - scan jitter under load
  - force/mask behavior correctness
  - commit/restore workflow
- CI pipeline with required checks:
  - build
  - tests
  - lint/format/static checks

Exit criteria:

- Every merge requires passing gates.
- Baseline regression suite runs automatically.

## Phase 5: Security and Operational Controls (Weeks 6-7)

Goal: raise baseline for commercial deployment.

Deliverables:

- Portal authentication/session expiration.
- Role gating for sensitive commands/config commit.
- Audit log entries for config + runtime command events.
- OTA strategy doc with signing and rollback model.

Exit criteria:

- Unauthorized config/command writes blocked.
- Change history is attributable and queryable.

## First Sprint Backlog (Start Now)

Priority order:

1. Build acceptance-test matrix from current contract.
2. Add runtime timing and queue metrics to snapshot payload.
3. Define timing budget thresholds and pass/fail criteria.
4. Carve out `kernel` and `portal` interface headers (no logic move yet).
5. Add a small test harness for set/reset precedence.

## Risks and Controls

- Risk: refactor changes behavior.
  - Control: shadow validation against frozen acceptance matrix.
- Risk: timing regression from telemetry overhead.
  - Control: bounded sampling + compile-time debug levels.
- Risk: rewrite stall.
  - Control: incremental phases with measurable weekly exits.

## Immediate Next Action

Execute Phase 0 Task 1 in this repository: create `docs/acceptance-matrix.md` and derive test cases directly from the existing contract before any deep refactor.
