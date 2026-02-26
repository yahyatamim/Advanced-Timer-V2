# Configuration Portal Implementation Contract

Version: 1.1
Status: Working Contract
Applies to: AdvancedTimer Configuration Portal and runtime integration

## End-of-Day Update (February 25, 2026)

Today:
- Maintained dual-core runtime split (`Core0` deterministic engine, `Core1` portal/network).
- Kept HTTP/WebSocket portal transport and runtime command channel active.
- Continued using staged config lifecycle endpoints (`load`, `save`, `validate`, `commit`, `restore`).
- Kept basic `/config` and `/settings` UIs connected to backend APIs for bring-up.

Open for next session:
- Finish advanced config UX guardrails and editor polish.
- Implement per-card analytical timing diagram.
- Complete debug indicators and breakpoint UX refinement.
- Add security/session controls and integration/acceptance tests.

## 1. Purpose

This document is the contract for how the Configuration Portal must look, behave, and integrate with the AdvancedTimer kernel.

The contract preserves these foundations:
- No-code, structured LogicCard configuration.
- Deterministic runtime transparency.
- Mobile-first field usability.
- Safe validation via runtime force/mask workflows.
- Strict architectural boundaries between UX/networking and kernel execution.

## Implementation Checklist (Execution Order)

1. `[x]` Define contract baseline for LogicCard structure, runtime/debug/test semantics, timing-preview boundaries, and settings/admin boundaries.
2. `[x]` Freeze firmware-portal interface schema (`snapshot`, `commands`, `staged config payload`, `commit/restore responses`, error model). See Section 20.
3. `[x]` Implement read-only runtime snapshot API/WebSocket stream for header + per-card Live State. HTTP + WebSocket runtime snapshot transport are in place.
4. `[~]` Implement fixed ordered card list UI bound to firmware scan order (no reorder path). Ordered list rendering exists (`/`, `/config`), but full section-complete card UX is pending.
5. `[~]` Implement staged configuration editor per card with schema validation and explicit commit flow. Backend config lifecycle endpoints are in place and a basic card editor page is available at `/config`; advanced UX/guardrails still pending.
6. `[x]` Implement kernel command interface for step, run mode, breakpoints, force/mask controls (command-gated, no direct memory writes). HTTP + WebSocket command transport are in place.
7. `[ ]` Implement per-card analytical timing diagram renderer (static/parameter-derived only, with explicit trigger assumptions).
8. `[ ]` Implement debug indicators (evaluation pulse, set/reset active, reset override, breakpoint toggle).
9. `[~]` Implement settings page system administration features (WiFi strategy, security controls, config history, system controls, back/home navigation). Core1 WiFi policy scaffold + basic settings page and endpoints are in place (`/settings`, `/api/settings`, WiFi save/reconnect, reboot); security and config-history management remain.
10. `[~]` Implement 3-level rollback storage operations and restore workflow with atomic commit semantics. File-backed LKG/slot rotation and restore endpoints are in place; hardening and atomicity guarantees still pending.
11. `[ ]` Add integration and acceptance tests mapped to Section 16 criteria plus Section 17 settings constraints.
12. `[ ]` Final contract audit: verify portal behavior matches this document and remove/reword stale implementation comments.

## Current Status Snapshot (February 25, 2026)

Current implementation state:
- Dual-core scaffold is active (`Core0` deterministic engine task, `Core1` portal/network task).
- WiFi fallback policy is active (Master -> User -> offline with low-frequency retry).
- Portal transport is active:
  - HTTP: `/`, `/config`, `/settings`, `/api/snapshot`, `/api/command`, `/api/config/*`, `/api/settings/*`
  - WebSocket: runtime snapshot broadcast + command/result channel on `:81`
- Runtime IO control is supported with no external IO bench:
  - input force (DI/AI) available directly from live page controls
  - output mask (per-card + global) available directly from live page controls
  - run mode controls (`RUN_NORMAL`, `RUN_SLOW`, `RUN_STEP`, `RUN_BREAKPOINT`)
  - per-card breakpoint enable/disable from live page controls
