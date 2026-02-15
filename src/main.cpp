/**********************************************************************************************
 * Advanced Timer — DESIGN PHILOSOPHY AND PRODUCT VISION
 *
 * This project is not a traditional timer relay, nor a full PLC.
 * It is a new class of automation controller designed to bridge the gap between
 * simple programmable timers and complex industrial PLC systems.
 *
 * ============================================================================================
 * 1) LIMITATIONS OF EXISTING TIMER RELAYS
 * ============================================================================================
 *
 * Conventional timer relays provide a set of predefined operating modes
 * (e.g., on-delay, off-delay, cyclic, one-shot, triggered pulse).
 *
 * In these devices:
 *
 * - Logic is fixed by the manufacturer.
 * - Users can only select a mode and adjust parameters (time, loop count,
 * trigger type).
 * - Relationships between multiple signals cannot be expressed.
 * - Complex conditions (AND, OR, dependencies, chaining) are impossible.
 *
 * Conceptually, a timer relay is equivalent to a single hardcoded state machine
 * with limited configuration options.
 *
 * ============================================================================================
 * 2) LIMITATIONS OF TRADITIONAL PLC SYSTEMS
 * ============================================================================================
 *
 * PLCs solve the flexibility problem but introduce a new barrier:
 *
 * - Users must learn programming concepts (ladder logic, function blocks,
 * structured text).
 * - Configuration requires specialized software tools and engineering
 * workflows.
 * - Debugging and modification require technical expertise.
 * - Cost and complexity are high for small-scale applications.
 *
 * For many users, PLCs are powerful but inaccessible.
 *
 * ============================================================================================
 * 3) CORE IDEA OF THE Advanced Timer
 * ============================================================================================
 *
 * The Advanced Timer introduces a different paradigm:
 *
 * - System behavior is defined by structured logic units called "LogicCards".
 * - Each LogicCard represents a modular state machine with configurable
 * behavior.
 * - Logic is expressed using set/reset rules, conditions, timing, and states.
 *
 * Users do not write programs.
 * Users describe behavior.
 *
 * Instead of selecting a fixed mode, users compose logic by connecting
 * LogicCards through conditions and references.
 *
 * ============================================================================================
 * 4) POSITIONING BETWEEN TIMERS AND PLCS
 * ============================================================================================
 *
 * This system is intentionally positioned between two worlds:
 *
 *   - Timer relays → simple but rigid.
 *   - PLCs        → powerful but complex.
 *
 * The Advanced Timer provides:
 *
 * - PLC-level logical flexibility.
 * - Timer-level simplicity of configuration.
 * - Visual and intuitive logic definition.
 * - Configuration-driven behavior instead of code-driven behavior.
 *
 * In essence:
 *
 *   "PLC power without PLC complexity."
 *
 * ============================================================================================
 * 5) TARGET USERS
 * ============================================================================================
 *
 * The system is designed for:
 *
 * - Electricians and technicians who need smarter control logic.
 * - System integrators building small to medium automation systems.
 * - Hobbyists and makers experimenting with automation.
 * - Small factories and workshops that cannot justify full PLC solutions.
 * - Automation beginners who are uncomfortable with traditional PLC
 * programming.
 *
 * These users need real logic capability without software engineering overhead.
 *
 * ============================================================================================
 * 6) WHY LOGICCARD ARCHITECTURE MATTERS
 * ============================================================================================
 *
 * Each LogicCard encapsulates:
 *
 * - Identity (global ID and type).
 * - Mode (behavior pattern).
 * - State (runtime lifecycle).
 * - Parameters (timing, thresholds, counters).
 * - Logic conditions (set/reset rules referencing other cards).
 * - Runtime signals (logical state, physical state, trigger events).
 *
 * By unifying all functional elements (IO and virtual signals) and embedding
 * timing/counting into every LogicCard, the system becomes:
 *
 * - Fully configurable through data (JSON).
 * - Observable and debuggable in real time.
 * - Extensible without rewriting core logic.
 *
 * ============================================================================================
 * 7) LONG-TERM VISION
 * ============================================================================================
 *
 * The Advanced Timer is designed as a foundational automation kernel that
 * can:
 *
 * - Scale from simple timer devices to complex automation controllers.
 * - Serve as a reusable core across multiple hardware platforms.
 * - Enable visual logic design tools and remote configuration interfaces.
 *
 * Ultimately, this project aims to redefine how embedded automation logic is
 * built:
 *
 * - From hardcoded firmware → to configurable logic systems.
 * - From fixed modes → to composable logic blocks.
 * - From programming → to behavior definition.
 *
 **********************************************************************************************/

