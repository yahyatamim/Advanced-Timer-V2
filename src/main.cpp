/**********************************************************************************************
 *
 * ADVANCED TIMER
 * MASTER ARCHITECTURE & BEHAVIOR SPECIFICATION
 *
 * ============================================================================================
 * 0) PURPOSE OF THIS DOCUMENT
 * ============================================================================================
 *
 * This block comment is the authoritative specification of the Advanced Timer
 * automation kernel.
 *
 * It defines:
 * • Project rationale
 * • System positioning
 * • Architectural principles
 * • Enum semantics
 * • Complete LogicCard field definitions
 * • DigitalInput contract
 * • DigitalOutput / SoftIO contract
 * • Condition system behavior
 *
 * This document serves as:
 * - Firmware development baseline
 * - Configuration portal reference
 * - Long-term system contract
 *
 * If implementation conflicts with this document,
 * this document is the source of truth.
 *
 *
 * ============================================================================================
 * 1) PROJECT RATIONALE & POSITIONING
 * ============================================================================================
 *
 * Traditional Timer Relays:
 * • Fixed manufacturer-defined modes
 * • No composable logic
 * • No inter-signal dependency modeling
 * • Rigid state machines
 *
 * Traditional PLC Systems:
 * • Extremely flexible
 * • Require programming knowledge
 * • Require external engineering tools
 * • Expensive and complex for small systems
 *
 * The Advanced Timer exists between these two extremes.
 *
 * It delivers:
 * • PLC-level logical flexibility
 * • Timer-level configuration simplicity
 * • Data-driven behavior
 * • No user programming requirement
 *
 * Philosophy:
 * Behavior is configured, not coded.
 *
 *
 * ============================================================================================
 * 2) CORE ARCHITECTURAL PRINCIPLES
 * ============================================================================================
 *
 * 2.1 Unified LogicCard Model
 * --------------------------------------------------------------------------------------------
 *
 * Every functional element in the system is represented by a LogicCard.
 *
 * Supported card families:
 *
 * DigitalInput
 * DigitalOutput
 * AnalogInput
 * SoftIO (virtual output)
 *
 * All share:
 * • Same data schema
 * • Same persistence model
 * • Same condition system
 *
 *
 * 2.2 Hardware vs Logic Separation
 * --------------------------------------------------------------------------------------------
 *
 * Logic Layer:
 * - State machines
 * - Timing
 * - Counters
 * - Condition evaluation
 *
 * Hardware Layer:
 * - GPIO binding
 * - ADC sampling
 * - Pin driving
 *
 * LogicCards describe behavior, not hardware directly.
 *
 *
 * 2.3 Persistent vs Runtime Model
 * --------------------------------------------------------------------------------------------
 *
 * Persistent (Filesystem JSON):
 * - type
 * - mode
 * - invert
 * - timing settings (setting1, setting2, setting3)
 * - set/reset conditions
 *
 * Runtime (volatile):
 * - logicalState
 * - physicalState
 * - triggerFlag
 * - currentValue
 * - state
 * - timing registers (startOnMs, startOffMs, repeatCounter)
 * Compatibility note:
 * - AI uses the same persisted keys (setting1/setting2/setting3,
 *   startOnMs/startOffMs, currentValue).
 * - Key names are unchanged; numeric unit meaning follows Section 10
 *   fixed-point centiunit rules.
 * - This phase does not require a schema migration.
 *
 *
 * ============================================================================================
 * 3) ENUM DEFINITIONS & SEMANTICS
 * ============================================================================================
 *
 * 3.1 logicCardType
 * --------------------------------------------------------------------------------------------
 *
 * DigitalInput → Physical digital GPIO input
 * DigitalOutput → Physical digital GPIO output
 * AnalogInput → Physical analog input channel
 * SoftIO → Virtual output-like logic signal
 *
 *
 * 3.2 logicOperator
 * --------------------------------------------------------------------------------------------
 *
 * Op_AlwaysTrue → unconditional clause
 * Op_None → clause disabled
 *
 * Logical Operators:
 * Op_LogicalTrue
 * Op_LogicalFalse
 *
 * Physical Operators:
 * Op_PhysicalOn
 * Op_PhysicalOff
 *
 * Trigger Operators:
 * Op_Triggered
 * Op_TriggerCleared
 *
 * Numeric Operators (compare against currentValue):
 * Op_GT, Op_LT, Op_EQ, Op_NEQ, Op_GTE, Op_LTE
 *
 * Process State Operators:
 * Op_Running
 * Op_Finished
 * Op_Stopped
 *
 *
 * 3.3 cardMode
 * --------------------------------------------------------------------------------------------
 *
 * DI Modes:
 *
 * DI modes determine TWO things:
 * 1. Which edge type triggers the one-cycle pulse (triggerFlag)
 * 2. Which edge type increments the event counter (currentValue)
 *
 * DI_Rising
 * - Triggers on: LOW → HIGH transition (rising edge only)
 * - Counter increments on: rising edges only
 * - Pulse (triggerFlag): generated on each rising edge (after debounce)
 * - Use case: Count button presses, start signals, rising events
 * - Example: Button pressed 3 times → currentValue = 3
 *
 * DI_Falling
 * - Triggers on: HIGH → LOW transition (falling edge only)
 * - Counter increments on: falling edges only
 * - Pulse (triggerFlag): generated on each falling edge (after debounce)
 * - Use case: Count button releases, stop signals, falling events
 * - Example: Button released 3 times → currentValue = 3
 *
 * DI_Change
 * - Triggers on: ANY transition (both rising AND falling edges)
 * - Counter increments on: all edges (both directions)
 * - Pulse (triggerFlag): generated on each edge (after debounce)
 * - Use case: Count total state changes, toggle-based behavior
 * - Example: Button pressed and released once → currentValue = 2 (both edges)
 *
 * DI Mode Behavior Matrix:
 * | Mode | Rising Edge | Falling Edge | Counter Behavior |
 * | --- | --- | --- | --- |
 * | DI_Rising | Counts ✓ | Ignores ✗ | Increments only on ↑ |
 * | DI_Falling | Ignores ✗ | Counts ✓ | Increments only on ↓ |
 * | DI_Change | Counts ✓ | Counts ✓ | Increments on any transition |
 *
 * Important: Mode acts as a COUNTER INCREMENT FILTER
 * - Edge detection is mode-dependent
 * - triggerFlag pulse is ONLY generated when detected edge matches mode
 * - currentValue only increments when detected edge matches mode
 * - Debounce (setting1) is applied before mode checking
 * - All counter increments occur only when setCondition == true
 *
 *
 * DO/SIO Modes:
 * DO_Normal
 * - First action: wait setting1 (OnDelay), then run Active.
 * - Input dependency: latched (ignores setCondition after trigger).
 * - Repeat behavior: full cycle for cycles 2..N.
 * - Primary use: sequential startups and deterministic delayed activation.
 *
 * DO_Immediate
 * - First action: skip initial OnDelay and enter Active immediately.
 * - Input dependency: latched (ignores setCondition after trigger).
 * - Repeat behavior: cycles 2..N run full cycle timing (OnDelay -> Active).
 * - Primary use: leading-edge pulse with stable periodic follow-up cycles.
 *
 * DO_Gated
 * - First action: wait setting1 (OnDelay), then run Active.
 * - Input dependency: gated (setCondition must stay true while running).
 * - Repeat behavior: full cycle while gate stays true.
 * - Primary use: safety interlock / enable chain behavior.
 *
 * DO/SIO comparison matrix:
 * | Feature | DO_Normal | DO_Immediate | DO_Gated |
 * | --- | --- | --- | --- |
 * | First action | Wait setting1 | Active immediately | Wait setting1 |
 * | Input dependency | Latched | Latched | Gated |
 * | Repeat 2..N | Full cycle | Full cycle | Full cycle if gate stays true |
 * | Primary use | Sequential startups | Instant alert pulse | Safety /
 * interlock |
 *
 * Legacy mode values:
 * - Retained for schema compatibility only.
 * - Deprecated for new DO/SIO configurations.
 * AI Placeholder Mode:
 * Mode_AI_Continuous
 * - Placeholder semantic tag for future AI runtime implementation.
 * - No additional runtime behavior is introduced in this phase.
 *
 *
 * 3.4 cardState
 * --------------------------------------------------------------------------------------------
 *
 * Generic states retained for compatibility.
 *
 * DO/SIO Canonical Phases:
 *
 * State_DO_Idle
 * State_DO_OnDelay
 * State_DO_Active
 * State_DO_Finished
 *
 * AI Placeholder State:
 * State_AI_Streaming
 * - Placeholder semantic tag for future AI runtime implementation.
 * - AI does not use a phase-machine in this phase.
 *
 *
 * 3.5 logicCombine
 * --------------------------------------------------------------------------------------------
 *
 * Combine_None → Use clause A only
 * Combine_AND → A && B
 * Combine_OR → A || B
 *
 *
 * ============================================================================================
 * 4) LOGICCARD STRUCT — COMPLETE FIELD SPECIFICATION
 * ============================================================================================
 *
 * Identity & Binding:
 *
 * id → Global unique ID
 * type → logicCardType
 * index → Position within family
 * hwPin → Hardware pin (255 for virtual)
 *
 *
 * Configuration:
 *
 * invert → bool
 *          true  = Active Low / inverted polarity
 *          false = Active High / normal polarity
 *
 *
 * Timing Parameters (fixed-point centiunits unless explicitly noted):
 *
 * setting1 → DO/SIO: ON delay / pulse-high duration
 *            DI:     debounce window
 *            AI:     input minimum (raw ADC/sensor lower bound)
 *
 * setting2 → DO/SIO: OFF delay / inter-cycle rest
 *            DI:     reserved
 *            AI:     input maximum (raw ADC/sensor upper bound)
 *
 * setting3 → DO/SIO: repeat count
 *                 0     = infinite
 *                 1     = one-shot
 *                 >1    = exactly N full cycles
 *            DI:     extra parameter / future use (e.g. filter samples,
 * long-press threshold…)
 *            AI:     EMA alpha in fixed range 0..1000 (0.0..1.0)
 *
 *
 * Core Runtime Signals:
 *
 * logicalState → DI: qualified logical input
 *                DO/SIO: mission latch (intent to run)
 *
 * physicalState → DI: polarity-adjusted / debounced input state
 *                 DO/SIO: time-shaped effective output truth
 *
 * triggerFlag → One-cycle pulse
 *               DI: edge detection (after debounce/qualify)
 *               DO/SIO: ignition from setCondition rising edge
 *
 *
 * Timing Registers:
 *
 * currentValue → DI: event counter
 *                DO/SIO: cycle counter (increments on physical rising edge)
 *                AI: EMA accumulator and filtered output value
 *
 * startOnMs -> Timestamp when current ON-phase began
 *             AI: output minimum (scaled physical lower bound)
 *
 * startOffMs -> Timestamp when current OFF-phase began
 *              AI: output maximum (scaled physical upper bound)
 *
 * repeatCounter -> DO/SIO: counts completed cycles for repeat logic
 *                 AI: unused placeholder in this phase
 *
 *
 * Mode & State:
 *
 * mode → DI: edge / debounce behavior
 *        DO/SIO: execution engine switch
 *                DO_Normal, DO_Immediate, DO_Gated
 *                (legacy mode values are compatibility-only)
 *        AI: placeholder mode tag (Mode_AI_Continuous)
 *
 * state → DI: filtering lifecycle state
 *         DO/SIO: phase state (Idle, OnDelay, Active, Finished)
 *         AI: placeholder state tag (State_AI_Streaming)
 *
 *
 * SET Group Fields:
 *
 * setA_ID
 * setA_Operator
 * setA_Threshold
 *
 * setB_ID
 * setB_Operator
 * setB_Threshold
 *
 * setCombine
 *
 *
 * RESET Group Fields:
 *
 * resetA_ID
 * resetA_Operator
 * resetA_Threshold
 *
 * resetB_ID
 * resetB_Operator
 * resetB_Threshold
 *
 * resetCombine
 *
 *
 * ============================================================================================
 * 5) SEQUENTIAL SCAN ENGINE CONTRACT
 * ============================================================================================
 *
 * Runtime model:
 * The automation kernel executes as a deterministic sequential scan engine.
 * One full scan cycle processes card families in a fixed order, then repeats.
 *
 * Canonical scan order per cycle:
 * 1) DigitalInput (DI) cards   : DI[0] -> DI[1] -> ... -> DI[N-1]
 * 2) AnalogInput (AI) cards    : AI[0] -> AI[1] -> ... -> AI[M-1]
 * 3) SoftIO (SIO) cards        : SIO[0] -> SIO[1] -> ... -> SIO[K-1]
 * 4) DigitalOutput (DO) cards  : DO[0] -> DO[1] -> ... -> DO[P-1]
 * 5) End of scan -> restart from DI[0]
 *
 * Per-card execution rule:
 * - Each card is fully evaluated when visited in sequence.
 * - The engine updates all relevant runtime fields for that card before moving
 * on. (e.g., physicalState, logicalState, triggerFlag, currentValue, state,
 * timing registers)
 *
 * DI-specific behavior during scan:
 * - On each DI visit, evaluate physical input first.
 * - Apply invert/debounce/gating/reset rules.
 * - Update logical state, trigger pulse, and counter (if conditions permit).
 * - Commit DI runtime updates before visiting next DI.
 *
 * Cross-family visibility:
 * - Cards evaluated later in the same scan see already-updated runtime values
 *   of cards evaluated earlier in that scan.
 * - Cards evaluated earlier see later-card changes only on the next scan.
 * - Because AI is evaluated before SIO/DO, downstream cards in the same scan
 *   cycle consume AI values updated in that same cycle.
 *
 * Determinism requirement:
 * - Scan order must remain fixed and predictable for repeatable behavior.
 * - No parallel/async card evaluation is assumed in this contract.
 *
 * Reset priority:
 * - resetCondition handling remains highest priority at card level as defined
 * elsewhere.
 * - Reset effects are applied immediately when that card is evaluated in scan.
 *
 * Note:
 * - This section defines global execution scheduling.
 * - Card-local logic semantics (DI/DO/SIO modes, phase transitions, repeat
 * logic, etc.) are defined in their respective contract sections.
 *
 *
 * ============================================================================================
 * 6) DIGITAL INPUT (DI) CONTRACT
 * ============================================================================================
 *
 * SET behavior:
 * Acts as gated enable.
 *
 * DI processing (edge detection, debounce, counter) occurs only when:
 * setCondition == true
 *
 * RESET behavior:
 * Clears:
 * - logicalState
 * - triggerFlag
 * - currentValue
 * - timing registers
 *
 * Also inhibits DI processing while true.
 *
 * Always update physical state (invert adjusted) in all the conditions.
 *
 * ============================================================================================
 * 7) DIGITAL OUTPUT (DO) & SOFTIO CONTRACT
 * ============================================================================================
 *
 * Trigger Model:
 * Mode switch behavior:
 * - DO_Normal:
 *   On trigger: Idle -> OnDelay -> Active -> repeat/finalize.
 * - DO_Immediate:
 *   On trigger: Idle -> Active (first cycle only).
 *   For cycles 2..N: OnDelay -> Active.
 * - DO_Gated:
 *   Same timing sequence as DO_Normal, but setCondition must stay true.
 *   If setCondition drops during OnDelay or Active, abort immediately to Idle.
 * setCondition rising edge → triggerFlag (one cycle)
 * triggerFlag → latch logicalState = true  (non-retriggerable while running)
 *
 * Non-Retriggerable:
 * New setCondition rising edges are ignored while state is not
 * State_DO_Idle or State_DO_Finished.
 *
 * Phases per cycle:
 * Canonical sequence: Idle -> OnDelay -> Active -> (repeat or Finished/Idle)
 *
 * Idle → OnDelay → Active → (repeat or Finished/Idle)
 *
 * Repeat logic:
 * - setting3 = 0     → repeat forever until reset
 * - setting3 = 1     → single cycle then complete
 * - setting3 = N > 1 → exactly N full cycles then complete
 *
 * Mission Completion:
 *
 * On final cycle completion:
 * - logicalState = false
 * - state = State_DO_Finished
 * - counter left at final value
 *
 * Card becomes re-armable.
 *
 * RESET:
 * - Immediate hard stop
 * - Clears counter, timers, logicalState
 * - Forces Idle
 * - Has highest priority over mode execution
 *
 * Execution Note:
 * - Retriggering is blocked while running (re-arm only in Idle or Finished).
 * - DO_Gated aborts mission on gate loss during OnDelay/Active.
 * - resetCondition always preempts all modes.
 *
 *
 * ============================================================================================
 * 8) ANALOG INPUT (AI) CONTRACT
 * ============================================================================================
 *
 * AI philosophy:
 * - AI cards are pure sensor transducers.
 * - AI is stateless except for EMA accumulation in currentValue.
 * - AI is evaluated every scan tick in deterministic order.
 * - AI is not mission-driven, event-triggered, latched, or phase-based.
 *
 * AI per-scan deterministic pipeline:
 * 1) Sample: read raw analog value from card hwPin.
 * 2) Clamp: constrain raw sample to [setting1, setting2].
 * 3) Scale: map clamped value linearly from input range to output range
 *    [startOnMs, startOffMs].
 * 4) Filter: apply EMA using setting3 alpha (0..1000 => 0.0..1.0).
 * 5) Store: write filtered result to currentValue for downstream consumption.
 *
 * AI gating and reset behavior:
 * - AI processing is never gated by setCondition or resetCondition.
 * - AI processing is never paused by set/reset groups.
 * - set/reset fields are schema placeholders for AI in this phase.
 *
 * AI range behavior:
 * - Raw input below setting1 clips to setting1.
 * - Raw input above setting2 clips to setting2.
 * - Values in range are linearly interpolated to [startOnMs, startOffMs].
 * - Directionality comes from endpoint assignment; no invert flag is required.
 *
 * AI numeric domain:
 * - All AI numeric parameters are non-negative unsigned values.
 * - No negative ranges or negative accumulator states are supported.
 *
 * AI fixed-point convention:
 * - AI numeric/storage conventions are defined by Section 10.
 *
 * ============================================================================================
 * 9) SYSTEM IDENTITY
 * ============================================================================================
 * This system is:
 *
 * A configurable automation kernel.
 * A composable logic engine.
 * A PLC-inspired embedded core.
 *
 * It is not:
 * A fixed-function timer.
 * A ladder-logic PLC.
 *
 * Core Idea:
 *
 * From hardcoded firmware
 * → To behavior-defined automation.
 *
 *
 * ============================================================================================
 * 10) FIXED-POINT DECIMAL CONVENTION (2 DECIMAL PLACES)
 * ============================================================================================
 *
 * Core rule:
 * - Values requiring decimals are stored as uint32_t centiunits (value x 100).
 * - JSON numeric storage in this phase uses integer centiunits.
 *
 * Conversion:
 * - display = stored / 100
 * - stored = round_half_away(display * 100)
 *
 * Field scope (minimal):
 * | Field(s) | Contract |
 * | --- | --- |
 * | setting1, setting2 | DI/DO/SIO timing + AI input range -> centiunits |
 * | startOnMs, startOffMs | AI scaling endpoints -> centiunits |
 * | setA_Threshold, setB_Threshold, resetA_Threshold, resetB_Threshold |
 * Numeric condition thresholds -> centiunits | | setting3 (DO/SIO, DI) |
 * Integer repeat/reserved (not centiunits) | | setting3 (AI) | Alpha in
 * milliunits 0..1000 (not centiunits) | | currentValue | AI -> centiunits;
 * DI/DO/SIO -> integer counters |
 *
 * AI scaling (centiunit arithmetic):
 * - scaled = startOnMs + (clamped - setting1) * (startOffMs - startOnMs) /
 *   (setting2 - setting1)
 *
 * Constraints:
 * - Numeric domain is non-negative unsigned.
 * - AI input bounds: setting1 <= setting2.
 * - AI output endpoints may be increasing or decreasing
 *   (startOnMs may be < or > startOffMs).
 *
 * Migration note (future work):
 * - Legacy millisecond/unitless configs require schema-versioned migration in
 *   a later phase; not defined here.
 *
 **********************************************************************************************/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include <cstring>

