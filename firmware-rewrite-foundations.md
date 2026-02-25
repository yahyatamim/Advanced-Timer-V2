# Firmware Rewrite Foundations (Production-Ready Direction)

Date: February 25, 2026
Status: Planning document (not immediate implementation)

## Why this document

This captures what we should do if we rewrite the firmware from scratch for long-term product quality.
The current priority can remain PoC delivery; this is the blueprint for when we harden it into a real off-the-shelf product.

## 1. Freeze Product Contract First

What:
- Define exact runtime behavior as a contract (scan semantics, reset priority, force/mask behavior, config API).
- Convert contract rules into acceptance tests.

Why this is crucial:
- Prevents accidental behavior changes during refactors.
- Keeps portal, firmware, and testbench aligned.
- Makes new contributors productive faster because expected behavior is explicit.

## 2. Enforce Layered Architecture with Hard Boundaries

What:
- Split code into layers: `kernel`, `hal`, `runtime`, `control`, `storage`, `portal`, `platform`.
- Keep networking/filesystem out of deterministic logic code.

Why this is crucial:
- Reduces coupling and regression risk.
- Makes modules independently testable.
- Enables future replacement (for example, new web stack or board family) without rewriting kernel logic.

## 3. Use Explicit Data Models (Config vs Runtime vs Snapshot)

What:
- Separate persistent configuration model from volatile runtime state and from exported snapshot payloads.
- Add schema versioning and migration rules.

Why this is crucial:
- Avoids bugs caused by shared mutable structures.
- Makes persistence and rollback safer.
- Enables backward compatibility as firmware evolves.

## 4. Keep Deterministic Core Single-Owner and Queue-Driven

What:
- Core0 owns logic state.
- Core1 sends commands through queues and reads snapshots through bounded exchange.
- Fixed tick scheduler with jitter/overrun monitoring.

Why this is crucial:
- Protects deterministic timing under portal/network load.
- Prevents race conditions and undefined behavior.
- Makes debugging easier because control flow is predictable.

## 5. Implement Power-Loss-Safe Persistence

What:
- Two-phase commit for config updates (staged -> validate -> commit).
- Checksum/version stamps.
- Atomic rotation for LKG and rollback slots.

Why this is crucial:
- Prevents bricking/corruption after sudden power loss.
- Preserves a known-good restore path.
- Critical for field reliability and serviceability.

## 6. Add Reliability and Fault-Containment Controls

What:
- Per-core watchdogs and health heartbeats.
- Fault counters, reboot reason tracking, and degraded-mode behavior.
- Ensure network failures cannot disturb scan loop timing.

Why this is crucial:
- Moves system from "works in demo" to "survives real field conditions."
- Speeds root-cause analysis after failures.
- Increases confidence for unattended deployment.

## 7. Build Test Strategy as a First-Class Feature

What:
- Unit tests for operators/cards/timers.
- Invariant and property tests (for example, reset priority always wins).
- Hardware-in-the-loop timing tests and long-run soak tests.

Why this is crucial:
- Firmware quality is dominated by test quality over time.
- Catches regressions before hardware deployment.
- Enables safe refactoring and feature expansion.

## 8. Add Security and Operational Controls

What:
- Authentication/session controls for portal access.
- Signed OTA with rollback support.
- Audit trail for config/command changes.

Why this is crucial:
- Required for commercial deployment and trust.
- Reduces risk of unauthorized or accidental changes.
- Improves supportability and compliance posture.

## 9. Standardize Tooling, CI, and Code Health Gates

What:
- Static analysis, formatting, lint, and compile/test CI pipelines.
- Release build profiles and reproducible artifacts.
- Clear coding standards and review checklist.

Why this is crucial:
- Prevents quality drift as codebase grows.
- Makes team output consistent.
- Reduces production defects caused by process gaps.

## 10. Plan Migration Path Instead of Big-Bang Switch

What:
- Build new kernel under compatibility API first.
- Compare old vs new outputs in shadow mode.
- Cut over with rollback firmware path.

Why this is crucial:
- Lowers delivery risk.
- Keeps learning cycles short and measurable.
- Avoids "rewrite stall" where nothing ships for months.

## Feature Creep Control: Candidate Features and Gating