/**********************************************************************************************
 * LOGIC CARD ENGINE — ARCHITECTURE OVERVIEW
 *
 * This project implements a PLC-like logic engine where every functional unit
 * (Digital IO, Analog IO, and Virtual IO) is represented as a unified entity
 * called a "LogicCard" with built-in timing/counting.
 *
 * The goal is to create a hardware-agnostic, configurable, persistent, and
 * scalable logic system where behavior is defined by configuration rather than
 * hardcoded logic.
 *
 * ============================================================================================
 * 1) CORE DESIGN PRINCIPLES
 * ============================================================================================
 *
 * 1.1 Unified Card Model
 * ----------------------
 * All functional elements share the same data structure (LogicCard).
 * This allows the logic engine to treat all cards uniformly, regardless of
 * their type.
 *
 * Card Types:
 * - DigitalInput  : Physical digital input pins.
 * - DigitalOutput : Physical digital output pins.
 * - AnalogInput   : Physical analog input channels.
 * - SoftIO        : Virtual outputs without hardware pins.
 *
 * Each card is identified by:
 * - id    : Global unique identifier (used in logic references).
 * - type  : Functional category of the card.
 * - index : Position within its type group.
 *
 * --------------------------------------------------------------------------------------------
 *
 * 1.2 Hardware vs Logic Separation
 * --------------------------------
 * LogicCards represent logical behavior and state, not hardware directly.
 *
 * - Logic layer  : Defines behavior, conditions, modes, and states.
 * - Hardware layer: Maps LogicCards to GPIO/ADC/etc. during initialization.
 *
 * This separation allows:
 * - Easy hardware changes without modifying logic.
 * - Simulation and debugging without physical hardware.
 *
 * --------------------------------------------------------------------------------------------
 *
 * 1.3 Persistent Configuration vs Runtime State
 * ---------------------------------------------
 * LogicCards contain two conceptual categories of data:
 *
 * A) Configuration (Persistent, stored in filesystem)
 *    - type, mode, constants, settings, logic conditions, allowRetrigger.
 *
 * B) Runtime State (Volatile, reset on boot)
 *    - logicalState, physicalState, triggerFlag, currentValue,
 *      eventCounter, startOnMs, startOffMs, state.
 *
 * This mirrors real PLC architecture:
 * - Configuration = PLC program (DNA).
 * - Runtime state = Process image (live behavior).
 *
 * ============================================================================================
 * 2) ENUM TYPES — PURPOSE AND BEHAVIOR
 * ============================================================================================
 *
 * 2.1 logicCardType
 * -----------------
 * Defines the fundamental nature of a card.
 *
 * - DigitalInput  : Reads physical digital signals.
 * - DigitalOutput : Drives physical outputs.
 * - AnalogInput   : Reads analog values.
 * - SoftIO        : Virtual logical outputs.
 *
 * Behavior impact:
 * - Determines which signals are meaningful.
 * - Determines how mode and state are interpreted.
 * - Determines whether hardware interaction exists.
 *
 * --------------------------------------------------------------------------------------------
 *
 * 2.2 cardMode
 * ------------
 * Defines how a card behaves logically.
 *
 * DO/SIO canonical usage (current focus):
 * - DO_ActiveHigh : Physical output ON means GPIO HIGH (normal polarity).
 * - DO_ActiveLow  : Physical output ON means GPIO LOW (inverted polarity).
 *
 * Existing DI/AI modes remain for compatibility with previously saved schema.
 * For DO/SIO cards, `mode` is treated as hardware polarity selection.
 *
 * --------------------------------------------------------------------------------------------
 *
 * 2.3 cardState
 * -------------
 * Defines the current internal lifecycle state of a card.
 *
 * DO/SIO canonical phase values:
 * - State_DO_Idle     : Phase 0, waiting for trigger.
 * - State_DO_OnDelay  : Phase 1, ON delay in progress.
 * - State_DO_Active   : Phase 2, active/pulse-high phase.
 * - State_DO_OffDelay : Phase 3, OFF delay in progress.
 *
 * Existing states remain for backward compatibility across card families.
 * For DO/SIO cards, state is the phase indicator other cards can evaluate.
 *
 * --------------------------------------------------------------------------------------------
 *
 * 2.4 logicOperator
 * -----------------
 * Defines how logic conditions evaluate other cards.
 *
 * Categories:
 *
 * Logical state operators:
 * - Op_LogicalTrue
 * - Op_LogicalFalse
 *
 * Physical state operators:
 * - Op_PhysicalOn
 * - Op_PhysicalOff
 *
 * Trigger operators:
 * - Op_Triggered
 * - Op_TriggerCleared
 *
 * Numeric comparison operators:
 * - Op_GT, Op_LT, Op_EQ, Op_NEQ, Op_GTE, Op_LTE
 *
 * Process state operators (timing/counting states):
 * - Op_Running
 * - Op_Finished
 * - Op_Stopped
 *
 * These operators define "how a source card is evaluated".
 *
 * --------------------------------------------------------------------------------------------
 *
 * 2.5 logicCombine
 * ----------------
 * Defines how multiple logic conditions are combined.
 *
 * - Combine_None : Single condition only.
 * - Combine_AND  : Both conditions must be true.
 * - Combine_OR   : Either condition can be true.
 *
 * ============================================================================================
 * 3) LOGIC CARD VARIABLES — MEANING AND USAGE
 * ============================================================================================
 *
 * Identity & Binding:
 * -------------------
 * id       : Global ID used for logic references.
 * type     : Card category (logicCardType).
 * index    : Position within type group.
 * hwPin    : Hardware pin number or 255 if virtual.
 *
 * Constants:
 * ----------
 * constant1 : DO/SIO mission repeat limit.
 *             = 1  : One-shot pulse.
 *             > 1  : Run exactly N cycles.
 *             <= 0 : Infinite repeating (oscillator/blinker mode).
 * constant2 : Reserved for future DO/SIO extensions (schema-compatible).
 *
 * Core Logic Signals:
 * -------------------
 * logicalState  : DO/SIO process latch ("intent/running" state).
 * physicalState : Time-filtered effective output state used by dependents.
 * triggerFlag   : One-cycle ignition pulse from setCondition rising edge.
 *
 * Behavior Control:
 * -----------------
 * allowRetrigger : Ignored/removed behavior, preserved only for schema
 *                  compatibility. DO/SIO model is non-retriggerable.
 *
 * Generic Settings:
 * -----------------
 * setting1 : ON delay / pulse-high duration in milliseconds.
 * setting2 : OFF delay / pulse-low/rest duration in milliseconds.
 *
 * Runtime Value:
 * --------------
 * currentValue  : Persistent cycle counter (increments on physical 0->1 edge).
 * startOnMs     : Timestamp register for ON-phase timing.
 * startOffMs    : Timestamp register for OFF-phase timing.
 *
 * Mode & State:
 * -------------
 * mode  : DO/SIO polarity interface (DO_ActiveHigh or DO_ActiveLow).
 * state : DO/SIO phase indicator (Idle/OnDelay/Active/OffDelay).
 *
 * Logic Conditions:
 * -----------------
 * Each card has two logic groups:
 *
 * SET condition:
 * - setA_ID, setA_Operator, setA_Threshold
 * - setB_ID, setB_Operator, setB_Threshold
 * - setCombine
 *
 * RESET condition:
 * - resetA_ID, resetA_Operator, resetA_Threshold
 * - resetB_ID, resetB_Operator, resetB_Threshold
 * - resetCombine
 *
 * These define when a card should turn ON or OFF.
 *
 * ============================================================================================
 * 4) BEHAVIOR BY CARD TYPE (CONCEPTUAL)
 * ============================================================================================
 *
 * DigitalInput:
 * - Existing behavior remains as previously defined.
 *
 * DigitalOutput:
 * - Executes mission/timing/counting as a self-contained state machine.
 * - Drives hardware pin according to `physicalState` and `mode` polarity.
 *
 * AnalogInput:
 * - Existing behavior remains as previously defined.
 *
 * SoftIO:
 * - Same mission/timing/counting model as DO, but virtual (no hardware pin).
 *
 * Note:
 * - Numeric comparison operators use currentValue.
 *
 * ============================================================================================
 * 5) OVERALL PHILOSOPHY
 * ============================================================================================
 *
 * This engine is designed as a "software PLC kernel":
 *
 * - Logic is data-driven, not hardcoded.
 * - All behavior is configurable and persistent.
 * - Hardware is abstracted from logic.
 * - Every card is a state machine.
 * - Global ID system enables flexible logic routing.
 *
 * The ultimate goal is to build a scalable, debuggable, and UI-configurable
 * automation engine similar in spirit to industrial PLC systems.
 *
 * ============================================================================================
 * 6) DO/SIO BEHAVIOR CONTRACT (REFERENCE FOR NEXT IMPLEMENTATION)
 * ============================================================================================
 *
 * This section is the definitive reference for DigitalOutput and SoftIO cards.
 * DI behavior and DI-related documentation remain intact and preserved.
 *
 * - `setCondition` rising edge generates one-cycle `triggerFlag`.
 * - `triggerFlag` is the only ignition source for process start.
 * - `triggerFlag` latches `logicalState = true` (process starts and stays
 * latched).
 * - Non-retriggerable model: new triggers are ignored while process is active.
 * - `resetCondition == true` is a hard stop:
 *   `logicalState=false`, `physicalState=false`, `currentValue=0`.
 *
 * Variable-by-variable intent:
 * - `id`, `type`, `index`, `hwPin`:
 *   identity and mapping fields; SIO ignores `hwPin` while DO maps
 * `physicalState` to pin.
 * - `constant1`: repeat mission limit (1 one-shot, >1 batch, <=0 infinite).
 * - `constant2`: reserved.
 * - `logicalState`: process latch/intent state.
 * - `physicalState`: effective time-filtered output truth.
 * - `triggerFlag`: one-cycle ignition pulse from set rising edge.
 * - `allowRetrigger`: schema-compatibility field only (ignored by DO/SIO
 * contract).
 * - `setting1`: ON duration / pulse-high time / on-delay window (ms).
 * - `setting2`: OFF duration / pulse-low rest / inter-cycle delay (ms).
 * - `currentValue`: execution odometer; increments on each `physicalState`
 * rising edge.
 * - `startOnMs`, `startOffMs`: internal phase timing registers.
 * - `mode`: output polarity (active-high / active-low).
 * - `state`: phase indicator:
 *   `State_DO_Idle` , `State_DO_OnDelay` ,
 *   `State_DO_Active` , `State_DO_OffDelay` .
 * - `setCondition` and `resetCondition` groups:
 *   set group generates start trigger edges, reset group provides immediate
 * hard stop/clear.
 *
 * DO vs SIO:
 * - DO maps `physicalState` to actual hardware pin via `hwPin`.
 * - SIO uses identical behavior as virtual signal (`hwPin` ignored).
 *
 **********************************************************************************************/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <TickTwo.h>