const uint8_t DI_Pins[] = {13, 12, 14, 27};  // Digital Input pins
const uint8_t DO_Pins[] = {26, 25, 33, 32};  // Digital Output pins
const uint8_t AI_Pins[] = {35, 34};          // Analog Input pins
const uint8_t SIO_Pins[] = {255, 255, 255, 255};
// SoftIO has no physical pins, so we can use 255 as a placeholder for "virtual"

// --- Card counts (can be changed later) ---
const uint8_t NUM_DI = sizeof(DI_Pins) / sizeof(DI_Pins[0]);
const uint8_t NUM_DO = sizeof(DO_Pins) / sizeof(DO_Pins[0]);
const uint8_t NUM_AI = sizeof(AI_Pins) / sizeof(AI_Pins[0]);
const uint8_t NUM_SIO = sizeof(SIO_Pins) / sizeof(SIO_Pins[0]);

const uint8_t TOTAL_CARDS = NUM_DI + NUM_DO + NUM_AI + NUM_SIO;

const uint8_t DI_START = 0;
const uint8_t DO_START = DI_START + NUM_DI;
const uint8_t AI_START = DO_START + NUM_DO;
const uint8_t SIO_START = AI_START + NUM_AI;
const char* kConfigPath = "/config.json";
const uint32_t SCAN_INTERVAL_MS = 10;

