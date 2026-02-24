# Configuration Portal Implementation Contract

Version: 1.1
Status: Working Contract
Applies to: AdvancedTimer Configuration Portal and runtime integration

## 1. Purpose

This document is the contract for how the Configuration Portal must look, behave, and integrate with the AdvancedTimer kernel.

The contract preserves these foundations:
- No-code, structured LogicCard configuration.
- Deterministic runtime transparency.
- Mobile-first field usability.
- Safe validation via test mode workflows.
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

## 3. LogicCard Structural Contract

### 3.1 List Model

LogicCards MUST be presented as a flat, fixed, ordered list.

Rules:
- Order MUST match firmware scan order.
- Reordering is not allowed.
- Card identity and order must be deterministic across sessions.

### 3.2 Card Layout (Five Sections)

Each card MUST expose exactly these sections:
1. Configuration (editable)
2. Live State (read-only)
3. Timing Diagram (analytical preview)
4. Simulation Controls (test mode only)
5. Debug Indicators

## 4. Card Section Contracts

### 4.1 Configuration Section (Editable)

Configuration fields MAY include, based on card family/capability:
- Parameters
- Thresholds
- Filter (EMA for AI, when enabled)
- Scaling (AI, when enabled)
- Aggregator (`AND` / `OR`)
- `SET` block (if supported)
- `RESET` block (if supported)

Rules:
- No nested logic structures.
- If a card supports latch semantics, both `SET` and `RESET` blocks MUST exist.
- Edits are staged only until explicit commit.

### 4.2 Live State Section (Read-only)

Live State MUST show runtime snapshot values produced by kernel evaluation, without UI-side recomputation.

Fields:
- Physical state
- Logical state
- Effective state (after force)
- Per-condition `TRUE/FALSE`
- Aggregated result
- `SET` result
- `RESET` result
- Final state
- Mask/force indicators

Portal MUST NOT recompute logic values shown in this section.

### 4.3 Timing Diagram Section (Analytical Preview)

Purpose: deterministic analytical timing preview for one card.

Rules:
- Derived only from card configuration parameters.
- Not driven by live runtime state.
- Not virtual time.
- Not a simulation engine.
- Must not affect kernel state or scheduling.

The diagram MUST:
- State trigger assumption explicitly (example: input `TRUE` at `t=0`).
- Show delay times.
- Show pulse widths.
- Show minimum on/off enforcement.
- Show latch behavior when applicable.
- Show reset dominance when applicable.

For AI cards, diagram SHOULD visualize:
- Threshold crossing behavior.
- Hysteresis band behavior.

For timer-oriented cards, diagram SHOULD visualize:
- Time axis.
- ON delay.
- OFF delay.
- Pulse window (if configured).

This section is analytical and static, not simulated.

### 4.4 Simulation Controls Section (Test Mode Only)

This section MUST be visible only when test mode is active.

Controls:
- DI/AI: force toggle, forced value, forced badge.
- DO: output mask toggle, masked badge.

Rules:
- Same kernel and same real-time semantics as normal run.
- No virtual clock.
- Logical evaluation is unchanged.
- Physical output drive is suppressed only when masked.

### 4.5 Debug Indicators Section

Per card indicators:
- Evaluation pulse indicator
- Breakpoint toggle
- `SET` active indicator
- `RESET` active indicator
- `RESET` override indicator (when both true)

Stepping rule:
- One atomic card evaluation per step.
- No sub-card stepping.

## 5. Evaluation Rules (Authoritative)

Evaluation order for each card:
1. Condition rows
2. Aggregate
3. `SET` evaluation
4. `RESET` evaluation
5. Final resolution

Final resolution rule:
- If `RESET == TRUE`, final state = `FALSE`.
- Else if `SET == TRUE`, final state = `TRUE`.
- Else, hold previous state.

`RESET` priority is absolute.

## 6. State Model Separation (Strict)

For IO-oriented cards, these states MUST be visually and semantically distinct:
- Physical: hardware-level observed/driven value.
- Logical: engine-processed value.
- Effective: value after force application.
- Final: value after set/reset resolution.

## 7. UX Contract

### 7.1 Mobile-First Industrial Usability

The UX MUST be optimized for mobile-dominant field conditions.

The portal SHOULD:
- Minimize keyboard-heavy interactions.
- Use tactile controls (dial, stepper, segmented buttons, toggles).
- Keep large tap targets and high contrast.

### 7.2 Top-Fixed Runtime Awareness

The layout MUST keep a fixed runtime header always visible.

The fixed header SHOULD include:
- Current mode (`RUN_NORMAL`, `RUN_STEP`, `RUN_BREAKPOINT`, `RUN_SLOW`)
- Test mode state
- Alarm state
- Critical IO indicators
- Optional current card index

