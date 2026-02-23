# Configuration Portal Implementation Contract

Version: 1.0
Status: Working Contract
Applies to: AdvancedTimer Configuration Portal and runtime integration

## 1. Purpose

This document is the contract for how the Configuration Portal must look, behave, and integrate with the AdvancedTimer kernel.

The contract preserves these foundations:
- No-code, structured LogicCard configuration.
- Deterministic runtime transparency.
- Mobile-first field usability.
- Safe validation via simulation and dry-run workflows.
- Strict architectural boundaries between UX/networking and kernel execution.

## 2. Product Philosophy (Non-Negotiable)

The portal is a field-configurable smart controller interface, not a general PLC IDE.

The system MUST remain:
- Parameter-driven.
- Schema-validated.
- Deterministic.
- Safe for commissioning.

The system MUST NOT introduce:
- Text scripting.
- Free-form expressions.
- Runtime code execution.
- Alternate execution semantics hidden from operators.

## 3. UX Contract

### 3.1 Mobile-First Industrial Usability

The UX MUST be optimized for mobile-dominant use in field conditions.

The portal SHOULD:
- Minimize keyboard-heavy interactions.
- Use tactile controls (dial, stepper, segmented buttons, toggles).
- Keep large tap targets and high contrast.

### 3.2 Top-Fixed Runtime Awareness

The UI layout MUST keep a fixed runtime header always visible.

The fixed header SHOULD include:
- Current mode (`RUN`, `STEP`, `SIMULATION`).
- Dry-run/test state.
- Alarm state.
- Critical IO indicators.
- Optional current card index.

Card configuration controls MUST be in a separate scrollable region below this header.

### 3.3 Deterministic Timing Preview (Per Card)

Each configurable timing card SHOULD expose a local analytical timing preview.

Preview rules:
- It is per-card, isolated, read-only.
- It is computed from configuration parameters only.
- It does not run kernel logic.
- It does not virtualize time.
- It does not alter scan execution.

The preview MUST state trigger assumptions clearly (for example, input true at `t=0`).

## 4. Runtime & Debug Control Contract

## 4.1 Stepping and Live Monitoring

Stepping is execution control, not simulation.

Stepping rules:
- Evaluation granularity MUST be whole LogicCard only.
- No partial card execution is allowed.
- Evaluation order MUST match normal deterministic order.
- Same runtime kernel MUST be used in normal and debug modes.
- No virtual clock or alternate timing engine is allowed.

Execution mode model:
- `RUN_NORMAL`: continuous scan at fixed interval.
- `RUN_STEP`: evaluate one card per user step.
- `RUN_BREAKPOINT`: pause after configured card completes.
- `RUN_SLOW`: Similar to run normal Mode but between card timing will be longer

Live monitoring MUST be read-only and non-intrusive.

### 4.2 Breakpoint Behavior

Breakpoints are card-level only.

If breakpoint is set on card `N`:
- Engine continues until card `N` completes atomically.
- Engine then transitions to paused state.
- Portal receives a consistent snapshot.

Breakpoints MUST NOT interrupt a card mid-evaluation.

### 4.3 Safety During Debug

In dry-run:
- Physical outputs MUST NOT energize.
- Logical states remain visible.

## 5. Simulation/Test Mode Contract

Simulation mode provides safe validation without energizing selected physical outputs.

Simulation rules:
- Same kernel, same semantics, same real time.
- No shadow/simulation-only engine.
- No virtual clock.
- No logical-vs-physical state duplication layer.

### 5.1 Output Masking

When simulation is active:
- Logic still computes output states normally.
- Masked outputs skip physical relay drive.
- Masking MAY be global or per-channel.

### 5.2 Input Forcing

Each input MAY be real or forced.

Effective value rule:
- `effectiveInput = isForced ? forcedValue : realValue`

Input forcing MUST NOT mutate persisted configuration unless explicitly committed as config.

### 5.3 Timing and State Preservation