#ifndef LOGIC_ENGINE_DEBUG
#define LOGIC_ENGINE_DEBUG 0
#endif

#if LOGIC_ENGINE_DEBUG
#define LOGIC_DEBUG_PRINTLN(x) Serial.println(x)
#else
#define LOGIC_DEBUG_PRINTLN(x) \
  do {                         \
  } while (0)
#endif

#define LIST_CARD_TYPES(X) \
  X(DigitalInput)          \
  X(DigitalOutput)         \
  X(AnalogInput)           \
  X(SoftIO)

#define LIST_OPERATORS(X) \
  X(Op_AlwaysTrue)        \
  X(Op_AlwaysFalse)       \
  X(Op_LogicalTrue)       \
  X(Op_LogicalFalse)      \
  X(Op_PhysicalOn)        \
  X(Op_PhysicalOff)       \
  X(Op_Triggered)         \
  X(Op_TriggerCleared)    \
  X(Op_GT)                \
  X(Op_LT)                \
  X(Op_EQ)                \
  X(Op_NEQ)               \
  X(Op_GTE)               \
  X(Op_LTE)               \
  X(Op_Running)           \
  X(Op_Finished)          \
  X(Op_Stopped)

#define LIST_MODES(X)   \
  X(Mode_None)          \
  X(Mode_DI_Rising)     \
  X(Mode_DI_Falling)    \
  X(Mode_DI_Change)     \
  X(Mode_AI_Continuous) \
  X(Mode_DO_Normal)     \
  X(Mode_DO_Immediate)  \
  X(Mode_DO_Gated)

