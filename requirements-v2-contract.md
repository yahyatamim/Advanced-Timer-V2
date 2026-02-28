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
- No standalone `TIMER` card family.
- No standalone `COUNTER` card family.

## 3. Terms

- `Card`: Deterministic processing unit with typed inputs/outputs.
- `Scan`: One complete ordered evaluation pass of all enabled cards.
- `logicalState`: Internal logic/mission state stored per card.
- `physicalState`: Effective IO-facing state stored per card.
- `Staged config`: Candidate config pending validate/commit.
- `Active config`: Running config currently applied.
- `LKG`: Last known good committed config.

State semantics by family:

- `DI.logicalState`: debounced/qualified logic result after set/reset gating.
- `DI.physicalState`: polarity-adjusted sampled input state.
- `DO.logicalState`: mission latch/intent state.
- `DO.physicalState`: effective time-shaped output state used to drive hardware when not masked.
- `SIO.logicalState`: mission latch/intent state (same as DO semantics).
- `SIO.physicalState`: effective time-shaped virtual output state (no hardware drive).
- `AI`: transducer card; `currentValue` is authoritative output value and AI does not use logical/physical mission semantics.

## 4. Data Representation and Validation Contract

This section defines global rules for how data is represented and validated throughout the system.

### 4.1. Fixed-Point "Centiunit" Convention

To avoid the use of floating-point arithmetic in deterministic kernel logic, all configuration parameters that require decimal precision MUST be stored, transmitted, and handled as 32-bit unsigned integers representing the desired value multiplied by 100.

- **Rule**: `stored_value = user_value * 100`.
- **Example**: A time duration of `1.25` seconds is stored and sent in API payloads as the integer `125`. A threshold of `0.5` is stored as `50`.
- **Scope**: This applies to all timing parameters, thresholds, and any other fields requiring decimal precision.
- **Responsibility**: The web portal UI is responsible for all conversions to and from the user-facing decimal format. The firmware kernel exclusively handles the integer form.

### 4.2. Unsigned Value Constraint

All numeric configuration parameters, including all timing values, thresholds, counts, and ranges, MUST be non-negative.

- **Rationale**: The physical and logical concepts represented (e.g., time, counts, duration) do not have a negative equivalent in this system.
- **Enforcement**: The configuration validation process MUST reject any configuration payload containing a negative value for any parameter. All internal representations for these values should use unsigned integer types.

## 5. System Architecture Contract

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
- `platform`: board IO adapters, time source, watchdog primitives, and a **System Clock Service** (manages time sync and provides authoritative wall-clock time).

Forbidden coupling:

- Kernel must not directly call portal/network/filesystem functions.

## 5. State Ownership and Authority Contract

This contract establishes the principle of single, authoritative ownership for all runtime state variables. This is a fundamental rule for ensuring predictable data flow and preventing race conditions.

### 5.1 Single Source of Truth

- Every runtime variable within the system has exactly one **owner**.
- The owner is the card or service whose internal logic is responsible for generating and updating that variable's value.
- All other parts of the system may only reference this variable as a **read-only** input for their own logic or for display in snapshots.

### 5.2 Ownership Examples

- The integrated cycle counter of a `DO` card (`currentValue`) is owned exclusively by that `DO` card. Its value is modified only by the card's internal timer and mission logic.
- The final computed result of a `MATH` card (`currentValue`) is owned exclusively by that `MATH` card. Its value is updated only when the card's own evaluation logic runs.
- The filtered output of an `AI` card (`currentValue`) is owned exclusively by that `AI` card's internal processing pipeline.

### 5.3 Enforcement

- The configuration validation process **MUST** reject any configuration that attempts to create a write-binding to a variable from a non-owner. For example, a `Variable Assignment` from Card B cannot target a change to Card A's `currentValue`.
- The dependency topology graph must treat owned variables as root nodes that can only have outgoing (read) edges to consumers.

## 6. Determinism and Timing Contract

## 6.1 Scan timing targets