// --- Card counts (can be changed later) ---
const uint8_t NUM_DI = 4;
const uint8_t NUM_DO = 4;
const uint8_t NUM_AI = 2;
const uint8_t NUM_SIO = 4;

const uint8_t totalCards = NUM_DI + NUM_DO + NUM_AI + NUM_SIO;

const uint8_t DI_Pins[NUM_DI] = {13, 12, 14, 27};  // Digital Input pins
const uint8_t DO_Pins[NUM_DO] = {26, 25, 33, 32};  // Digital Output pins
const uint8_t AI_Pins[NUM_AI] = {35, 34};          // Analog Input pins
const uint32_t TICK_MS = 10;

// 1. Define the Master Lists
// Canonical card families sharing one schema and serialization model.
// DI/AI behavior remains backward-compatible with prior configuration files.
// DO/SIO behavior is documented as the next implementation contract.
#define LIST_CARD_TYPES(X) \
  X(DigitalInput)          \
  X(DigitalOutput)         \
  X(AnalogInput)           \
  X(SoftIO)

// Operators for evaluating another card inside set/reset logic groups.
// Numeric operators compare against `currentValue`.
// State operators evaluate process lifecycle visibility across cards.
#define LIST_OPERATORS(X) \
  X(Op_None)              \
  X(Op_AlwaysTrue)        \
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