- Config lifecycle backend is implemented:
  - load active
  - staged save
  - staged validate
  - commit/deploy
  - restore (`LKG`, `SLOT1..3`, `FACTORY`)
- Basic Settings UI and Config Editor UI are implemented and connected to backend APIs.
- Config Editor UX has humanized field labels and context-aware operator labels.
- Factory defaults were updated to sensible values; set/reset reference IDs default to self-card.

Known intentionally temporary state:
- Scan interval is currently `500 ms` for bring-up/testing.
- Target production scan interval remains `10 ms` after stabilization.

Primary remaining work:
- Complete advanced config UX guardrails and polish.
- Implement per-card analytical timing diagram UI.
- Implement debug indicators UI and breakpoint UX polish.
- Add security/session control.
- Add integration/acceptance tests and perform final contract audit.

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

### 4.4 Runtime IO Controls Section

This section is visible on the live page and does not require test mode.

Controls:
- DI: `Real` / `Forced High` / `Forced Low`.
- AI: `Real` / `Forced Value`.
- DO: `Direct` / `Masked`.
- Per-card breakpoint: `Off` / `On`.

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

### 8.3 Safety During Debug

Rules:
- Physical outputs MUST NOT energize when masked.
- Logical states remain visible and deterministic.

## 9. Runtime Force/Mask Control Contract

Runtime force/mask controls provide safe validation without energizing selected physical outputs.

Control rules:
- Same kernel, same semantics, same real time.
- No shadow/simulation-only engine.
- No virtual clock.
- No duplicated logic semantics between run and test paths.

### 9.1 Output Masking

- Logic still computes output states normally.
- Masked outputs skip physical relay drive.
- Masking MAY be global or per-channel.

### 9.2 Input Forcing

Each input MAY be real or forced.

Effective value rule:
- `effectiveInput = isForced ? forcedValue : realValue`

Digital input (DI) force source modes:
- `REAL`: read actual GPIO input (`digitalRead(...)`) and continue normal processing.
- `FORCED_HIGH`: bypass GPIO read and use logical high as input source for downstream processing.
- `FORCED_LOW`: bypass GPIO read and use logical low as input source for downstream processing.

Analog input (AI) force source modes:
- `REAL`: read actual analog sample (`analogRead(...)`) and continue clamp/scale/filter pipeline.
- `FORCED_VALUE`: bypass hardware sample and use portal-provided forced numeric value as pipeline input.

When forced input is active:
- The forced source replaces only hardware acquisition.
- The same deterministic processing pipeline remains unchanged.

Input forcing MUST NOT mutate persisted configuration unless explicitly committed as config.

### 9.3 Timing and State Preservation

Changing control mode (`Real`/`Forced`/`Masked`) MUST:
- Keep timers in real time.
- Preserve latch/counter/timer states.
- Avoid engine restart requirements.

### 9.6 Mask/Force Runtime Semantics (Authoritative)

Mask/force is a built-in runtime feature of the same logic engine; it is not a separate simulation engine.

DO output mask behavior:
- If mask is enabled for an output, firmware MUST skip `digitalWrite(...)` for that output.
- Logic evaluation, state transitions, timers, counters, and snapshots MUST continue normally.
- If mask is disabled, firmware performs `digitalWrite(...)` using normal logic result.

Force behavior:
- Force affects input source selection only (DI/AI acquisition path).
- Force MUST NOT change logic semantics, timing semantics, or scan order.

### 9.4 Step + Breakpoint Interactions

Step mode and breakpoint mode MAY be combined with force/mask controls.

When combined:
- One full card evaluation per step.
- Mask/force behavior remains active.
- Timing remains real.

### 9.5 Operator Indication