- Nominal production scan interval: `10 ms`.
- Allowed configurable scan interval range: `10..1000 ms`.
- Scan-speed variation is controlled by configured scan interval, not a dedicated slow run mode.
- Jitter and overrun budgets must be explicitly configured and exposed in diagnostics.

## 6.2 Deterministic execution rules

- Card evaluation order is fixed by ascending card ID.
- One scan produces one immutable snapshot revision.
- Evaluation topology is built at validate/commit time, never rebuilt during scan.
- All per-scan work is bounded; unbounded loops/allocations are prohibited in kernel path.

## 6.3 Run modes

Supported run modes:

- `RUN_NORMAL`: evaluate all enabled cards every scan.
- `RUN_STEP`: evaluate exactly one card evaluation step per user step command.
- `RUN_BREAKPOINT`: halt at configured breakpoint boundary; continue on command.

## 6.4 Build-Time Hardware Profile Contract

Each firmware build target MUST define a hardware profile that gates available card families, IO channel capacity, and optional features.

Minimum profile capabilities:

- Explicit compile-time capacities for each family (`DI`, `DO`, `AI`, `SIO`, `MATH`, `RTC` alarm channels).
- DI/DO/AI channel arrays (empty array means family unavailable in that build).
- Feature gates for optional capabilities (for example RTC chip presence).
- Backend selection for IO adapters (for example internal ADC vs external ADC).

Rules:

- If a family is disabled by profile, config validation MUST reject that card type.
- `RTC` card support requires both hardware capability (`hasRtc`) and build gate enabled.
- Kernel card logic MUST remain hardware-agnostic and consume adapter interfaces only.
- Remote/protocol-specific IO (for example Modbus) is out of core scope and MUST be introduced only through installable plugin adapters.
- The contract MUST NOT assume a single ESP32 variant; supported targets may include different ESP32 families and multi-chip controller combinations, represented only through platform adapters/profiles.

## 7. Card Families and Shared Contract

## 7.1 Profile-Optional card families

- `DI` (Digital Input)
- `AI` (Analog Input)
- `SIO` (Soft IO / virtual channel)
- `DO` (Digital Output)
- `MATH` (deterministic numeric compute)
- `RTC` (time and schedule source)

Rules:

- Every family is optional per hardware profile.
- A build may have zero cards of any family.
- Family counts are fixed at compile time per build target.
- Valid capacities are `0..N` for each family (`DI`, `DO`, `AI`, `SIO`, `MATH`, `RTC` alarm channels), where `N` is defined by hardware/profile design.
- If a family capacity is `0` (or gate disabled), config validation MUST reject that family in payloads.

Integrated behavior requirements:

- `DI` includes an integrated qualified-edge counter.
- `DO` and `SIO` include integrated timer and cycle counter behavior.

## 7.2 Common card fields

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
- `logicalState` (not used by AI semantics)
- `physicalState` (not used by AI semantics)
- `lastEvalUs`
- `faultCounters` (map)
- `currentValue` (authoritative numeric output for AI; counter/cycle value for DI/DO/SIO per family rules)

## 8. Set/Reset Condition Contract

This section defines the structure and behavior of the condition blocks used to control stateful cards. This logic is the primary mechanism for creating event-driven behavior in the system.

### 8.1. Block Structure

A condition block (for either `set` or `reset`) is composed of two clauses and a combiner, allowing for compound logic.

- **Clause A**: A logical comparison defined by (`source_card_ID`, `operator`, `threshold`).
- **Clause B**: A second logical comparison defined by (`source_card_ID`, `operator`, `threshold`).
- **Combiner**: An operator that determines how the results of Clause A and Clause B are combined.
  - `None`: The block's result is the result of Clause A only.
  - `AND`: The block's result is `true` only if both Clause A and Clause B are true.
  - `OR`: The block's result is `true` if either Clause A or Clause B is true.

State comparison support:

- Condition clauses may reference `STATE` only from `DO` and `SIO` cards via their `missionState`.
- Valid `missionState` values are `IDLE`, `ACTIVE`, `FINISHED`.
- For `STATE` comparisons, only `EQ` is valid.
- If `missionState` does not match the configured comparison value, the clause result is `false`.