#define LIST_STATES(X)  \
  X(State_None)         \
  X(State_DI_Idle)      \
  X(State_DI_Filtering) \
  X(State_DI_Qualified) \
  X(State_DI_Inhibited) \
  X(State_AI_Streaming) \
  X(State_DO_Idle)      \
  X(State_DO_OnDelay)   \
  X(State_DO_Active)    \
  X(State_DO_Finished)

#define LIST_COMBINE(X) \
  X(Combine_None)       \
  X(Combine_AND)        \
  X(Combine_OR)

#define as_enum(name) name,
enum logicCardType { LIST_CARD_TYPES(as_enum) };
enum logicOperator { LIST_OPERATORS(as_enum) };
enum cardMode { LIST_MODES(as_enum) };
enum cardState { LIST_STATES(as_enum) };
enum combineMode { LIST_COMBINE(as_enum) };
#undef as_enum

#define ENUM_TO_STRING_CASE(name) \
  case name:                      \
    return #name;

#define ENUM_TRY_PARSE_IF(name) \
  if (strcmp(s, #name) == 0) {  \
    out = name;                 \
    return true;                \
  }

const char* toString(logicCardType value) {
  switch (value) { LIST_CARD_TYPES(ENUM_TO_STRING_CASE) }
  return "DigitalInput";
}

const char* toString(logicOperator value) {
  switch (value) { LIST_OPERATORS(ENUM_TO_STRING_CASE) }
  return "Op_AlwaysTrue";
}

const char* toString(cardMode value) {
  switch (value) { LIST_MODES(ENUM_TO_STRING_CASE) }
  return "Mode_None";
}

const char* toString(cardState value) {
  switch (value) { LIST_STATES(ENUM_TO_STRING_CASE) }
  return "State_None";
}

const char* toString(combineMode value) {
  switch (value) { LIST_COMBINE(ENUM_TO_STRING_CASE) }
  return "Combine_None";
}

bool tryParseLogicCardType(const char* s, logicCardType& out) {
  if (s == nullptr) return false;
  LIST_CARD_TYPES(ENUM_TRY_PARSE_IF)
  return false;
}

bool tryParseLogicOperator(const char* s, logicOperator& out) {
  if (s == nullptr) return false;
  LIST_OPERATORS(ENUM_TRY_PARSE_IF)
  return false;
}

bool tryParseCardMode(const char* s, cardMode& out) {
  if (s == nullptr) return false;
  LIST_MODES(ENUM_TRY_PARSE_IF)
  return false;
}

bool tryParseCardState(const char* s, cardState& out) {
  if (s == nullptr) return false;
  LIST_STATES(ENUM_TRY_PARSE_IF)
  return false;
}

bool tryParseCombineMode(const char* s, combineMode& out) {
  if (s == nullptr) return false;
  LIST_COMBINE(ENUM_TRY_PARSE_IF)
  return false;
}

logicCardType parseOrDefault(const char* s, logicCardType fallback) {
  logicCardType value = fallback;
  if (tryParseLogicCardType(s, value)) return value;
  return fallback;
}

logicOperator parseOrDefault(const char* s, logicOperator fallback) {
  logicOperator value = fallback;
  if (tryParseLogicOperator(s, value)) return value;
  return fallback;
}

cardMode parseOrDefault(const char* s, cardMode fallback) {
  cardMode value = fallback;
  if (tryParseCardMode(s, value)) return value;
  return fallback;
}

cardState parseOrDefault(const char* s, cardState fallback) {
  cardState value = fallback;
  if (tryParseCardState(s, value)) return value;
  return fallback;
}

combineMode parseOrDefault(const char* s, combineMode fallback) {
  combineMode value = fallback;
  if (tryParseCombineMode(s, value)) return value;
  return fallback;
}

#undef ENUM_TO_STRING_CASE
#undef ENUM_TRY_PARSE_IF

struct LogicCard {
  // Global unique card ID used by set/reset reference and lookup.
  uint8_t id;
  // Type family of the card (DI, DO, AI, SIO).
  logicCardType type;
  // Position index within its family (e.g. DI0=0, DI1=1, DO0=0, SIO2=2).
  uint8_t index;
  // Hardware pin number for physical cards (255 for virtual SoftIO).
  uint8_t hwPin;

  // Active Low / Inverted Polarity flag.
  bool invert;

  // DI: debounce duration for DI
  // DO/SIO: on-delay duration (wait before activating output)
  // For DO/SIO 0 means infinite wait to on delay until reset
  // AI: input minimum (raw ADC/sensor lower bound)
  uint32_t setting1;
  // DI: reserved for future use
  // DO/SIO: off-delay duration (turn off output after interval ends)
  // For DO/SIO 0 means infinite wait to off delay until reset
  // AI: input maximum (raw ADC/sensor upper bound)
  uint32_t setting2;
  // DI: reserved for future use
  // DO/SIO: repeat count (0 = infinite, 1 = single pulse, N = N full cycles)
  // AI: EMA alpha in range 0..1000 (represents 0.0..1.0)
  uint32_t setting3;

  // DI: qualified logical state after debounce/qualification when setCondition
  // is true DO/SIO: mission latch indicating active cycle (set on trigger,
  // cleared on completion/reset)
  // AI: unused placeholder in this phase
  bool logicalState;
  // DI: physical input state after polarity adjustment not considering
  // set/reset conditions DO/SIO: effective output state considering timing and
  // mission state
  // AI: unused placeholder in this phase
  bool physicalState;
  // DI: edge-triggered one cycle pulse generated when logicalState transitions
  // to true DO/SIO: one cycle pulse generated on setCondition rising edge to
  // trigger mission start
  // AI: unused placeholder in this phase
  bool triggerFlag;

  // DI: event counter incremented on each qualified edge when setCondition is
  // true DO/SIO: cycle counter incremented on each physical rising edge to
  // track repeat cycles Reset to 0 on when reset condition is met for DI/DO/SIO
  // AI: EMA accumulator and filtered output storage
  uint32_t currentValue;
  // DI: timestamp of last qualified edge for debounce timing
  // DO/SIO: timestamp when current ON-phase started for timing control
  // AI: output minimum (scaled physical lower bound)
  uint32_t startOnMs;
  // DI: timestamp of last qualified edge for debounce timing
  // DO/SIO: timestamp when current OFF-phase started for timing control
  // AI: output maximum (scaled physical upper bound)
  uint32_t startOffMs;
  // DI: unused for now, reserved for future use (e.g. long-press tracking)
  // DO/SIO: counts completed cycles for repeat logic to determine when to
  // finish
  // AI: unused placeholder in this phase
  uint32_t repeatCounter;

  // DI: edge/debounce mode (rising, falling, change)
  // DO/SIO: execution mode switch for timing semantics
  // (DO_Normal/DO_Immediate/DO_Gated) Legacy mode values are deprecated and
  // kept for schema compatibility only.
  // AI: placeholder mode tag (Mode_AI_Continuous), no runtime effect yet
  cardMode mode;
  // DI: filtering lifecycle state (Idle, Filtering, Qualified, Inhibited)
  // DO/SIO: phase state (Idle, OnDelay, Active, Finished)
  // AI: placeholder state tag (State_AI_Streaming), no phase machine
  cardState state;

  // SET Group
  uint8_t setA_ID;
  logicOperator setA_Operator;
  uint32_t setA_Threshold;

  uint8_t setB_ID;
  logicOperator setB_Operator;
  uint32_t setB_Threshold;

  combineMode setCombine;

  // RESET Group
  uint8_t resetA_ID;
  logicOperator resetA_Operator;
  uint32_t resetA_Threshold;

  uint8_t resetB_ID;
  logicOperator resetB_Operator;
  uint32_t resetB_Threshold;

  combineMode resetCombine;
};
LogicCard logicCards[TOTAL_CARDS] = {};
bool gPrevSetCondition[TOTAL_CARDS] = {};
bool gPrevDISample[TOTAL_CARDS] = {};
bool gPrevDIPrimed[TOTAL_CARDS] = {};

void serializeCardToJson(const LogicCard& card, JsonObject& json) {
  json["id"] = card.id;
  json["type"] = toString(card.type);
  json["index"] = card.index;
  json["hwPin"] = card.hwPin;
  json["invert"] = card.invert;

  json["setting1"] = card.setting1;
  json["setting2"] = card.setting2;
  json["setting3"] = card.setting3;

  json["logicalState"] = card.logicalState;
  json["physicalState"] = card.physicalState;
  json["triggerFlag"] = card.triggerFlag;
  json["currentValue"] = card.currentValue;
  json["startOnMs"] = card.startOnMs;
  json["startOffMs"] = card.startOffMs;
  json["repeatCounter"] = card.repeatCounter;

  json["mode"] = toString(card.mode);
  json["state"] = toString(card.state);

  json["setA_ID"] = card.setA_ID;
  json["setA_Operator"] = toString(card.setA_Operator);
  json["setA_Threshold"] = card.setA_Threshold;
  json["setB_ID"] = card.setB_ID;
  json["setB_Operator"] = toString(card.setB_Operator);
  json["setB_Threshold"] = card.setB_Threshold;
  json["setCombine"] = toString(card.setCombine);

  json["resetA_ID"] = card.resetA_ID;
  json["resetA_Operator"] = toString(card.resetA_Operator);
  json["resetA_Threshold"] = card.resetA_Threshold;
  json["resetB_ID"] = card.resetB_ID;
  json["resetB_Operator"] = toString(card.resetB_Operator);
  json["resetB_Threshold"] = card.resetB_Threshold;
  json["resetCombine"] = toString(card.resetCombine);
}

void deserializeCardFromJson(JsonObjectConst json, LogicCard& card) {
  card.id = json["id"] | card.id;
  card.type = parseOrDefault(json["type"] | nullptr, DigitalInput);
  card.index = json["index"] | card.index;
  card.hwPin = json["hwPin"] | card.hwPin;
  card.invert = json["invert"] | card.invert;

  card.setting1 = json["setting1"] | card.setting1;
  card.setting2 = json["setting2"] | card.setting2;
  card.setting3 = json["setting3"] | card.setting3;

  card.logicalState = json["logicalState"] | card.logicalState;
  card.physicalState = json["physicalState"] | card.physicalState;
  card.triggerFlag = json["triggerFlag"] | card.triggerFlag;
  card.currentValue = json["currentValue"] | card.currentValue;
  card.startOnMs = json["startOnMs"] | card.startOnMs;
  card.startOffMs = json["startOffMs"] | card.startOffMs;
  card.repeatCounter = json["repeatCounter"] | card.repeatCounter;

  card.mode = parseOrDefault(json["mode"] | nullptr, Mode_None);
  card.state = parseOrDefault(json["state"] | nullptr, State_None);

  card.setA_ID = json["setA_ID"] | card.setA_ID;
  card.setA_Operator =
      parseOrDefault(json["setA_Operator"] | nullptr, Op_AlwaysFalse);
  card.setA_Threshold = json["setA_Threshold"] | card.setA_Threshold;
  card.setB_ID = json["setB_ID"] | card.setB_ID;
  card.setB_Operator =
      parseOrDefault(json["setB_Operator"] | nullptr, Op_AlwaysFalse);
  card.setB_Threshold = json["setB_Threshold"] | card.setB_Threshold;
  card.setCombine = parseOrDefault(json["setCombine"] | nullptr, Combine_None);

  card.resetA_ID = json["resetA_ID"] | card.resetA_ID;
  card.resetA_Operator =
      parseOrDefault(json["resetA_Operator"] | nullptr, Op_AlwaysFalse);
  card.resetA_Threshold = json["resetA_Threshold"] | card.resetA_Threshold;
  card.resetB_ID = json["resetB_ID"] | card.resetB_ID;
  card.resetB_Operator =
      parseOrDefault(json["resetB_Operator"] | nullptr, Op_AlwaysFalse);
  card.resetB_Threshold = json["resetB_Threshold"] | card.resetB_Threshold;
  card.resetCombine =
      parseOrDefault(json["resetCombine"] | nullptr, Combine_None);
}

void initializeCardSafeDefaults(LogicCard& card, uint8_t globalId) {
  card.id = globalId;
  card.invert = false;
  card.setting1 = 0;
  card.setting2 = 0;
  card.setting3 = 0;
  card.logicalState = false;
  card.physicalState = false;
  card.triggerFlag = false;
  card.currentValue = 0;
  card.startOnMs = 0;
  card.startOffMs = 0;
  card.repeatCounter = 0;

  card.setA_ID = 0;
  card.setB_ID = 0;
  card.resetA_ID = 0;
  card.resetB_ID = 0;
  card.setA_Operator = Op_AlwaysFalse;
  card.setB_Operator = Op_AlwaysFalse;
  card.resetA_Operator = Op_AlwaysFalse;
  card.resetB_Operator = Op_AlwaysFalse;
  card.setA_Threshold = 0;
  card.setB_Threshold = 0;
  card.resetA_Threshold = 0;
  card.resetB_Threshold = 0;
  card.setCombine = Combine_None;
  card.resetCombine = Combine_None;

  if (globalId < DO_START) {
    card.type = DigitalInput;
    card.index = globalId - DI_START;
    card.hwPin = DI_Pins[card.index];
    card.mode = Mode_None;
    card.state = State_DI_Idle;
    return;
  }

  if (globalId < AI_START) {
    card.type = DigitalOutput;
    card.index = globalId - DO_START;
    card.hwPin = DO_Pins[card.index];
    card.mode = Mode_DO_Normal;
    card.state = State_DO_Idle;
    return;
  }

  if (globalId < SIO_START) {
    card.type = AnalogInput;
    card.index = globalId - AI_START;
    card.hwPin = AI_Pins[card.index];
    card.mode = Mode_AI_Continuous;
    card.state = State_AI_Streaming;
    return;
  }

  card.type = SoftIO;
  card.index = globalId - SIO_START;
  card.hwPin = SIO_Pins[card.index];
  card.mode = Mode_DO_Normal;
  card.state = State_DO_Idle;
}

void initializeAllCardsSafeDefaults() {
  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    initializeCardSafeDefaults(logicCards[i], i);
  }
}