UI MUST clearly show persistent indicators:
- `OUTPUT MASKED`
- `INPUT FORCED`
- `BREAKPOINT PAUSED` (when paused in `RUN_BREAKPOINT`)

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
- Current development scan interval is `500 ms` (temporary bring-up value).
- Target production scan interval is `10 ms`.
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

## 17. Settings Page - System Administration Contract (Unified)

### 17.1 Purpose and Scope

The Settings page is an administrative control surface and MUST remain fully separated from the LogicCard deterministic engine.

Settings scope includes:
- Connectivity.
- Security.
- Configuration management.
- System-level controls.

Settings page MUST NOT:
- Modify kernel memory directly.
- Change logic execution semantics.
- Interrupt deterministic scan.
- Recompute runtime logic.

All settings/config changes MUST follow:
- Stage -> Validate -> Commit -> Persist -> Confirm.

### 17.2 Connectivity Contract (WiFi Strategy)

Network model:
- Master SSID (permanent, non-editable).
- User SSID (configurable).

Default credentials:
- Master SSID: `advancedtimer`
- Master password: `12345678`
- User SSID default value: `FactoryNext`
- User password default value: `FactoryNext20$22#`

Boot connect behavior:
1. Attempt Master SSID for `2 s`.
2. If unavailable, attempt User SSID for up to `3 minutes`.
3. If unavailable, disable WiFi and continue in offline mode.

Rules:
- No AP mode.
- No infinite reconnect loop.
- WiFi execution is Core 1 only.
- Kernel execution MUST NOT wait on WiFi.
- Optional low-frequency retry MAY exist if non-continuous and non-blocking.

### 17.3 Security Contract

Settings page MUST support:
- Portal login enable/disable.
- Password change.
- Session timeout policy.

Access control MUST remain independent of logic engine execution.

### 17.4 Configuration Management (3-Level Rollback)

Storage model MUST maintain:
- Active Config.
- Last Known Good (LKG).
- Rollback Slot 1.
- Rollback Slot 2.
- Rollback Slot 3.
- Factory Baseline.

Constraints:
- Maximum history depth is 3 previous versions.
- Entries are full snapshots.
- Entries are version-stamped and timestamped.
- Optional checksum SHOULD be supported.

Commit behavior (success path only):
1. `Slot3 <- Slot2`
2. `Slot2 <- Slot1`
3. `Slot1 <- LKG`
4. `LKG <- Active`
5. `Active <- NewConfig`

On commit failure:
- `Active` MUST remain unchanged.
- History chain MUST remain unchanged.

All history and active-update operations MUST be atomic.

Restore behavior:
- User MAY restore from `LKG`, `Slot1`, `Slot2`, `Slot3`, or Factory Baseline.
- Restore MUST follow Stage -> Validate -> Commit -> Persist -> Confirm.
- Restored configuration becomes the new `Active` and is inserted into history through normal successful commit flow.

Safety rules:
- No partial writes.
- No silent automatic restore.
- No corruption of `Active` on failure.
- Engine restart/re-init only after successful commit when required by firmware apply path.

### 17.5 System Controls

Settings page MUST provide:
- Reboot device.
- Firmware version display.
- Optional global feature flags that do not alter logic semantics.

Navigation requirement:
- Settings page MUST include a visible Back/Home action to return to the main portal home screen.

### 17.6 Architectural Separation (ESP32)

Core 0 responsibilities:
- Deterministic scan engine.
- Logic evaluation.
- IO drive.

Core 1 responsibilities:
- Web server.
- WiFi stack.
- Settings page.
- Config staging and administration workflows.

Inter-core communication MUST use bounded mechanisms only:
- Queue-based commands/events, and/or
- Snapshot exchange.

Core 1 MUST NOT directly mutate Core 0 logic memory.

### 17.7 Determinism Protection

Settings operations MUST never:
- Reset timers silently.
- Change scan timing implicitly.
- Alter runtime state without explicit commit path.
- Introduce alternate execution semantics.