// Unified mode list kept schema-compatible across card families.
// For DO/SIO cards, mode is the polarity interface selector:
// - DO_ActiveHigh: logical ON maps to physical HIGH.
// - DO_ActiveLow : logical ON maps to physical LOW.
// DI/AI legacy mode values stay available for persisted projects.
#define LIST_MODES(X) \
  X(Mode_None)        \
  X(DI_Rising)        \
  X(DI_Falling)       \
  X(DI_Change)        \
  X(DI_Immediate)     \
  X(DI_Debounced)     \
  X(DO_ActiveHigh)    \
  X(DO_ActiveLow)     \
  X(Out_NoDelay)      \
  X(Out_OnDelay)      \
  X(Out_AutoOff)      \
  X(AI_Immediate)     \
  X(AI_Smoothed)      \
  X(OneShot)          \
  X(Repeating)        \
  X(Count_Up)         \
  X(Count_Down)

// Unified runtime state list kept schema-compatible across card families.
// DO/SIO canonical phase contract:
// Other values remain for existing DI/AI and legacy process flows.
#define LIST_STATES(X)   \
  X(State_None)          \
  X(State_Off)           \
  X(State_Debouncing)    \
  X(State_OnDelay)       \
  X(State_AutoOff)       \
  X(State_On)            \
  X(State_Ready)         \
  X(State_Running)       \
  X(State_Finished)      \
  X(State_Stopped)       \
  X(State_Idle)          \
  X(State_Counting)      \
  X(State_TargetReached) \
  X(State_DO_Idle)       \
  X(State_DO_OnDelay)    \
  X(State_DO_Active)     \
  X(State_DO_OffDelay)

#define LIST_COMBINE(X) \
  X(Combine_None)       \
  X(Combine_AND)        \
  X(Combine_OR)

#define as_enum(name) name,
enum logicCardType { LIST_CARD_TYPES(as_enum) };
enum logicOperator { LIST_OPERATORS(as_enum) };
enum cardMode { LIST_MODES(as_enum) };
enum cardState { LIST_STATES(as_enum) };
enum logicCombine { LIST_COMBINE(as_enum) };
#undef as_enum

// ----------------------------------------------------------------------------
// DO/SIO semantic aliases (non-breaking readability layer).
// These aliases do not change enum values or serialization schema.
// ----------------------------------------------------------------------------
constexpr cardMode Mode_DO_PolarityActiveHigh = DO_ActiveHigh;
constexpr cardMode Mode_DO_PolarityActiveLow = DO_ActiveLow;

constexpr cardState Phase_DO_Idle = State_DO_Idle;
constexpr cardState Phase_DO_OnDelay = State_DO_OnDelay;
constexpr cardState Phase_DO_Active = State_DO_Active;
constexpr cardState Phase_DO_OffDelay = State_DO_OffDelay;

// ----------------------------------------------------------------------------
// DO/SIO quick map (documentation only):
// - Trigger source     : setCondition rising edge -> triggerFlag (one cycle).
// - Latch signal       : logicalState (latched until reset/mission completion).
// - Effective output   : physicalState (timed truth used by dependent cards).
// - Counter behavior   : currentValue increments on physicalState 0->1
// transitions.
// - Phase progression  : Idle(0) -> OnDelay(1) -> Active(2) -> OffDelay(3) ->
// next/idle.
// ----------------------------------------------------------------------------

struct LogicCard {
  // ----------------------------------------------------------------------------
  // Identity and binding fields.
  // These fields let every card participate in one global reference graph.
  // ----------------------------------------------------------------------------

  // Global unique card ID.
  // Used by set/reset references, lookups, and JSON persistence keys.
  // Stable IDs allow cards to reference each other across reboots.
  uint8_t id;

  // Card family discriminator.
  // DI and AI represent physical inputs, DO represents hardware-driven outputs,
  // and SIO represents virtual output-like logic signals.
  logicCardType type;

  // Zero-based index inside its own family.
  // Example: DO0..DOn for outputs, DI0..DIn for digital inputs.
  // Used for human-readable mapping and per-family pin tables.
  uint8_t index;

  // Bound hardware pin for physical cards.
  // DI/DO/AI: actual MCU pin/channel mapping used by hardware layer.
  // SIO: no hardware mapping; value is ignored (conventionally 255).
  uint8_t hwPin;

  // ----------------------------------------------------------------------------
  // Persistent configuration constants.
  // ----------------------------------------------------------------------------

  // DI role:
  // - Input polarity selector in legacy DI flow
  //   (0 = active high, 1 = active low).
  // DO/SIO role:
  // - Mission repeat limit controller.
  // - 1  => one-shot cycle.
  // - >1 => execute exactly N cycles.
  // - <=0 => repeat infinitely (oscillator/blinker behavior).
  float constant1;

  // DI role:
  // - Reserved / currently unused in active DI behavior.
  // DO/SIO role:
  // - Reserved for future extensions (e.g., waveform/PWM metadata).
  // - Kept in schema for forward compatibility.
  float constant2;