bool saveLogicCardsToLittleFS() {
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    JsonObject obj = array.add<JsonObject>();
    serializeCardToJson(logicCards[i], obj);
  }

  File file = LittleFS.open(kConfigPath, "w");
  if (!file) return false;
  size_t written = serializeJson(doc, file);
  file.close();
  return written > 0;
}

bool loadLogicCardsFromLittleFS() {
  File file = LittleFS.open(kConfigPath, "r");
  if (!file) return false;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) return false;
  if (!doc.is<JsonArray>()) return false;

  JsonArrayConst array = doc.as<JsonArrayConst>();
  if (array.size() != TOTAL_CARDS) return false;

  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    JsonVariantConst item = array[i];
    if (!item.is<JsonObjectConst>()) return false;
    deserializeCardFromJson(item.as<JsonObjectConst>(), logicCards[i]);
  }
  return true;
}

void printLogicCardsJsonToSerial(const char* label) {
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    JsonObject obj = array.add<JsonObject>();
    serializeCardToJson(logicCards[i], obj);
  }

  Serial.println(label);
  serializeJsonPretty(doc, Serial);
  Serial.println();
}

void configureHardwarePinsSafeState() {
  for (uint8_t i = 0; i < NUM_DO; ++i) {
    pinMode(DO_Pins[i], OUTPUT);
    digitalWrite(DO_Pins[i], LOW);
  }
  for (uint8_t i = 0; i < NUM_DI; ++i) {
    pinMode(DI_Pins[i], INPUT_PULLUP);
  }
}