Core principle:
- Logic is deterministic and runtime-authoritative.
- Settings are administrative and controlled.
- Boundaries are strict and safety-first.

## 18. Delivery Tasklist and Source-of-Truth Convergence

This section is the execution checklist for configuration portal delivery.

Status legend:
- `[x]` done
- `[~]` in progress
- `[ ]` not started

### 18.1 Master Source-of-Truth Rule

`README.md` is the primary contract for portal + kernel integration behavior.

Rules:
- Any behavior change MUST be reflected here first (or in the same change set).
- Firmware comments in `main.cpp` are implementation notes, not primary authority.
- If `main.cpp` comments and this file conflict, this file is authoritative until reconciled.

### 18.2 Implementation Checklist (Execution Order)

Moved to the beginning of the document for execution-first workflow.

### 18.3 `main.cpp` Documentation Migration Tracker

Source file: `src/main.cpp` block comment header (`ADVANCED TIMER MASTER ARCHITECTURE & BEHAVIOR SPECIFICATION`).

Migration tasks:
1. `[x]` Extract architecture rationale and positioning text into a dedicated "Kernel Architecture Contract" section in this file (or linked spec).
2. `[x]` Extract enum semantic definitions (`logicCardType`, `logicOperator`, `cardMode`, `cardState`) into normative tables.
3. `[x]` Extract complete `LogicCard` field semantics (persistent vs runtime) and align names with portal-visible fields.
4. `[x]` Extract DI/DO/SIO behavior matrices and convert to concise MUST/SHOULD rules.
5. `[x]` Extract AI placeholder semantics and mark explicitly as current-phase limits.
6. `[x]` Add cross-reference tags in `main.cpp` comments pointing back to section numbers in this file.
7. `[x]` Remove duplicated or contradictory long-form comments from `main.cpp` after parity check.

### 18.4 Working Method

For each implementation PR/change set:
1. Update relevant checklist item status.
2. Reference touched section numbers from this contract in commit/PR notes.
3. If scope changed, update this tasklist before implementation continues.

## 19. Kernel Architecture Contract (Migrated from `src/main.cpp`)

### 19.1 Rationale and Positioning

AdvancedTimer is positioned between fixed-function timer relays and general PLC systems.

Positioning goals:
- PLC-like composable logic behavior.
- Timer-relay-like configuration simplicity.
- Data-driven behavior, not user-written code.

Core philosophy:
- Behavior is configured, not coded.

### 19.2 Core Architectural Principles

Unified LogicCard model:
- Every functional element is a LogicCard.
- Supported families: `DigitalInput`, `DigitalOutput`, `AnalogInput`, `SoftIO`.
- Families share one schema pattern and one condition model.

Hardware-vs-logic separation:
- Logic layer owns state machine/timing/counter/condition semantics.
- Hardware layer owns GPIO/ADC binding and physical drive/sampling.

Persistent-vs-runtime separation:
- Persistent config stores static behavior parameters (`type`, `mode`, `invert`, `setting1/2/3`, set/reset groups).
- Runtime state stores volatile execution data (`logicalState`, `physicalState`, `triggerFlag`, `currentValue`, timing registers, phase `state`).

### 19.3 Enum Semantics (Normative)

#### 19.3.1 `logicCardType`

| Enum | Meaning |
| --- | --- |
| `DigitalInput` | Physical digital GPIO input source |
| `DigitalOutput` | Physical digital GPIO output actuator |
| `AnalogInput` | Physical analog input sensor channel |
| `SoftIO` | Virtual output-like signal with no physical pin drive |

#### 19.3.2 `logicOperator`