  // ----------------------------------------------------------------------------
  // Runtime signal image (observable by other cards).
  // ----------------------------------------------------------------------------

  // DI role:
  // - Debounced and qualified DI logical result.
  // DO/SIO role:
  // - Process latch / intent state.
  // - Set true by trigger event and remains latched until reset or completion.
  bool logicalState;

  // DI role:
  // - Physical input state after DI polarity/qualification path.
  // DO/SIO role:
  // - Time-shaped output state produced by timing phases.
  // - Main signal dependents should consume for effective output truth.
  bool physicalState;

  // DI role:
  // - One-cycle edge pulse from configured DI edge mode.
  // DO/SIO role:
  // - One-cycle ignition pulse from setCondition rising edge.
  // - Non-retriggerable contract: ignored while process is already active.
  bool triggerFlag;

  // DI role:
  // - Legacy behavior switch retained for schema compatibility.
  // DO/SIO role:
  // - Intentionally ignored by contract (non-retriggerable model).
  // - Kept to avoid schema migration breaks with prior saved data.
  bool allowRetrigger;

  // ----------------------------------------------------------------------------
  // Timing parameters (milliseconds).
  // ----------------------------------------------------------------------------

  // DI role:
  // - Debounce time window.
  // DO/SIO role:
  // - ON timing parameter (on-delay / pulse-high duration).
  // - 0 can be used to collapse/skip ON hold behavior depending on mode.
  uint32_t setting1;

  // DI role:
  // - Reserved / currently unused in active DI behavior.
  // DO/SIO role:
  // - OFF timing parameter (off-delay / pulse-low rest interval).
  // - Governs pause between cycles for repeating missions.
  uint32_t setting2;

  // ----------------------------------------------------------------------------
  // Runtime counters and timer registers.
  // ----------------------------------------------------------------------------

  // DI role:
  // - Qualified DI event counter.
  // DO/SIO role:
  // - Mission odometer (cycle counter).
  // - Increments on each physicalState rising edge (0 -> 1).
  // - Persists until resetCondition clears it.
  uint32_t currentValue;

  // DI role:
  // - Timestamp for ON debounce candidate window start.
  // DO/SIO role:
  // - Internal register storing ON-phase start time.
  // - Compared against millis() to advance phase timing.
  uint32_t startOnMs;

  // DI role:
  // - Timestamp for OFF debounce candidate window start.
  // DO/SIO role:
  // - Internal register storing OFF-phase start time.
  // - Compared against millis() to advance phase timing.
  uint32_t startOffMs;

  // ----------------------------------------------------------------------------
  // Mode and phase visibility.
  // ----------------------------------------------------------------------------

  // DI role:
  // - DI edge behavior selector (Rising/Falling/Change/Immediate/Debounced).
  // DO/SIO role:
  // - Hardware polarity adapter for physical mapping semantics.
  // - DO_ActiveHigh or DO_ActiveLow while logic remains positive internally.
  cardMode mode;

  // DI role:
  // - Current runtime phase in DI filtering/debounce flow.
  // DO/SIO role:
  // - Public timing phase indicator:
  //   Idle(0), OnDelay(1), Active(2), OffDelay(3).
  // - Exposed so other cards can depend on process phase explicitly.
  cardState state;

  // ----------------------------------------------------------------------------
  // SET condition group (start gatekeeper).
  // Evaluated for rising-edge trigger generation.
  // ----------------------------------------------------------------------------

  // DI role:
  // - Source card ID for DI start qualification graph.
  // DO/SIO role:
  // - Source card ID for clause-A of setCondition (trigger gate).
  // - Points to card evaluated by `setA_Operator`.
  int setA_ID;

  // DI role:
  // - Operator selecting which DI source signal/property is tested.
  // DO/SIO role:
  // - Clause-A evaluation operator for setCondition.
  // - Evaluates source logical/physical/trigger/state/value semantics.
  logicOperator setA_Operator;

  // DI role:
  // - Numeric threshold used when DI clause-A operator is value-based.
  // DO/SIO role:
  // - Numeric threshold for clause-A numeric operators vs source
  // `currentValue`.
  uint32_t setA_Threshold;

  // DI role:
  // - Optional second DI source card ID for composite start conditions.
  // DO/SIO role:
  // - Clause-B source card ID for setCondition.
  // - Used when `setCombine` is `Combine_AND` or `Combine_OR`.
  int setB_ID;

  // DI role:
  // - Optional second DI operator for composite start conditions.
  // DO/SIO role:
  // - Clause-B evaluation operator for setCondition.
  // - Same signal semantics as `setA_Operator`.
  logicOperator setB_Operator;

  // DI role:
  // - Threshold paired with DI clause-B numeric operator.
  // DO/SIO role:
  // - Threshold paired with clause-B numeric operator against source value.
  uint32_t setB_Threshold;

  // DI role:
  // - Combines DI clause-A and clause-B evaluations for start decisions.
  // DO/SIO role:
  // - setCondition combiner: `Combine_None` uses clause-A only.
  // - `Combine_AND`/`Combine_OR` includes clause-B in trigger decision.
  logicCombine setCombine;

  // ----------------------------------------------------------------------------
  // RESET condition group (hard stop + clear gatekeeper).
  // Evaluated continuously; true causes immediate process reset.
  // ----------------------------------------------------------------------------