### 8.2. Card Support Matrix

| Card Family | Supports Set/Reset |
| :--- | :--- |
| `DI` (Digital Input) | **Yes** |
| `DO` (Digital Output) | **Yes** |
| `SIO` (Soft IO) | **Yes** |
| `MATH` | **Yes** |
| `AI` (Analog Input) | No |
| `RTC` (Real-Time Clock) | No |

### 8.3. Behavioral Contract by Card Type

The final boolean result of a `set` or `reset` block has a different effect depending on the card it belongs to.

- **For `DI` cards**: The `set` condition acts as a **gate**. The card only processes physical input changes and increments its counter when the `set` condition is true. `reset` takes priority, clearing the integrated counter and inhibiting any further updates or edge detection until the reset condition becomes false.

- **For `DO` and `SIO` cards**: A **rising edge** (a transition from `false` to `true` in a single scan) on the `set` condition **triggers the start of a mission** (its timed sequence). `reset` acts as a **hard stop**, immediately aborting any in-progress mission and forcing the card to its idle state.

- **For `MATH` cards**: The `set` condition acts as an **enable gate**. When true, the card actively performs its calculation every scan. When false, the card **holds its last calculated value**. `reset` forces the card's output to its configured `fallbackValue`.

### 8.4. Special Case: `Gated` Mode for DO/SIO

The behavior of the `set` condition changes fundamentally for `DO` and `SIO` cards depending on their `mode`.

- In `Normal` and `Immediate` modes, the `set` condition is **edge-triggered**. It only needs to be true for a single scan to *latch* the mission, which will then run to completion even if the `set` condition becomes false later.

- In `Gated` mode, the `set` condition is **level-triggered**. It must **remain true** for the entire duration of the mission (`delayBeforeON` and `onDuration` phases). If the `set` condition becomes false at any point during the mission, the mission is immediately aborted, and the card returns to its idle state.

## 9. Card-Specific Requirements

## 8.1 DI (Digital Input)

Config requirements:

- physical channel mapping
- invert input flag
- **`debounceTime`**: The primary configuration parameter, defining the time an input must be stable before a change is qualified.
- `edge mode` (`RISING`, `FALLING`, `CHANGE`)
- set condition block
- reset condition block
- integrated counter enable/visibility policy

Runtime requirements:

- raw physical read
- polarity-adjusted sample
- debounced qualified sample
- logicalState
- physicalState
- edge/trigger indicators
- integrated counter value

Force modes:

- `REAL`
- `FORCED_HIGH`
- `FORCED_LOW`

Rules:

- DI edge mode is mandatory and drives which qualified edges increment the integrated counter.
- DI logical-state updates and counter increments execute only when set condition is true.
- DI updates are applied only after debounce qualification timing is satisfied.
- Reset has strict priority over set: when reset condition is true, DI counter is reset and logical state is held from further update.
- Invert flag is applied before debounce and edge qualification.
- Force transitions must not corrupt edge detection counters.

## 8.2 AI (Analog Input)

Config requirements:

- physical channel mapping
- engineering units
- **Core pipeline parameters:**
  - `inputRange` (min/max)
  - `clampRange` (min/max)
  - `outputRange` (min/max)
  - `emaAlpha` (`0..100` centiunits representing `0.00..1.00`)

Runtime requirements:

- raw sample
- clamped value
- mapped/scaled value
- EMA filtered value
- quality flag (`GOOD|CLAMPED|INVALID`)
- `currentValue` as authoritative AI output

Force modes:

- `REAL`
- `FORCED_VALUE`

Rules:

- AI pipeline is always: `raw -> clamp -> map/scale -> EMA`.
- EMA is always applied. To disable smoothing behavior, set `emaAlpha = 100` (`1.00`).
- AI is a transducer/data-capture card and has no internal logical-condition evaluation.
- AI does not run set/reset gating semantics internally.
- AI does not use logicalState/physicalState mission semantics for control behavior.
- Type and unit metadata must be preserved through snapshots.
- Invalid forced values must be rejected by command validation.

## 8.3 SIO (Soft IO)

Config requirements:

- **`mode`**: The card's operational mode, identical to DO (`Normal`, `Immediate`, `Gated`).
- **Integrated Timer and Counter Parameters**:
  - `delayBeforeON`: Time from `set` trigger to output activation.
  - `onDuration`: Duration the output remains active.
  - `repeatCount`: Number of cycles to run (`0` for infinite).
- set condition block
- reset condition block
- write policy (which roles/commands may write)

Runtime requirements:

- logicalState
- physicalState
- missionState (`IDLE`, `ACTIVE`, `FINISHED`)
- set/reset active indicators
- reset-dominance indicator
- cycle counters and mission phase state

Rules:

- SIO behavior is identical to DO behavior except it has no GPIO binding and never drives hardware.
- Set/reset precedence, timing phases, gating behavior, and counters must match DO semantics.
- Because SIO has no physical relay/GPIO drive, SIO mask semantics are not applicable.
- Unauthorized writes are rejected and audited.

## 8.4 DO (Digital Output)

Config requirements:

- physical channel mapping
- **`mode`**: The card's operational mode (`Normal`, `Immediate`, `Gated`).
- **Integrated Timer and Counter Parameters**:
  - `delayBeforeON`: Time from `set` trigger to output activation.
  - `onDuration`: Duration the output remains active.
  - `repeatCount`: Number of cycles to run (`0` for infinite).
- set condition block
- reset condition block

Runtime requirements:

- logicalState
- masked output result
- physicalState
- physical drive state
- missionState (`IDLE`, `ACTIVE`, `FINISHED`)
- set/reset active indicators
- reset-dominance indicator
- integrated cycle counter (tracks completed repeat cycles)

Mask modes:

- `DIRECT`
- `MASKED`

Rules:

- Reset precedence is mandatory: if set and reset are both true in same evaluation, reset wins.
- Masking suppresses physical drive only; logical evaluation still executes.
- DO timer and counter behavior are intrinsic to DO and not delegated to separate timer/counter cards.
- In `Normal` and `Immediate` modes, a new set trigger received while a mission is already in progress (`ACTIVE`) must be ignored until the card returns to `IDLE` or `FINISHED`.

## 8.5 MATH

The MATH card is a versatile, multi-purpose processing block for performing calculations and control loop algorithms. Its entire operation is gated by `set` and `reset` conditions.

### 8.5.1 Config requirements

- **`mode`**: Selects the card's primary function.
  - `Mode_Standard_Pipeline`: A sequential arithmetic and processing chain.
  - `Mode_PID_Controller`: A Proportional-Integral-Derivative control loop.
  - Future modes may include `Mode_Totalizer`, `Mode_PeakHold`, etc.
- **`set` condition block**: Enables the card's operation.
- **`reset` condition block**: Disables the card and forces its output to the `fallbackValue`. Has priority over `set`.
- **`fallbackValue`**: The output value when the card is reset or encounters a fault.

#### 8.5.1.1 `Mode_Standard_Pipeline` Config