| Enum | Meaning |
| --- | --- |
| `Op_AlwaysTrue` | Clause always evaluates true |
| `Op_AlwaysFalse` | Clause always evaluates false |
| `Op_LogicalTrue` | Target card logical state is true |
| `Op_LogicalFalse` | Target card logical state is false |
| `Op_PhysicalOn` | Target card physical state is on |
| `Op_PhysicalOff` | Target card physical state is off |
| `Op_Triggered` | Target card trigger pulse is asserted |
| `Op_TriggerCleared` | Target card trigger pulse is not asserted |
| `Op_GT` | `currentValue > threshold` |
| `Op_LT` | `currentValue < threshold` |
| `Op_EQ` | `currentValue == threshold` |
| `Op_NEQ` | `currentValue != threshold` |
| `Op_GTE` | `currentValue >= threshold` |
| `Op_LTE` | `currentValue <= threshold` |
| `Op_Running` | Target card is in running/active execution |
| `Op_Finished` | Target card reached finished state |
| `Op_Stopped` | Target card is stopped/idle |

Note:
- `Op_AlwaysFalse` is present in current firmware enum list.
- `Op_None` is not a required current enum in firmware.

#### 19.3.3 `cardMode`

DI modes:

| Mode | Trigger edges | Counter increment | Notes |
| --- | --- | --- | --- |
| `Mode_DI_Rising` | Rising only | Rising only | Debounce applies before edge qualification |
| `Mode_DI_Falling` | Falling only | Falling only | Debounce applies before edge qualification |
| `Mode_DI_Change` | Rising and falling | Rising and falling | Counts all qualified transitions |

Rules:
- DI mode acts as edge/counter filter.
- `triggerFlag` pulse is emitted only for the selected edge class.
- DI counting occurs only while `setCondition == true`.

DO/SIO modes:

| Mode | First action | Input dependency | Repeat behavior |
| --- | --- | --- | --- |
| `Mode_DO_Normal` | Wait `setting1` (Delay Before ON), then turn ON for `setting2` | Latched | Cycles `2..N` run full cycle |
| `Mode_DO_Immediate` | Turn ON immediately, then keep ON for `setting2` | Latched | Cycles `2..N` run full cycle |
| `Mode_DO_Gated` | Wait `setting1` (Delay Before ON), then turn ON for `setting2` | Gated (`setCondition` must remain true) | Full cycle only while gate remains true |

Rules:
- Legacy DO/SIO mode values are compatibility-only and deprecated for new configuration.
- AI uses placeholder mode `Mode_AI_Continuous` in current phase.

#### 19.3.4 `cardState`

| Enum | Meaning |
| --- | --- |
| `State_DO_Idle` | DO/SIO idle/re-armable state |
| `State_DO_OnDelay` | DO/SIO in Delay Before ON phase |
| `State_DO_Active` | DO/SIO output ON duration phase |
| `State_DO_Finished` | DO/SIO mission complete |
| `State_AI_Streaming` | AI placeholder state tag in current phase |

#### 19.3.5 `combineMode`

| Enum | Meaning |
| --- | --- |
| `Combine_None` | Use A clause only |
| `Combine_AND` | `A && B` |
| `Combine_OR` | `A || B` |

### 19.4 LogicCard Field Contract (Normative)

Identity and binding:
- `id`: globally unique card identifier.
- `type`: one of `logicCardType`.
- `index`: position inside family.
- `hwPin`: physical pin, or `255` for virtual cards.

Configuration:
- `invert`: active-low polarity switch where relevant.
- `setting1`, `setting2`, `setting3`: family-dependent parameters.

Family-dependent parameter semantics:
- DI:
  - `setting1`: debounce window.
  - `setting2`: reserved.
  - `setting3`: reserved/future extension.
- DO/SIO:
  - `setting1`: delay before output turns ON for each cycle.
  - `setting2`: ON duration before output turns OFF.
  - `setting3`: repeat count (`0=infinite`, `1=one-shot`, `N>1` exact cycles).
- AI:
  - `setting1`: input minimum.
  - `setting2`: input maximum.
  - `setting3`: EMA alpha `0.00..1.00` (stored internally as `0..1000` milliunits).