  // DI role:
  // - Source card ID for DI reset/clear qualification graph.
  // DO/SIO role:
  // - Source card ID for clause-A of resetCondition (hard stop gate).
  // - Points to card evaluated by `resetA_Operator`.
  int resetA_ID;

  // DI role:
  // - Operator selecting DI source signal/property for reset behavior.
  // DO/SIO role:
  // - Clause-A evaluation operator for resetCondition.
  // - True result can immediately clear mission runtime state.
  logicOperator resetA_Operator;

  // DI role:
  // - Numeric threshold for DI clause-A value-based reset operators.
  // DO/SIO role:
  // - Numeric threshold for clause-A numeric operators vs source
  // `currentValue`.
  uint32_t resetA_Threshold;

  // DI role:
  // - Optional second DI source card ID for composite reset logic.
  // DO/SIO role:
  // - Clause-B source card ID for resetCondition.
  // - Used when `resetCombine` is `Combine_AND` or `Combine_OR`.
  int resetB_ID;

  // DI role:
  // - Optional second DI operator for composite reset logic.
  // DO/SIO role:
  // - Clause-B evaluation operator for resetCondition.
  // - Same signal semantics as `resetA_Operator`.
  logicOperator resetB_Operator;

  // DI role:
  // - Threshold paired with DI clause-B numeric reset operator.
  // DO/SIO role:
  // - Threshold paired with clause-B numeric operator against source value.
  uint32_t resetB_Threshold;

  // DI role:
  // - Combines DI clause-A and clause-B reset evaluations.
  // DO/SIO role:
  // - resetCondition combiner: `Combine_None` uses clause-A only.
  // - True resetCondition provides hard stop and counter clear semantics.
  logicCombine resetCombine;
};
LogicCard logicCards[totalCards];

uint32_t lastTickMs = 0;

// Helper function for enum to string conversion and vice versa (for JSON
// serialization) utilizing the X-Macro lists

#define ENUM_TO_STRING_CASE(name) \
  case name:                      \
    return #name;