void bootstrapCardsFromStorage() {
  initializeAllCardsSafeDefaults();
  if (loadLogicCardsFromLittleFS()) {
    printLogicCardsJsonToSerial("Loaded JSON from /config.json:");
    return;
  }

  initializeAllCardsSafeDefaults();
  if (saveLogicCardsToLittleFS()) {
    printLogicCardsJsonToSerial("Saved default JSON to /config.json:");
  } else {
    Serial.println("Failed to save default JSON to /config.json");
  }
}

LogicCard* getCardById(uint8_t id) {
  if (id >= TOTAL_CARDS) return nullptr;
  return &logicCards[id];
}

bool isDoRunningState(cardState state) {
  return state == State_DO_OnDelay || state == State_DO_Active;
}

bool evalOperator(const LogicCard& target, logicOperator op,
                  uint32_t threshold) {
  switch (op) {
    case Op_AlwaysTrue:
      return true;
    case Op_AlwaysFalse:
      return false;
    case Op_LogicalTrue:
      return target.logicalState;
    case Op_LogicalFalse:
      return !target.logicalState;
    case Op_PhysicalOn:
      return target.physicalState;
    case Op_PhysicalOff:
      return !target.physicalState;
    case Op_Triggered:
      return target.triggerFlag;
    case Op_TriggerCleared:
      return !target.triggerFlag;
    case Op_GT:
      return target.currentValue > threshold;
    case Op_LT:
      return target.currentValue < threshold;
    case Op_EQ:
      return target.currentValue == threshold;
    case Op_NEQ:
      return target.currentValue != threshold;
    case Op_GTE:
      return target.currentValue >= threshold;
    case Op_LTE:
      return target.currentValue <= threshold;
    case Op_Running:
      return isDoRunningState(target.state);
    case Op_Finished:
      return target.state == State_DO_Finished;
    case Op_Stopped:
      return target.state == State_DO_Idle || target.state == State_DO_Finished;
    default:
      return false;
  }
}