Runtime core signals:
- `logicalState`: DI qualified state, DO/SIO mission latch.
- `physicalState`: DI physical/polarity-adjusted state, DO/SIO effective time-shaped state.
- `triggerFlag`: one-scan pulse for DI edges or DO/SIO mission ignition.
- `currentValue`: DI edge counter, DO/SIO cycle counter, AI filtered value/accumulator.
- `startOnMs`, `startOffMs`, `repeatCounter`: timing and cycle bookkeeping; AI reuses `startOnMs/startOffMs` as output scale endpoints in this phase.
- `mode`: behavior mode enum.
- `state`: runtime phase/state enum.

Set/Reset condition groups:
- SET clause A: `setA_ID`, `setA_Operator`, `setA_Threshold`.
- SET clause B: `setB_ID`, `setB_Operator`, `setB_Threshold`.
- SET combiner: `setCombine`.
- RESET clause A: `resetA_ID`, `resetA_Operator`, `resetA_Threshold`.
- RESET clause B: `resetB_ID`, `resetB_Operator`, `resetB_Threshold`.
- RESET combiner: `resetCombine`.

### 19.5 Sequential Scan Engine Contract

Deterministic scan order per cycle:
1. DI family (`DI[0..N-1]`)
2. AI family (`AI[0..M-1]`)
3. SIO family (`SIO[0..K-1]`)
4. DO family (`DO[0..P-1]`)

Rules:
- Each card is fully evaluated atomically when visited.
- Later cards in the same scan see updated runtime values from earlier cards.
- Earlier cards see updates from later cards on the next scan.
- No parallel card evaluation semantics are permitted.

### 19.6 Digital Input (DI) Contract

Rules:
- DI processing (edge, debounce, counting) executes only when `setCondition == true`.
- RESET has highest priority: clears DI runtime signal/counter/timing fields and inhibits DI processing while true.
- Physical state sampling remains updated every scan for transparency.

### 19.7 Digital Output and SoftIO Contract

Trigger model:
- `setCondition` rising edge creates one-cycle `triggerFlag`.
- Trigger latches mission intent (`logicalState = true`) in latch modes.

Non-retriggerable rule:
- New triggers are ignored unless card is in `State_DO_Idle` or `State_DO_Finished`.

Phase model:
- Canonical phase progression: `Idle -> OnDelay (Delay Before ON) -> Active (ON Duration) -> (repeat | Finished/Idle)`.

Repeat semantics:
- `setting3 = 0`: repeat until RESET.
- `setting3 = 1`: one cycle then finish.
- `setting3 = N > 1`: exactly `N` full cycles.

RESET semantics:
- Immediate hard stop.
- Clears mission/timers/counters and forces idle.
- Absolute priority over mode progression.

Gated mode semantics:
- In `Mode_DO_Gated`, if `setCondition` drops during `OnDelay` (Delay Before ON) or `Active` (ON Duration), mission aborts immediately to idle.

### 19.8 Analog Input (AI) Contract

AI is deterministic per-scan transducer logic in current phase, not mission/phase driven logic.

Per-scan pipeline:
1. Sample raw analog input from `hwPin`.
2. Clamp raw value to `[setting1, setting2]`.
3. Scale linearly into `[startOnMs, startOffMs]`.
4. Apply EMA filter using `setting3` alpha (`0.00..1.00`).
5. Store filtered output in `currentValue`.

Current-phase constraints:
- AI is not gated by SET/RESET clauses.
- AI set/reset fields are schema placeholders only in this phase.
- `State_AI_Streaming` and `Mode_AI_Continuous` are semantic placeholders.

### 19.9 Fixed-Point Decimal Convention

Core rule:
- Decimal-capable fields are stored as unsigned centiunits (`value x 100`) unless explicitly exempted.

Conversions:
- Display value = `stored / 100`.
- Stored value = rounded display-to-centiunit conversion.