- **Arithmetic Stage:**
  - `inputA_source`: A `CONSTANT` or `VARIABLE_REF`.
  - `inputB_source`: A `CONSTANT` or `VARIABLE_REF`.
  - `operator`: `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `POW`, `MIN`, `MAX`.
- **Pipeline Stage:**
  - `rateLimit`: Maximum change in output units per second. An inert value (e.g., `0`) disables this stage.
  - `clampMin`, `clampMax`: The range to which the arithmetic result is clamped.
  - `scaleMin`, `scaleMax`: The range to which the clamped value is scaled.
  - `emaAlpha`: The alpha for the EMA filter (`0..100` centiunits representing `0.00..1.00`). An inert value of `100` effectively disables the filter.

#### 8.5.1.2 `Mode_PID_Controller` Config

- `processVariable_source`: A `VARIABLE_REF` to the measured system variable.
- `setpoint_source`: A `CONSTANT` or `VARIABLE_REF` for the target value.
- `gainP`, `gainI`, `gainD`: The proportional, integral, and derivative tuning gains.
- `outputMin`, `outputMax`: Limits for the PID output to prevent integral windup and ensure safe operation.
- `integralResetPolicy`: Defines behavior for resetting the integral term.

### 8.5.2 Runtime requirements

- `setResult`, `resetResult`: The boolean result of the `set`/`reset` condition evaluations.
- `intermediateValue`: The value after the arithmetic stage, before the pipeline.
- `currentValue`: The final output value after all processing.
- `faultStatus`: Indicates faults like divide-by-zero.

### 8.5.3 Rules

1.  **Gating is paramount**: If the `reset` condition is true, the card's `currentValue` is immediately forced to `fallbackValue` and no other processing occurs. If the `set` condition is false, the card holds its last known `currentValue`.
2.  **`Standard_Pipeline` Execution Order**: When active, the card **MUST** execute the following sequence every scan:
    1.  `Arithmetic`: `intermediateValue = operator(inputA, inputB)`.
    2.  `Rate Limiter`: The output is slewed towards `intermediateValue` at the configured rate.
    3.  `Clamp`: The rate-limited value is clamped.
    4.  `Scale`: The clamped value is scaled.
    5.  `EMA Filter`: The scaled value is filtered.
    6.  The result is stored in `currentValue`.
3.  **Inert Parameter Values**: To bypass a pipeline stage in `Standard` mode, the user can provide inert values:
    - `rateLimit = 0`: Disables the rate limiter.
    - `clampMin >= clampMax`: Disables clamping.
    - `scaleMin == clampMin` AND `scaleMax == clampMax`: Disables scaling.
    - `emaAlpha = 100` (`1.00`): Disables the EMA filter.
4.  **`PID_Controller` Execution**: When active, the card calculates its output based on the standard PID algorithm, respecting the output limits to prevent integral windup. The `set` condition enables the loop, and `reset` disables it and clears the integral term.
5.  **Faults**: A fault during calculation (e.g., divide-by-zero) MUST force the `currentValue` to the `fallbackValue` for that scan and set a fault flag.

## 8.6 RTC

The RTC card is a logic component that acts as an independent, configurable scheduler. It evaluates a single time-based schedule against the global System Clock Service and produces a boolean output state, making it ideal for triggering time-based events.

RTC is treated as schedule-based alarm functionality; available RTC card instances are bounded by compile-time RTC alarm channel capacity in the active hardware profile.

### 8.6.1 Config requirements

- **`schedule`**: A block defining the single schedule for this card.
  - `year` (optional)
  - `month` (optional)
  - `day` (optional)
  - `hour` (required)
  - `minute` (required)
  - `second` (required)
  - `weekday` (optional)
  - Omitted optional fields are wildcards:
    - if `month` is omitted, the schedule is valid for all months.
    - if `weekday` is omitted, the schedule is valid for all weekdays.
    - if `year` or `day` are omitted, those fields also match all valid values.
- **`triggerDuration`**: The duration for which the RTC output remains asserted after a matching schedule trigger.

### 8.6.2 Runtime requirements

- **`logicalState`**: The primary output. Is `true` while the card is inside an active trigger-duration window started by a schedule match; `false` otherwise.
- **`timeUntilNextStartSec`**: Seconds until the schedule will next become active.
- **`timeUntilNextEndSec`**: Seconds until the currently active schedule will end.

### 8.6.3 Rules

- The RTC card's evaluation **MUST** be based on the authoritative time provided by the global System Clock Service.
- The card itself does not manage time synchronization, timezones, or DST; it is only a schedule evaluator.
- Its `logicalState` is a standard boolean variable that can be used as an input or condition for any other card in the system.
- The card has no `set` or `reset` conditions; its state is purely a function of its schedule and the current time.
- A schedule trigger occurs when current clock fields match all configured non-wildcard schedule fields.
- On trigger, `logicalState` is asserted for `triggerDuration`; after duration expiry, `logicalState` returns to `false` unless retriggered.

## 9. Variable Assignment Contract

Variable assignment is a typed binding system for card parameters.
It is not a scripting engine.

## 9.1 Binding source modes

- `CONSTANT`
- `VARIABLE_REF`

## 9.2 Allowed references

Reference types:

- `BOOL`
- `NUMBER`
- `TIME_WINDOW`
- `STATE` (only `DO`/`SIO` `missionState`: `IDLE|ACTIVE|FINISHED`)

Bindings must satisfy:

- type compatibility
- range compatibility
- unit compatibility (for numeric values)
- availability in topology (no forward-cycle dependency)

## 9.3 Binding lifecycle

- Defined in staged config.
- Fully validated before commit.
- Applied atomically on successful commit.
- Any invalid binding aborts whole commit transaction.

## 10. Dependency Topology Contract

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

## 11. Configuration and Schema Contract

## 11.1 Model separation

Required separate models:

- persistent config model
- in-memory runtime state model
- exported snapshot model

## 11.2 Schema versioning

- Config payload must include `schemaVersion`.
- Migration path must exist for every supported previous version.
- Migration must be deterministic and auditable.

## 11.3 Config lifecycle states

- `ACTIVE`
- `STAGED`
- `VALIDATED`
- `COMMITTED`
- `ROLLED_BACK`

## 12. Persistence and Power-Loss Safety

## 12.1 Storage artifacts

Required slots:

- active config
- staged config
- LKG config (single rollback slot for last committed config)
- factory default config

## 12.2 Commit protocol (transactional)

1. Save staged payload.
2. Validate staged payload (schema + topology + semantic checks).
3. Write commit candidate with checksum/version/hash.
4. Rotate active/LKG atomically (single rollback slot).
5. Promote candidate to active.
6. Publish commit result event.

If any step fails:

- active remains previous valid config,
- failure is logged and surfaced via API,
- rollback path remains valid.

## 13. WiFi Connectivity Contract

This section defines the device's behavior for connecting to a wireless network.

### 13.1. Dual-SSID Strategy
The device MUST support two WiFi configurations to ensure both field usability and recoverability:
- **Master SSID**: A permanent, non-editable network configuration intended for factory setup, diagnostics, or emergency field recovery. Default credentials will be hard-coded.
- **User SSID**: A user-configurable network that the device will use for its primary operational connection.

### 13.2. Connection Logic
On boot, the device MUST follow this connection sequence:
1.  Attempt to connect to the **Master SSID** with a short timeout (e.g., 2-3 seconds).
2.  If the Master SSID fails, attempt to connect to the **User SSID** with a longer timeout (e.g., 3 minutes).
3.  If both attempts fail, the device MUST enter a fully operational **offline mode**, where the deterministic kernel continues to run without interruption. The device will periodically and non-intrusively re-attempt the connection sequence in the background.

### 13.3. Architectural Constraints
- **STA Mode Only**: The device MUST operate exclusively in Station (STA) mode to connect to existing WiFi networks. It MUST NOT provide an Access Point (AP) mode.
- **Non-Blocking Operation**: All WiFi-related tasks (scanning, connecting, handling traffic) MUST execute on the non-deterministic Core 1 and MUST NOT block or interfere with the timing of the scan loop on Core 0.

## 14. Portal UI/UX Contract

This section defines the high-level principles and protocol-level rules for the web-based user interface.

### 14.1. Core UX Philosophy
- **Mobile-First Industrial Usability**: The UI MUST be optimized for use on mobile devices in field conditions. This requires large, touch-friendly tap targets, high-contrast visuals for readability in varied lighting, and a workflow that minimizes keyboard entry.
- **Constant Runtime Awareness**: The layout MUST feature a fixed header that is always visible on screen, regardless of scroll position. This header's primary responsibility is to display the most critical, real-time system status:
  - Current Run Mode (`RUN_NORMAL`, `RUN_STEP`, etc.)
  - Test / IO Override Status (e.g., `OUTPUTS MASKED`, `INPUTS FORCED`)
  - Alarm State
  - Connectivity Status
- **Clarity and Safety**: The UI must provide clear, unambiguous visual feedback for all critical user actions. It must be impossible for a user to confuse a locally-staged (edited) configuration with the active configuration currently running on the device.

### 14.2. Protocol-Level Rules
- The portal presents cards in a fixed, deterministic order matching the firmware's scan order.
- The UI **MUST NOT** recompute or reinterpret authoritative logical results from the firmware. All runtime state displayed to the user must be from the kernel's snapshot.
- All configuration edits are staged locally in the portal until explicitly sent to the device via the `save staged` or `commit` API commands.
- Runtime controls in the UI (e.g., Force, Mask buttons) must reflect the actual, kernel-acknowledged state, not an optimistic local state.

## 15. Command and API Contract

### 15.1. Command classes

- Runtime control: run mode, step, breakpoint operations.
- IO control: force input, mask output (per-output and global output mask).
- Config lifecycle: load, save staged, validate, commit, restore.
- Admin control: reboot, diagnostics reset, session management.

### 15.2. API behavior guarantees

- Every mutating command returns structured success/error payload.
- Validation errors return machine-readable codes and human message.
- Snapshot API always returns the latest complete snapshot revision.
- WebSocket event ordering must preserve revision sequence.

### 15.3. Minimum response metadata

- `requestId`
- `timestamp`
- `status`
- `errorCode` (on failure)
- `snapshotRevision` (where relevant)

## 16. Security and Authorization Contract

### 16.1. Role model

Minimum roles:

- `VIEWER`
- `OPERATOR`
- `ENGINEER`
- `ADMIN`

### 16.2. Protected operations

Authentication and authorization required for:

- config commit/restore
- force/mask writes
- variable assignment edits
- schedule/RTC policy edits
- firmware/OTA actions

### 16.3. Audit requirements

Audit record fields:

- actor identity
- role
- action
- target resource
- before/after hash or value summary
- timestamp
- result (`SUCCESS|FAILURE`)

## 17. Fault Handling and Degraded Mode

### 17.1. Fault classes

- validation fault
- runtime math fault
- RTC/time source fault
- IO read/write fault
- queue overflow/backpressure fault
- scan overrun fault
- watchdog fault

### 17.2. Fault policies

Each fault class must have configurable severity:

- `INFO`
- `WARN`
- `CRITICAL`

Critical fault behavior:

- deterministic scan continues where safe,
- unsafe outputs move to configured safe state,
- fault latched until acknowledged or cleared by policy.

## 18. Observability and Diagnostics

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

## 19. Performance and Resource Contract

- Kernel path must avoid dynamic heap allocation per scan.
- Snapshot generation must be bounded and non-blocking to kernel.
- Portal/network load must not violate scan timing budget.
- Memory and queue limits must be explicit and monitored.

## 20. Acceptance Test Contract

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

## 21. Backward Compatibility Policy

- DI/AI/SIO/DO behavior is default-compatible unless explicitly version-gated in schema.
- New features are schema/config gated and must not alter legacy behavior unless explicitly configured.
- Migration must preserve equivalent behavior for legacy configs.

## 22. Change Control

- Contract changes require version bump and change log entry.
- Breaking behavior changes require major version increment.
- Any ambiguity discovered during implementation must be resolved by editing this contract before coding continues.
- Behavior/API/validation decisions must be captured in `docs/decisions.md`.
<<<<<<< HEAD
=======

## 23. Immediate Follow-up Artifacts
>>>>>>> 549b1e14679d699bc6e73313db0fe34b58c71af1

## 23. Immediate Follow-up Artifacts

<<<<<<< HEAD
Create and maintain:

- `docs/INDEX.md`
- `docs/acceptance-matrix-v2.md`
- `docs/schema-v2.md`
- `docs/dependency-topology-rules.md`
=======
- `docs/acceptance-matrix-v2.md`
- `docs/schema-v2.md`
- `docs/dependency-topology-rules.md`
>>>>>>> 549b1e14679d699bc6e73313db0fe34b58c71af1
- `docs/timing-budget-v2.md`
- `docs/fault-policy-v2.md`
- `docs/api-contract-v2.md`
- `docs/decisions.md`
<<<<<<< HEAD
- `docs/hardware-profile-v2.md`
=======
>>>>>>> 549b1e14679d699bc6e73313db0fe34b58c71af1