bool evalCondition(uint8_t aId, logicOperator aOp, uint32_t aTh, uint8_t bId,
                   logicOperator bOp, uint32_t bTh, combineMode combine) {
  const LogicCard* aCard = getCardById(aId);
  bool aResult = (aCard != nullptr) ? evalOperator(*aCard, aOp, aTh) : false;
  if (combine == Combine_None) return aResult;

  const LogicCard* bCard = getCardById(bId);
  bool bResult = (bCard != nullptr) ? evalOperator(*bCard, bOp, bTh) : false;

  if (combine == Combine_AND) return aResult && bResult;
  if (combine == Combine_OR) return aResult || bResult;
  return false;
}

void resetDIRuntime(LogicCard& card) {
  card.logicalState = false;
  card.triggerFlag = false;
  card.currentValue = 0;
  card.startOnMs = 0;
  card.startOffMs = 0;
  card.repeatCounter = 0;
}

void processDICard(LogicCard& card, uint32_t nowMs) {
  bool sample = false;
  if (card.hwPin != 255) {
    sample = (digitalRead(card.hwPin) == HIGH);
  }
  if (card.invert) sample = !sample;
  card.physicalState = sample;

  const bool setCondition = evalCondition(
      card.setA_ID, card.setA_Operator, card.setA_Threshold, card.setB_ID,
      card.setB_Operator, card.setB_Threshold, card.setCombine);
  const bool resetCondition =
      evalCondition(card.resetA_ID, card.resetA_Operator, card.resetA_Threshold,
                    card.resetB_ID, card.resetB_Operator, card.resetB_Threshold,
                    card.resetCombine);

  if (resetCondition) {
    resetDIRuntime(card);
    card.state = State_DI_Inhibited;
    return;
  }

  if (!setCondition) {
    card.triggerFlag = false;
    card.state = State_DI_Idle;
    return;
  }

  bool previousSample = sample;
  if (card.id < TOTAL_CARDS) {
    if (gPrevDIPrimed[card.id]) {
      previousSample = gPrevDISample[card.id];
    }
    gPrevDISample[card.id] = sample;
    gPrevDIPrimed[card.id] = true;
  }

  const bool risingEdge = (!previousSample && sample);
  const bool fallingEdge = (previousSample && !sample);
  bool edgeMatchesMode = false;
  switch (card.mode) {
    case Mode_DI_Rising:
      edgeMatchesMode = risingEdge;
      break;
    case Mode_DI_Falling:
      edgeMatchesMode = fallingEdge;
      break;
    case Mode_DI_Change:
      edgeMatchesMode = risingEdge || fallingEdge;
      break;
    default:
      edgeMatchesMode = false;
      break;
  }

  if (!edgeMatchesMode) {
    card.triggerFlag = false;
    card.state = State_DI_Idle;
    return;
  }

  const uint32_t elapsed = nowMs - card.startOnMs;
  if (card.setting1 > 0 && elapsed < card.setting1) {
    card.triggerFlag = false;
    card.state = State_DI_Filtering;
    return;
  }

  card.triggerFlag = true;
  card.currentValue += 1;
  card.logicalState = sample;
  card.startOnMs = nowMs;
  card.state = State_DI_Qualified;
}

