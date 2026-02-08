/**********************************************************************************************
 * LOGICCARD ENGINE — DESIGN PHILOSOPHY AND PRODUCT VISION
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
 * 3) CORE IDEA OF THE LOGICCARD ENGINE
 * ============================================================================================
 *
 * The LogicCard engine introduces a different paradigm:
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
 * The LogicCard engine provides:
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
 * By unifying all functional elements (IO, timers, counters, virtual signals)
 * under the same LogicCard model, the system becomes:
 *
 * - Fully configurable through data (JSON).
 * - Observable and debuggable in real time.
 * - Extensible without rewriting core logic.
 *
 * ============================================================================================
 * 7) LONG-TERM VISION
 * ============================================================================================
 *
 * The LogicCard engine is designed as a foundational automation kernel that
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
 * (Digital IO, Analog IO, Timers, Counters, and Virtual IO) is represented as a
 * unified entity called a "LogicCard".
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
 * - Timer         : Time-based logic units.
 * - Counter       : Event-counting logic units.
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
 *    - logicalState, physicalState, triggerFlag, currentValue, state.
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
 * - Timer         : Time-based state machines.
 * - Counter       : Event-based counting units.
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
 * Examples:
 * - Mode_None       : No special behavior.
 * - DI_Immediate    : Digital input reacts instantly.
 * - DI_Debounced    : Digital input uses debounce logic.
 * - Out_NoDelay     : Output changes immediately.
 * - Out_OnDelay     : Output activates after delay.
 * - Out_AutoOff     : Output turns off automatically after time.
 * - AI_Immediate    : Analog input without filtering.
 * - AI_Smoothed     : Analog input with smoothing.
 * - OneShot         : Timer Single pulse behavior.
 * - Repeating       : Timer auto-repeating behavior.
 * - Count_Up        : Counter increments.
 * - Count_Down      : Counter decrements.
 *
 * Mode defines "how the card behaves".
 *
 * --------------------------------------------------------------------------------------------
 *
 * 2.3 cardState
 * -------------
 * Defines the current internal lifecycle state of a card.
 *
 * Examples:
 * - State_None          : Undefined / not active.
 * - State_Off           : Inactive state.
 * - State_OnDelay       : Waiting for delay completion.
 * - State_AutoOff       : Auto-off countdown running.
 * - State_On            : Active state.
 * - State_Ready         : Ready to start.
 * - State_Running       : Timer/Counter active.
 * - State_Finished      : Timer/Counter completed.
 * - State_Stopped       : Timer/Counter stopped.
 * - State_Idle          : Waiting state.
 * - State_Counting      : Counter active.
 * - State_TargetReached : Counter reached target.
 *
 * State defines "where the card currently is".
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
 * Process state operators (Timer/Counter):
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
 * constant1 : Calibration or scaling parameter.
 * constant2 : Secondary calibration or offset parameter.
 *
 * Core Logic Signals:
 * -------------------
 * logicalState  : Logical meaning of the card (true/false).
 * physicalState : Actual hardware or virtual output state.
 * triggerFlag   : One-cycle event flag for edge detection.
 *
 * Behavior Control:
 * -----------------
 * allowRetrigger : Determines whether repeated triggers are allowed.
 *
 * Generic Settings:
 * -----------------
 * setting1 : Primary configuration value.
 *            Examples: delay time, threshold, target count.
 *
 * setting2 : Secondary configuration value.
 *            Examples: auto-off time, debounce cycles, smoothing window.
 *
 * Runtime Value:
 * --------------
 * currentValue : Dynamic numeric value.
 *                Examples: elapsed time, analog reading, counter value.
 *
 * Mode & State:
 * -------------
 * mode  : Defines behavior pattern of the card.
 * state : Defines current lifecycle state.
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
 * - logicalState reflects interpreted input.
 * - physicalState reflects raw hardware state.
 * - triggerFlag indicates edge transitions.
 *
 * DigitalOutput:
 * - logicalState represents desired output state.
 * - physicalState represents actual output pin state.
 *
 * AnalogInput:
 * - currentValue holds analog reading.
 * - logicalState may represent threshold-based logic.
 *
 * SoftIO:
 * - Behaves like DigitalOutput without hardware.
 * - Used for internal logic routing.
 *
 * Timer:
 * - currentValue represents elapsed time.
 * - state represents timer lifecycle.
 *
 * Counter:
 * - currentValue represents count value.
 * - state represents counting lifecycle.
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
const uint8_t NUM_TIMER = 2;
const uint8_t NUM_COUNTER = 2;

const uint8_t totalCards =
    NUM_DI + NUM_DO + NUM_AI + NUM_SIO + NUM_TIMER + NUM_COUNTER;

const uint8_t DI_Pins[NUM_DI] = {13, 12, 14, 27};  // Digital Input pins
const uint8_t DO_Pins[NUM_DO] = {26, 25, 33, 32};  // Digital Output pins
const uint8_t AI_Pins[NUM_AI] = {35, 34};          // Analog Input pins

// 1. Define the Master Lists
#define LIST_CARD_TYPES(X) \
  X(DigitalInput)          \
  X(DigitalOutput)         \
  X(AnalogInput)           \
  X(SoftIO)                \
  X(Timer)                 \
  X(Counter)

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

#define LIST_MODES(X) \
  X(Mode_None)        \
  X(DI_Immediate)     \
  X(DI_Debounced)     \
  X(Out_NoDelay)      \
  X(Out_OnDelay)      \
  X(Out_AutoOff)      \
  X(AI_Immediate)     \
  X(AI_Smoothed)      \
  X(OneShot)          \
  X(Repeating)        \
  X(Count_Up)         \
  X(Count_Down)

#define LIST_STATES(X) \
  X(State_None)        \
  X(State_Off)         \
  X(State_OnDelay)     \
  X(State_AutoOff)     \
  X(State_On)          \
  X(State_Ready)       \
  X(State_Running)     \
  X(State_Finished)    \
  X(State_Stopped)     \
  X(State_Idle)        \
  X(State_Counting)    \
  X(State_TargetReached)

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

struct LogicCard {
  uint8_t id;
  logicCardType type;
  uint8_t index;
  uint8_t hwPin;
  float constant1;
  float constant2;
  bool logicalState;
  bool physicalState;
  bool triggerFlag;
  bool allowRetrigger;
  uint32_t setting1;
  uint32_t setting2;
  uint32_t currentValue;
  cardMode mode;
  cardState state;

  // LogicCondition setCondition;
  int setA_ID;
  logicOperator setA_Operator;
  uint32_t setA_Threshold;
  int setB_ID;
  logicOperator setB_Operator;
  uint32_t setB_Threshold;
  logicCombine setCombine;

  // LogicCondition resetCondition;
  int resetA_ID;
  logicOperator resetA_Operator;
  uint32_t resetA_Threshold;
  int resetB_ID;
  logicOperator resetB_Operator;
  uint32_t resetB_Threshold;
  logicCombine resetCombine;
};
LogicCard logicCards[totalCards];

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

void initLogicEngine() {
  if (!LittleFS.begin(true)) {
    Serial.println("FS Mount Failed");
    return;
  }

  if (!loadConfig()) {
    Serial.println("Generating Defaults...");

    for (uint8_t i = 0; i < totalCards; i++) {
      logicCards[i].id = i;

      // --- Self-Reference Logic ---
      logicCards[i].setA_ID = i;
      logicCards[i].setB_ID = i;
      logicCards[i].resetA_ID = i;
      logicCards[i].resetB_ID = i;

      logicCards[i].setCombine = Combine_None;
      logicCards[i].resetCombine = Combine_None;
      logicCards[i].setA_Operator = Op_None;
      logicCards[i].resetA_Operator = Op_None;

      // --- Hardware Pin Assignment ---
      if (i < NUM_DI) {
        logicCards[i].type = DigitalInput;
        logicCards[i].index = i;
        logicCards[i].hwPin = DI_Pins[i];
      } else if (i < (NUM_DI + NUM_DO)) {
        logicCards[i].type = DigitalOutput;
        logicCards[i].index = i - NUM_DI;
        logicCards[i].hwPin = DO_Pins[logicCards[i].index];
      } else if (i < (NUM_DI + NUM_DO + NUM_AI)) {
        logicCards[i].type = AnalogInput;
        logicCards[i].index = i - (NUM_DI + NUM_DO);
        logicCards[i].hwPin = AI_Pins[logicCards[i].index];
      } else {
        // SoftIO, Timers, Counters
        logicCards[i].hwPin = 255;
        if (i < NUM_DI + NUM_DO + NUM_AI + NUM_SIO) {
          logicCards[i].type = SoftIO;
          logicCards[i].index = i - (NUM_DI + NUM_DO + NUM_AI);
        } else if (i < NUM_DI + NUM_DO + NUM_AI + NUM_SIO + NUM_TIMER) {
          logicCards[i].type = Timer;
          logicCards[i].index = i - (NUM_DI + NUM_DO + NUM_AI + NUM_SIO);
        } else {
          logicCards[i].type = Counter;
          logicCards[i].index =
              i - (NUM_DI + NUM_DO + NUM_AI + NUM_SIO + NUM_TIMER);
        }
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
  Serial.println("\n--- LogicCard Engine Starting ---");
  delay(1000);  // Allow time for Serial to initialize
  initLogicEngine();

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