#define STRING_TO_ENUM_IF(name) \
  if (strcmp(str, #name) == 0) return name;

const char* cardTypeToStr(logicCardType t) {
  switch (t) { LIST_CARD_TYPES(ENUM_TO_STRING_CASE) }
  return "DigitalInput";
}
logicCardType strToCardType(const char* str) {
  LIST_CARD_TYPES(STRING_TO_ENUM_IF)
  return DigitalInput;
}

const char* modeToStr(cardMode m) {
  switch (m) { LIST_MODES(ENUM_TO_STRING_CASE) }
  return "Mode_None";
}
cardMode strToMode(const char* str) {
  LIST_MODES(STRING_TO_ENUM_IF)
  return Mode_None;
}

const char* opToStr(logicOperator o) {
  switch (o) { LIST_OPERATORS(ENUM_TO_STRING_CASE) }
  return "Op_None";
}
logicOperator strToOp(const char* str) {
  LIST_OPERATORS(STRING_TO_ENUM_IF)
  return Op_None;
}

const char* stateToStr(cardState s) {
  switch (s) { LIST_STATES(ENUM_TO_STRING_CASE) }
  return "State_None";
}
cardState strToState(const char* str) {
  LIST_STATES(STRING_TO_ENUM_IF)
  return State_None;
}

const char* combineToStr(logicCombine c) {
  switch (c) { LIST_COMBINE(ENUM_TO_STRING_CASE) }
  return "Combine_None";
}
logicCombine strToCombine(const char* str) {
  LIST_COMBINE(STRING_TO_ENUM_IF)
  return Combine_None;
}
#undef ENUM_TO_STRING_CASE
#undef STRING_TO_ENUM_IF

// This function should serialize whole cards array at once.
// Need to modify to do that.
void serializeCardToJson(const LogicCard& card, JsonObject& json) {
  json["id"] = card.id;
  json["type"] = cardTypeToStr(card.type);
  json["index"] = card.index;
  json["hwPin"] = card.hwPin;
  json["constant1"] = card.constant1;
  json["constant2"] = card.constant2;
  json["logicalState"] = card.logicalState;
  json["physicalState"] = card.physicalState;
  json["triggerFlag"] = card.triggerFlag;
  json["setting1"] = card.setting1;
  json["setting2"] = card.setting2;
  json["currentValue"] = card.currentValue;
  json["startOnMs"] = card.startOnMs;
  json["startOffMs"] = card.startOffMs;
  json["mode"] = modeToStr(card.mode);
  json["state"] = stateToStr(card.state);
  json["allowRetrigger"] = card.allowRetrigger;

  json["setA_ID"] = card.setA_ID;
  json["setA_Operator"] = opToStr(card.setA_Operator);
  json["setA_Threshold"] = card.setA_Threshold;
  json["setB_ID"] = card.setB_ID;
  json["setB_Operator"] = opToStr(card.setB_Operator);
  json["setB_Threshold"] = card.setB_Threshold;
  json["setCombine"] = combineToStr(card.setCombine);

  json["resetA_ID"] = card.resetA_ID;
  json["resetA_Operator"] = opToStr(card.resetA_Operator);
  json["resetA_Threshold"] = card.resetA_Threshold;
  json["resetB_ID"] = card.resetB_ID;
  json["resetB_Operator"] = opToStr(card.resetB_Operator);
  json["resetB_Threshold"] = card.resetB_Threshold;
  json["resetCombine"] = combineToStr(card.resetCombine);
}

void deserializeCardFromJson(JsonObject& json, LogicCard& card) {
  card.id = json["id"];
  card.type = strToCardType(json["type"]);
  card.index = json["index"];
  card.hwPin = json["hwPin"];
  card.constant1 = json["constant1"];
  card.constant2 = json["constant2"];
  card.logicalState = json["logicalState"];
  card.physicalState = json["physicalState"];
  card.triggerFlag = json["triggerFlag"];
  card.setting1 = json["setting1"];
  card.setting2 = json["setting2"];
  card.currentValue = json["currentValue"];
  if (json["startOnMs"].is<uint32_t>()) {
    card.startOnMs = json["startOnMs"];
  } else {
    card.startOnMs = 0;
  }
  if (json["startOffMs"].is<uint32_t>()) {
    card.startOffMs = json["startOffMs"];
  } else {
    card.startOffMs = 0;
  }
  card.mode = strToMode(json["mode"]);
  card.state = strToState(json["state"]);
  card.allowRetrigger = json["allowRetrigger"];

  card.setA_ID = json["setA_ID"];
  card.setA_Operator = strToOp(json["setA_Operator"]);
  card.setA_Threshold = json["setA_Threshold"];
  card.setB_ID = json["setB_ID"];
  card.setB_Operator = strToOp(json["setB_Operator"]);
  card.setB_Threshold = json["setB_Threshold"];
  card.setCombine = strToCombine(json["setCombine"]);

  card.resetA_ID = json["resetA_ID"];
  card.resetA_Operator = strToOp(json["resetA_Operator"]);
  card.resetA_Threshold = json["resetA_Threshold"];
  card.resetB_ID = json["resetB_ID"];
  card.resetB_Operator = strToOp(json["resetB_Operator"]);
  card.resetB_Threshold = json["resetB_Threshold"];
  card.resetCombine = strToCombine(json["resetCombine"]);
}

void saveConfig() {
  File file = LittleFS.open("/config.json", "w");
  if (!file) return;

  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();

  for (int i = 0; i < totalCards; i++) {
    JsonObject obj = array.add<JsonObject>();
    serializeCardToJson(logicCards[i], obj);
  }

  // Write JSON to file

  serializeJson(doc, file);
  file.close();
}

bool loadConfig() {
  if (!LittleFS.exists("/config.json")) return false;

  File file = LittleFS.open("/config.json", "r");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) return false;

  JsonArray array = doc.as<JsonArray>();
  for (int i = 0; i < totalCards; i++) {
    JsonObject obj = array[i];
    deserializeCardFromJson(obj, logicCards[i]);
  }
  return true;
}

LogicCard* getCardById(int id) {
  if (id < 0 || id >= totalCards) return nullptr;
  return &logicCards[id];
}

bool evalOperator(const LogicCard& card, logicOperator op, uint32_t threshold) {
  switch (op) {
    case Op_AlwaysTrue:
      return true;
    case Op_LogicalTrue:
      return card.logicalState;
    case Op_LogicalFalse:
      return !card.logicalState;
    case Op_PhysicalOn:
      return card.physicalState;
    case Op_PhysicalOff:
      return !card.physicalState;
    case Op_Triggered:
      return card.triggerFlag;
    case Op_TriggerCleared:
      return !card.triggerFlag;
    case Op_GT:
      return card.currentValue > threshold;
    case Op_LT:
      return card.currentValue < threshold;
    case Op_EQ:
      return card.currentValue == threshold;
    case Op_NEQ:
      return card.currentValue != threshold;
    case Op_GTE:
      return card.currentValue >= threshold;
    case Op_LTE:
      return card.currentValue <= threshold;
    case Op_Running:
      return card.state == State_Running;
    case Op_Finished:
      return card.state == State_Finished;
    case Op_Stopped:
      return card.state == State_Stopped;
    case Op_None:
    default:
      return false;
  }
}

bool evalClause(int id, logicOperator op, uint32_t threshold) {
  if (op == Op_None) return false;
  LogicCard* src = getCardById(id);
  if (!src) return false;
  return evalOperator(*src, op, threshold);
}

bool evalCondition(int aId, logicOperator aOp, uint32_t aThr, int bId,
                   logicOperator bOp, uint32_t bThr, logicCombine combine) {
  bool a = evalClause(aId, aOp, aThr);
  if (combine == Combine_None) return a;
  bool b = evalClause(bId, bOp, bThr);
  if (combine == Combine_AND) return a && b;
  if (combine == Combine_OR) return a || b;
  return a;
}

void initLogicEngine() {
  if (!LittleFS.begin(true)) {
    Serial.println("FS Mount Failed");
    return;
  }

  if (!loadConfig()) {
    Serial.println("Generating Defaults...");

    for (uint8_t i = 0; i < totalCards; i++) {
      logicCards[i].id = i;
      logicCards[i].constant1 = 1;
      logicCards[i].constant2 = 0;
      logicCards[i].logicalState = false;
      logicCards[i].physicalState = false;
      logicCards[i].triggerFlag = false;
      logicCards[i].allowRetrigger = false;
      logicCards[i].setting1 = 0;
      logicCards[i].setting2 = 0;
      logicCards[i].currentValue = 0;
      logicCards[i].startOnMs = 0;
      logicCards[i].startOffMs = 0;
      logicCards[i].mode = Mode_None;
      logicCards[i].state = State_None;

      // --- Self-Reference Logic ---
      logicCards[i].setA_ID = i;
      logicCards[i].setB_ID = i;
      logicCards[i].resetA_ID = i;
      logicCards[i].resetB_ID = i;

      logicCards[i].setCombine = Combine_None;
      logicCards[i].resetCombine = Combine_None;
      logicCards[i].setA_Operator = Op_None;
      logicCards[i].setB_Operator = Op_None;
      logicCards[i].resetA_Operator = Op_None;
      logicCards[i].resetB_Operator = Op_None;
      logicCards[i].setA_Threshold = 0;
      logicCards[i].setB_Threshold = 0;
      logicCards[i].resetA_Threshold = 0;
      logicCards[i].resetB_Threshold = 0;

      // --- Hardware Pin Assignment ---
      if (i < NUM_DI) {
        logicCards[i].type = DigitalInput;
        logicCards[i].index = i;
        logicCards[i].hwPin = DI_Pins[i];
      } else if (i < (NUM_DI + NUM_DO)) {
        logicCards[i].type = DigitalOutput;
        logicCards[i].index = i - NUM_DI;
        logicCards[i].hwPin = DO_Pins[logicCards[i].index];
        logicCards[i].mode = DO_ActiveHigh;
        logicCards[i].state = State_DO_Idle;
      } else if (i < (NUM_DI + NUM_DO + NUM_AI)) {
        logicCards[i].type = AnalogInput;
        logicCards[i].index = i - (NUM_DI + NUM_DO);
        logicCards[i].hwPin = AI_Pins[logicCards[i].index];
      } else {
        // SoftIO (virtual outputs)
        logicCards[i].hwPin = 255;
        logicCards[i].type = SoftIO;
        logicCards[i].index = i - (NUM_DI + NUM_DO + NUM_AI);
        logicCards[i].mode = DO_ActiveHigh;
        logicCards[i].state = State_DO_Idle;
      }
    }
    saveConfig();
  }

  // --- Physical Pin Binding ---
  for (int i = 0; i < totalCards; i++) {
    if (logicCards[i].hwPin != 255) {
      if (logicCards[i].type == DigitalOutput) {
        pinMode(logicCards[i].hwPin, OUTPUT);
        digitalWrite(logicCards[i].hwPin, LOW);
      } else {
        pinMode(logicCards[i].hwPin, INPUT);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- Advanced Timer Starting ---");
  delay(1000);  // Allow time for Serial to initialize
  initLogicEngine();
  lastTickMs = millis();

  // Debug: Print loaded configuration
  for (int i = 0; i < totalCards; i++) {
    Serial.print("Card ID: ");
    Serial.print(logicCards[i].id);
    Serial.print(", Type: ");
    Serial.print(cardTypeToStr(logicCards[i].type));
    Serial.print(", Index: ");
    Serial.print(logicCards[i].index);
    Serial.print(", HW Pin: ");
    Serial.print(logicCards[i].hwPin);
    Serial.print(", constant1: ");
    Serial.print(logicCards[i].constant1);
    Serial.print(", constant2: ");
    Serial.print(logicCards[i].constant2);
    Serial.print(", logicalState: ");
    Serial.print(logicCards[i].logicalState);
    Serial.print(", physicalState: ");
    Serial.print(logicCards[i].physicalState);
    Serial.print(", triggerFlag: ");
    Serial.print(logicCards[i].triggerFlag);
    Serial.print(", allowRetrigger: ");
    Serial.print(logicCards[i].allowRetrigger);
    Serial.print(", setting1: ");
    Serial.print(logicCards[i].setting1);
    Serial.print(", setting2: ");
    Serial.print(logicCards[i].setting2);
    Serial.print(", currentValue: ");
    Serial.print(logicCards[i].currentValue);
    Serial.print(", startOnMs: ");
    Serial.print(logicCards[i].startOnMs);
    Serial.print(", startOffMs: ");
    Serial.print(logicCards[i].startOffMs);
    Serial.print(", mode: ");
    Serial.print(modeToStr(logicCards[i].mode));
    Serial.print(", state: ");
    Serial.print(stateToStr(logicCards[i].state));
    Serial.print(", setA_ID: ");
    Serial.print(logicCards[i].setA_ID);
    Serial.print(", setA_Operator: ");
    Serial.print(opToStr(logicCards[i].setA_Operator));
    Serial.print(", setA_Threshold: ");
    Serial.print(logicCards[i].setA_Threshold);
    Serial.print(", setB_ID: ");
    Serial.print(logicCards[i].setB_ID);
    Serial.print(", setB_Operator: ");
    Serial.print(opToStr(logicCards[i].setB_Operator));
    Serial.print(", setB_Threshold: ");
    Serial.print(logicCards[i].setB_Threshold);
    Serial.print(", setCombine: ");
    Serial.print(combineToStr(logicCards[i].setCombine));
    Serial.print(", resetA_ID: ");
    Serial.print(logicCards[i].resetA_ID);
    Serial.print(", resetA_Operator: ");
    Serial.print(opToStr(logicCards[i].resetA_Operator));
    Serial.print(", resetA_Threshold: ");
    Serial.print(logicCards[i].resetA_Threshold);
    Serial.print(", resetB_ID: ");
    Serial.print(logicCards[i].resetB_ID);
    Serial.print(", resetB_Operator: ");
    Serial.print(opToStr(logicCards[i].resetB_Operator));
    Serial.print(", resetB_Threshold: ");
    Serial.print(logicCards[i].resetB_Threshold);
    Serial.print(", resetCombine: ");
    Serial.println(combineToStr(logicCards[i].resetCombine));
    Serial.println("---------------------------------------------");
  }
}

void loop() {}