uint32_t clampUInt32(uint32_t value, uint32_t lo, uint32_t hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

void processAICard(LogicCard& card) {
  uint32_t raw = 0;
  if (card.hwPin != 255) {
    raw = static_cast<uint32_t>(analogRead(card.hwPin));
  }

  const uint32_t inMin =
      (card.setting1 < card.setting2) ? card.setting1 : card.setting2;
  const uint32_t inMax =
      (card.setting1 < card.setting2) ? card.setting2 : card.setting1;
  const uint32_t clamped = clampUInt32(raw, inMin, inMax);

  uint32_t scaled = card.startOnMs;
  if (inMax != inMin) {
    const int64_t outMin = static_cast<int64_t>(card.startOnMs);
    const int64_t outMax = static_cast<int64_t>(card.startOffMs);
    const int64_t outDelta = outMax - outMin;
    const int64_t inDelta = static_cast<int64_t>(inMax - inMin);
    const int64_t inOffset = static_cast<int64_t>(clamped - inMin);
    int64_t mapped = outMin + ((inOffset * outDelta) / inDelta);
    if (mapped < 0) mapped = 0;
    scaled = static_cast<uint32_t>(mapped);
  }

  const uint32_t alpha = (card.setting3 > 1000) ? 1000 : card.setting3;
  const uint64_t filtered =
      ((static_cast<uint64_t>(alpha) * scaled) +
       (static_cast<uint64_t>(1000 - alpha) * card.currentValue)) /
      1000ULL;
  card.currentValue = static_cast<uint32_t>(filtered);
  card.mode = Mode_AI_Continuous;
  card.state = State_AI_Streaming;
}

void forceDOIdle(LogicCard& card) {
  card.logicalState = false;
  card.physicalState = false;
  card.triggerFlag = false;
  card.startOnMs = 0;
  card.startOffMs = 0;
  card.repeatCounter = 0;
  card.state = State_DO_Idle;
}

void driveDOHardware(const LogicCard& card, bool driveHardware, bool level) {
  if (!driveHardware) return;
  if (card.hwPin == 255) return;
  digitalWrite(card.hwPin, level ? HIGH : LOW);
}

void processDOCard(LogicCard& card, uint32_t nowMs, bool driveHardware) {
  const bool setCondition = evalCondition(
      card.setA_ID, card.setA_Operator, card.setA_Threshold, card.setB_ID,
      card.setB_Operator, card.setB_Threshold, card.setCombine);
  const bool resetCondition =
      evalCondition(card.resetA_ID, card.resetA_Operator, card.resetA_Threshold,
                    card.resetB_ID, card.resetB_Operator, card.resetB_Threshold,
                    card.resetCombine);

  bool prevSet = false;
  if (card.id < TOTAL_CARDS) {
    prevSet = gPrevSetCondition[card.id];
    gPrevSetCondition[card.id] = setCondition;
  }
  const bool setRisingEdge = setCondition && !prevSet;

  if (resetCondition) {
    forceDOIdle(card);
    driveDOHardware(card, driveHardware, false);
    return;
  }

  const bool retriggerable =
      (card.state == State_DO_Idle || card.state == State_DO_Finished);
  card.triggerFlag = setRisingEdge && retriggerable;

  if (card.triggerFlag) {
    card.logicalState = true;
    card.repeatCounter = 0;
    if (card.mode == Mode_DO_Immediate) {
      card.state = State_DO_Active;
      card.startOffMs = nowMs;
    } else {
      card.state = State_DO_OnDelay;
      card.startOnMs = nowMs;
    }
  }

  if (card.mode == Mode_DO_Gated && isDoRunningState(card.state) &&
      !setCondition) {
    forceDOIdle(card);
    driveDOHardware(card, driveHardware, false);
    return;
  }

  bool effectiveOutput = false;
  switch (card.state) {
    case State_DO_OnDelay: {
      effectiveOutput = false;
      if (card.setting1 == 0) {
        break;
      }
      if ((nowMs - card.startOnMs) >= card.setting1) {
        card.state = State_DO_Active;
        card.startOffMs = nowMs;
        effectiveOutput = true;
      }
      break;
    }
    case State_DO_Active: {
      effectiveOutput = true;
      if (card.setting2 == 0) {
        break;
      }
      if ((nowMs - card.startOffMs) >= card.setting2) {
        card.repeatCounter += 1;
        effectiveOutput = false;

        if (card.setting3 == 0) {
          card.state = State_DO_OnDelay;
          card.startOnMs = nowMs;
          break;
        }

        if (card.repeatCounter >= card.setting3) {
          card.logicalState = false;
          card.state = State_DO_Finished;
          break;
        }

        card.state = State_DO_OnDelay;
        card.startOnMs = nowMs;
      }
      break;
    }
    case State_DO_Finished:
    case State_DO_Idle:
    default:
      effectiveOutput = false;
      break;
  }

  card.physicalState = effectiveOutput;
  driveDOHardware(card, driveHardware, effectiveOutput);
}

void processSIOCard(LogicCard& card, uint32_t nowMs) {
  processDOCard(card, nowMs, false);
}

void runScanCycle(uint32_t nowMs) {
  for (uint8_t i = DI_START; i < DO_START; ++i) {
    processDICard(logicCards[i], nowMs);
  }
  for (uint8_t i = AI_START; i < SIO_START; ++i) {
    processAICard(logicCards[i]);
  }
  for (uint8_t i = SIO_START; i < TOTAL_CARDS; ++i) {
    processSIOCard(logicCards[i], nowMs);
  }
  for (uint8_t i = DO_START; i < AI_START; ++i) {
    processDOCard(logicCards[i], nowMs, true);
  }
}

void setup() {
  Serial.begin(115200);
  configureHardwarePinsSafeState();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    initializeAllCardsSafeDefaults();
    return;
  }

  bootstrapCardsFromStorage();
}

void loop() {
  static uint32_t lastScanMs = 0;
  uint32_t nowMs = millis();
  if (lastScanMs == 0) {
    lastScanMs = nowMs;
  }

  if ((nowMs - lastScanMs) >= SCAN_INTERVAL_MS) {
    runScanCycle(nowMs);
    lastScanMs += SCAN_INTERVAL_MS;
  }
}