Entering/exiting simulation MUST:
- Keep timers in real time.
- Preserve latch/counter/timer states.
- Avoid engine restart requirements.

### 5.4 Simulation + Step

Simulation and step mode MAY be combined.

When combined:
- One full card evaluation per step.
- Mask/force behavior remains active.
- Timing remains real.

### 5.5 Operator Indication

When simulation is active, UI MUST clearly show persistent test indicators, including:
- `TEST MODE ACTIVE`
- `OUTPUT MASKED`
- `INPUT FORCED`

Operators MUST always be able to distinguish real vs simulated IO.

## 6. Dual-Core Architecture Contract (ESP32)

Core split objective: preserve deterministic scan timing under portal/network load.

### 6.1 Core Responsibilities

Core 0 (deterministic engine) is responsible for:
- Fixed scan scheduler.
- LogicCard evaluation.
- Timer updates.
- IO abstraction/drive.
- Step/breakpoint execution semantics.
- Simulation mask enforcement.
- Snapshot production.

Core 1 (portal/network) is responsible for:
- Web server and WebSocket.
- JSON handling.
- UI interaction.
- Configuration staging.
- Visualization and OTA-related portal functions.

### 6.2 Hard Boundary Rules

Core 1 MUST NOT:
- Execute logic directly.
- Modify logic memory directly.
- Interrupt mid-card execution.
- Block deterministic scan.

Inter-core exchange MUST use a bounded, explicit mechanism (queue, ring buffer, or double-buffer snapshot).

## 7. Firmware Baseline Constraints (Current `main.cpp`)

Current firmware facts that portal behavior MUST respect:
- Scan interval is `10 ms`.
- Config persistence path is `/config.json`.
- Card families: `DigitalInput`, `DigitalOutput`, `AnalogInput`, `SoftIO`.
- Deterministic family scan order:
  1. DI
  2. AI
  3. SIO
  4. DO

The portal MUST treat runtime constants and enum names as authoritative from firmware.

## 8. Configuration Workflow Contract

The portal MUST separate:
- Staged configuration.
- Active runtime configuration.

Commit workflow MUST be:
1. Edit staged configuration.
2. Validate schema and constraints.
3. Optionally validate behavior analytically (timing preview).
4. Optionally run simulation test.
5. Commit full configuration payload.
6. Persist and confirm active state.

On commit failure:
- Active configuration MUST remain unchanged.
- Error reason MUST be shown.
- Staged data SHOULD remain recoverable.

## 9. Architectural Boundary Rule

UX and kernel boundaries are strict:
- UI preview logic MUST NOT depend on runtime engine internals.
- Portal UI state MUST NOT directly manipulate kernel memory.
- Portal actions MUST go through an explicit command interface.
- Kernel determinism MUST NOT depend on UI rendering/network load.

## 10. Non-Goals

This contract does not allow:
- Time rewind.
- State rollback.
- Full-system virtual simulation engine.
- Sub-card stepping granularity.
- Changing logic semantics between normal/debug/simulation.

## 11. Future Extensions (Allowed)

Allowed extensions, if they preserve this contract:
- Visual dependency tracing.
- Decision "why" introspection hints.
- Execution trace logging.
- Per-card execution timing metrics.
- Signature overlays.

All extensions MUST preserve:
- Single-kernel semantics.
- Deterministic order.
- Real-time behavior.
- Safety-first operation.

## 12. Acceptance Criteria

Portal implementation is contract-compliant when all are true:
- No-code, parameter-only configuration flow exists.
- Mobile-first UX and top-fixed runtime header are implemented.
- Analytical per-card timing preview is isolated from runtime engine.
- Step/breakpoint behavior is card-atomic and deterministic.
- Simulation masking/forcing is real-time and clearly indicated.
- Dual-core boundary rules are enforced.
- Staged vs active configuration workflow is explicit and safe.
- Runtime visualization is read-only and non-intrusive.