These features are valid for product direction, but they should enter through controlled gates so PoC scope stays stable.

### 1. RTC Clock for Scheduled Tasks

Importance:
- Enables time-based automation (daily/weekly schedules, blackout windows, maintenance windows).
- Becomes critical in many real deployments where event-driven logic is not enough.

Risks if added too early:
- Time sync and timezone/DST edge cases can destabilize behavior.
- Adds complexity to validation and troubleshooting.

PoC stance:
- Keep out of immediate PoC unless schedule automation is a demo requirement.
- Design config schema now with reserved schedule fields to avoid future breaking changes.

### 2. Power-Fail Memory Persistence

Importance:
- Protects runtime and config continuity across outages.
- Supports safer restart behavior and better post-fault recovery.

Practical priority:
- Medium for PoC, high for productization.
- As you noted, not always critical at PoC stage if config and safe defaults already recover cleanly.

PoC stance:
- Keep current config persistence and safe boot behavior.
- Defer full runtime-state persistence/journaling until hardening phase.

### 3. Remote Configuration over Internet (Domain + External Bridge)

Importance:
- Essential for fleet management and remote support.
- Reduces field service cost and response time.

Risks:
- Largest security and operational risk surface.
- Requires auth, transport security, device identity, and auditability.

PoC stance:
- Do not expose direct internet write paths in early PoC.
- First define architecture: device -> secure broker/API -> authenticated portal.

### 4. Remote Monitoring and Control (ESP32 Connectivity)

Importance:
- Strong product value: real-time visibility, remote diagnostics, remote operation.
- Natural extension of existing local portal and WebSocket model.

Risks:
- Control-path misuse can affect safety if not policy-gated.
- Needs role-based permissions, rate limiting, and robust command validation.

PoC stance:
- Prioritize remote monitoring first.
- Gate remote control behind explicit safety and authorization requirements.

## Recommended Priority Order (to minimize scope risk)

1. Remote monitoring (read-only)
2. RTC scheduling
3. Power-fail runtime persistence hardening
4. Remote configuration/control over internet

Reasoning:
- Start with lower-risk, high-visibility capability.
- Delay internet write/control features until security and reliability baselines are complete.

## Entry Criteria Before Any New Major Feature

Add a feature only when all are true:
- Deterministic scan timing is stable under test load.
- Config commit/rollback paths are verified.
- Acceptance tests cover existing behavior and pass in CI.
- Fault handling and observability (logs/counters/reboot reasons) are in place.

## Domain Strategy (advancedtimer.com)

You already own `advancedtimer.com`, which is a major advantage for productization.

Suggested domain layout:
- `api.advancedtimer.com`: device cloud API/broker endpoint.
- `app.advancedtimer.com`: operator/admin web application.
- `docs.advancedtimer.com`: product docs, API docs, onboarding.
- `fw.advancedtimer.com`: firmware release metadata and artifact distribution.

Connectivity model:
- Device initiates outbound secure connection to `api.advancedtimer.com`.
- No inbound port exposure on customer router/network.
- Local portal remains available for offline commissioning and recovery.

Phased rollout:
1. Read-only remote monitoring (status, telemetry, alarms, snapshots).
2. Remote configuration staging (validate-only path first).
3. Controlled remote commit/restore with strict authorization and audit trails.
4. Optional remote control actions with role policies and safety interlocks.

Security baseline before remote write/control:
- Device identity and certificate/token lifecycle.
- Operator authentication and role-based authorization.
- End-to-end TLS and request signing/replay protection.
- Full audit logs for config changes and runtime commands.
- Safe rollback path for both firmware and configuration.

## Practical Note: PoC First, Harden Later

Your current choice (finish PoC-level firmware first) is valid and pragmatic.

Recommended approach:
1. Keep current PoC moving to demonstrate end-to-end capability.
2. Avoid adding technical debt that conflicts with the above foundations.
3. Start production-hardening in phases once PoC behavior is stable.

## Suggested Next Step (when ready)

Create a phased hardening backlog with three tracks:
- Architecture refactor track
- Reliability + persistence track
- Test + CI + security track

This allows gradual evolution from PoC to product-grade firmware without freezing feature progress.