Card content is in a separate scrollable region below this header.

## 8. Runtime and Debug Control Contract

### 8.1 Stepping and Live Monitoring

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
- `RUN_SLOW`: same semantics as `RUN_NORMAL`, slower inter-card pacing for observation.

Live monitoring MUST be read-only and non-intrusive.

### 8.2 Breakpoint Behavior

Breakpoints are card-level only.

If breakpoint is set on card `N`:
- Engine continues until card `N` completes atomically.
- Engine then transitions to paused state.
- Portal receives a consistent snapshot.

Breakpoints MUST NOT interrupt a card mid-evaluation.

### 8.3 Safety During Test/Debug

In test mode:
- Physical outputs MUST NOT energize when masked.
- Logical states remain visible and deterministic.

## 9. Simulation/Test Mode Contract

Simulation mode provides safe validation without energizing selected physical outputs.

Simulation rules:
- Same kernel, same semantics, same real time.
- No shadow/simulation-only engine.
- No virtual clock.
- No duplicated logic semantics between run and test paths.

### 9.1 Output Masking

When test mode is active:
- Logic still computes output states normally.
- Masked outputs skip physical relay drive.
- Masking MAY be global or per-channel.

### 9.2 Input Forcing

Each input MAY be real or forced.

Effective value rule:
- `effectiveInput = isForced ? forcedValue : realValue`

Input forcing MUST NOT mutate persisted configuration unless explicitly committed as config.

### 9.3 Timing and State Preservation

Entering/exiting test mode MUST:
- Keep timers in real time.
- Preserve latch/counter/timer states.
- Avoid engine restart requirements.

### 9.4 Test Mode + Step

Test mode and step mode MAY be combined.

When combined:
- One full card evaluation per step.
- Mask/force behavior remains active.
- Timing remains real.

### 9.5 Operator Indication

When test mode is active, UI MUST clearly show persistent indicators:
- `TEST MODE ACTIVE`
- `OUTPUT MASKED`
- `INPUT FORCED`

Operators MUST always be able to distinguish real vs forced/masked IO.

## 10. Determinism Protection and Boundaries

The portal MUST protect kernel determinism:
- UI never modifies kernel memory directly.
- UI never recomputes authoritative logic.
- UI never introduces alternate timing semantics.
- Visualization is snapshot-driven.
- Dual-core boundary is respected.

## 11. Dual-Core Architecture Contract (ESP32)

Core split objective: preserve deterministic scan timing under portal/network load.

### 11.1 Core Responsibilities

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

### 11.2 Hard Boundary Rules

Core 1 MUST NOT:
- Execute logic directly.
- Modify logic memory directly.
- Interrupt mid-card execution.
- Block deterministic scan.

Inter-core exchange MUST use a bounded, explicit mechanism (queue, ring buffer, or double-buffer snapshot).

## 12. Firmware Baseline Constraints (Current `main.cpp`)

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

## 13. Configuration Workflow Contract

The portal MUST separate:
- Staged configuration
- Active runtime configuration

Commit workflow MUST be:
1. Edit staged configuration.
2. Validate schema and constraints.
3. Optionally validate behavior analytically (timing preview).
4. Optionally run test mode validation.
5. Commit full configuration payload.
6. Persist and confirm active state.

On commit failure:
- Active configuration MUST remain unchanged.
- Error reason MUST be shown.
- Staged data SHOULD remain recoverable.

## 14. Non-Goals

This contract does not allow:
- Time rewind.
- State rollback.
- Full-system virtual simulation engine.
- Sub-card stepping granularity.
- Changing logic semantics between normal/debug/test.

## 15. Future Extensions (Allowed)

Allowed extensions, if they preserve this contract:
- Visual dependency tracing.
- Decision "why" introspection hints.
- Execution trace logging.
- Per-card execution timing metrics.
- Signature overlays.
- Additional timing-diagram visual styles (for example, minimalist or waveform grid) without changing analytical semantics.

All extensions MUST preserve:
- Single-kernel semantics.
- Deterministic order.
- Real-time behavior.
- Safety-first operation.

## 16. Acceptance Criteria

Portal implementation is contract-compliant when all are true:
- No-code, parameter-only configuration flow exists.
- LogicCard UI is a fixed, firmware-ordered list.
- Each card exposes the five required sections.
- Live State is read-only and kernel-authoritative.
- Per-card timing diagram is analytical/static, not simulated.
- Step/breakpoint behavior is card-atomic and deterministic.
- Test masking/forcing is real-time and clearly indicated.
- State model separation (physical/logical/effective/final) is explicit.
- Dual-core boundary rules are enforced.
- Staged vs active configuration workflow is explicit and safe.