Field scope:
- Centiunits: `setting1`, `setting2`, `startOnMs`, `startOffMs`, and condition thresholds.
- Not centiunits: `setting3` for DI/DO/SIO (integer meaning), `setting3` for AI (normalized alpha `0.00..1.00`, internally milliunits), DI/DO/SIO counters in `currentValue`.

AI scaling formula:
- `scaled = startOnMs + (clamped - setting1) * (startOffMs - startOnMs) / (setting2 - setting1)`

Constraints:
- Non-negative unsigned numeric domain.
- AI input bound must satisfy `setting1 <= setting2`.
- AI output endpoints may be ascending or descending.

## 20. Firmware-Portal Interface Contract (Frozen v1)

This section defines the frozen interface between firmware and portal UI.

### 20.1 Transport Model

- Runtime streaming: WebSocket (snapshot and runtime events).
- Command path: WebSocket command envelope.
- Configuration commit/restore: HTTP JSON API.
- All messages are UTF-8 JSON objects.

### 20.2 Runtime Snapshot Schema

Top-level snapshot message:

```json
{
  "type": "runtime_snapshot",
  "schemaVersion": 1,
  "tsMs": 123456789,
  "scanIntervalMs": 10,
  "scanSeq": 8123,
  "runMode": "RUN_NORMAL",
  "testMode": {
    "active": false,
    "outputMaskGlobal": false
  },
  "system": {
    "alarmActive": false,
    "wifiOnline": true,
    "firmwareVersion": "1.0.0"
  },
  "cards": []
}
```

`cards[]` element schema:

```json
{
  "id": 0,
  "type": "DigitalInput",
  "index": 0,
  "familyOrder": 0,
  "physicalState": false,
  "logicalState": false,
  "effectiveState": false,
  "finalState": false,
  "triggerFlag": false,
  "aggregatedResult": false,
  "setResult": false,
  "resetResult": false,
  "resetOverride": false,
  "setConditionRows": [
    { "slot": "A", "value": false },
    { "slot": "B", "value": false }
  ],
  "resetConditionRows": [
    { "slot": "A", "value": false },
    { "slot": "B", "value": false }
  ],
  "maskForced": {
    "inputForced": false,
    "forcedValue": false,
    "outputMasked": false
  },
  "debug": {
    "evalPulse": false,
    "breakpointEnabled": false,
    "state": "State_DI_Idle",
    "mode": "Mode_DI_Rising"
  },
  "runtime": {
    "currentValue": 0,
    "startOnMs": 0,
    "startOffMs": 0,
    "repeatCounter": 0
  }
}
```

Rules:
- `cards[]` order MUST match deterministic firmware scan order.
- Snapshot is authoritative; portal MUST NOT recompute these fields.
- Unknown fields MUST be ignored by clients for forward compatibility.

### 20.3 Command Coverage (Required)

Portal-backend communication is bidirectional and command-capable.

Required command classes:
- Runtime control commands (portal -> backend, WebSocket): step, run mode, breakpoints, force, mask.
- Configuration lifecycle commands (portal -> backend, HTTP): load active config, save staged config, validate, commit/deploy, restore.
- Runtime/status events (backend -> portal, WebSocket): runtime snapshots and command results.

Required configuration command endpoints:
- `GET /api/config/active`: load current active configuration from device to portal.
- `POST /api/config/staged/save`: save staged configuration on device without activating it.
- `POST /api/config/staged/validate`: validate staged or provided config payload.
- `POST /api/config/commit`: save and deploy config as active (commit path).
- `POST /api/config/restore`: restore active config from history/factory source.

### 20.4 Command Envelope Schema

All WebSocket commands use:

```json
{
  "type": "command",
  "schemaVersion": 1,
  "requestId": "cmd-0001",
  "name": "step_once",
  "payload": {}
}
```

Supported commands:

```json
{ "name": "set_run_mode", "payload": { "mode": "RUN_NORMAL" } }
```

```json
{ "name": "step_once", "payload": {} }
```

```json
{ "name": "set_breakpoint", "payload": { "cardId": 4, "enabled": true } }
```

```json
{ "name": "set_test_mode", "payload": { "active": true } }
```

```json
{ "name": "set_input_force", "payload": { "cardId": 1, "forced": true, "value": true } }
```

```json
{ "name": "set_output_mask", "payload": { "cardId": 8, "masked": true } }
```

Notes:
- `set_test_mode` remains available for compatibility. Current live-page controls no longer require test mode to use force/mask.

Command response:

```json
{
  "type": "command_result",
  "schemaVersion": 1,
  "requestId": "cmd-0001",
  "ok": true,
  "error": null
}
```

### 20.5 Staged Configuration and Commit API

`GET /api/config/active` response:

```json
{
  "ok": true,
  "schemaVersion": 1,
  "activeVersion": "v42",
  "config": {
    "cards": []
  },
  "error": null
}
```

`POST /api/config/staged/save` request:

```json
{
  "schemaVersion": 1,
  "config": {
    "cards": []
  }
}
```

`POST /api/config/staged/save` response:

```json
{
  "ok": true,
  "stagedVersion": "s19",
  "error": null
}
```

`POST /api/config/staged/validate` request:

```json
{
  "schemaVersion": 1,
  "config": {
    "cards": []
  }
}
```

`POST /api/config/staged/validate` response:

```json
{
  "ok": true,
  "validation": {
    "errors": [],
    "warnings": []
  }
}
```

`POST /api/config/commit` request:

```json
{
  "schemaVersion": 1,
  "config": {
    "cards": []
  },
  "options": {
    "persist": true
  }
}
```

`POST /api/config/commit` response:

```json
{
  "ok": true,
  "activeVersion": "v42",
  "historyHead": {
    "lkgVersion": "v41",
    "slot1Version": "v40",
    "slot2Version": "v39",
    "slot3Version": "v38"
  },
  "requiresRestart": false,
  "error": null
}
```

### 20.6 Restore API

`POST /api/config/restore` request:

```json
{
  "schemaVersion": 1,
  "source": "LKG"
}
```

Allowed `source` values:
- `LKG`
- `SLOT1`
- `SLOT2`
- `SLOT3`
- `FACTORY`

`POST /api/config/restore` response:

```json
{
  "ok": true,
  "restoredFrom": "LKG",
  "activeVersion": "v43",
  "requiresRestart": false,
  "error": null
}
```

### 20.7 Error Model (Stable)

Error object:

```json
{
  "code": "VALIDATION_FAILED",
  "message": "setA_ID refers to unknown card id",
  "field": "cards[3].setA_ID",
  "details": {
    "cardId": 3
  }
}
```

Stable error codes:
- `INVALID_REQUEST`
- `UNSUPPORTED_SCHEMA_VERSION`
- `VALIDATION_FAILED`
- `COMMIT_FAILED`
- `RESTORE_FAILED`
- `BUSY`
- `NOT_FOUND`
- `FORBIDDEN_IN_MODE`
- `INTERNAL_ERROR`

Rules:
- `code` is stable for automation.
- `message` is operator-facing text.
- `field` is optional and used for form binding.

### 20.8 Required Card Config Payload Keys

Each config card payload MUST include at minimum:
- `id`, `type`, `index`, `hwPin`
- `invert`, `setting1`, `setting2`, `setting3`
- `mode`, `state`
- `setA_ID`, `setA_Operator`, `setA_Threshold`
- `setB_ID`, `setB_Operator`, `setB_Threshold`
- `setCombine`
- `resetA_ID`, `resetA_Operator`, `resetA_Threshold`
- `resetB_ID`, `resetB_Operator`, `resetB_Threshold`
- `resetCombine`

Runtime-only fields MUST NOT be required in commit payloads.
