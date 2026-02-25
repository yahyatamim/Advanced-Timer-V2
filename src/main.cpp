/**********************************************************************************************
 *
 * ADVANCED TIMER FIRMWARE NOTES
 *
 * Canonical behavioral contract lives in README.md.
 * - Primary reference: README.md Section 19 (Kernel Architecture Contract)
 * - Integration/runtime contracts: README.md Sections 3 through 18
 *
 * This file should keep only implementation-local comments.
 * Do not duplicate long-form architecture contracts here.
 **********************************************************************************************/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

#include <cctype>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

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
const char* kStagedConfigPath = "/config_staged.json";
const char* kLkgConfigPath = "/config_lkg.json";
const char* kSlot1ConfigPath = "/config_slot1.json";
const char* kSlot2ConfigPath = "/config_slot2.json";
const char* kSlot3ConfigPath = "/config_slot3.json";
const char* kFactoryConfigPath = "/config_factory.json";
const char* kPortalSettingsPath = "/portal_settings.json";
const uint32_t kDefaultScanIntervalMs = 500;
const uint32_t kMinScanIntervalMs = 10;
const uint32_t kMaxScanIntervalMs = 1000;
const char* kMasterSsid = "advancedtimer";
const char* kMasterPassword = "12345678";
const char* kDefaultUserSsid = "FactoryNext";
const char* kDefaultUserPassword = "FactoryNext20$22#";
const uint32_t MASTER_WIFI_TIMEOUT_MS = 2000;
const uint32_t USER_WIFI_TIMEOUT_MS = 180000;

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
enum runMode { RUN_NORMAL, RUN_STEP, RUN_BREAKPOINT, RUN_SLOW };
enum inputSourceMode {
  InputSource_Real,
  InputSource_ForcedHigh,
  InputSource_ForcedLow,
  InputSource_ForcedValue
};
#undef as_enum

#define ENUM_TO_STRING_CASE(name) \
  case name:                      \
    return #name;

bool enumTokenEquals(const char* s, const char* token) {
  if (s == nullptr || token == nullptr) return false;
  // Keep only enum token characters to tolerate hidden bytes (BOM/ZWSP/etc.).
  char cleaned[64];
  size_t j = 0;
  for (size_t i = 0; s[i] != '\0' && j < (sizeof(cleaned) - 1); ++i) {
    const unsigned char ch = static_cast<unsigned char>(s[i]);
    if (std::isalnum(ch) || ch == '_') {
      cleaned[j++] = static_cast<char>(ch);
    }
  }
  cleaned[j] = '\0';

  return strcmp(cleaned, token) == 0;
}

#define ENUM_TRY_PARSE_IF(name) \
  if (enumTokenEquals(s, #name)) {  \
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

const char* toString(runMode value) {
  switch (value) {
    case RUN_NORMAL:
      return "RUN_NORMAL";
    case RUN_STEP:
      return "RUN_STEP";
    case RUN_BREAKPOINT:
      return "RUN_BREAKPOINT";
    case RUN_SLOW:
      return "RUN_SLOW";
    default:
      return "RUN_NORMAL";
  }
}

const char* toString(inputSourceMode value) {
  switch (value) {
    case InputSource_Real:
      return "REAL";
    case InputSource_ForcedHigh:
      return "FORCED_HIGH";
    case InputSource_ForcedLow:
      return "FORCED_LOW";
    case InputSource_ForcedValue:
      return "FORCED_VALUE";
    default:
      return "REAL";
  }
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
  // DO/SIO: delay before output turns ON
  // For DO/SIO 0 means stay in delay phase until reset
  // AI: input minimum (raw ADC/sensor lower bound)
  uint32_t setting1;
  // DI: reserved for future use
  // DO/SIO: ON duration (time to keep output ON before turning OFF)
  // For DO/SIO 0 means stay ON until reset
  // AI: input maximum (raw ADC/sensor upper bound)
  uint32_t setting2;
  // DI: reserved for future use
  // DO/SIO: repeat count (0 = infinite, 1 = single pulse, N = N full cycles)
  // AI: EMA alpha in range 0.0..1.0 (stored internally as 0..1000)
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
  // DO/SIO: timestamp when current delay-before-ON phase started
  // AI: output minimum (scaled physical lower bound)
  uint32_t startOnMs;
  // DI: timestamp of last qualified edge for debounce timing
  // DO/SIO: timestamp when current ON-duration phase started
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
  // OnDelay = delay-before-ON, Active = output ON duration
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

struct SharedRuntimeSnapshot {
  uint32_t seq;
  uint32_t tsMs;
  uint32_t lastCompleteScanUs;
  runMode mode;
  bool testModeActive;
  bool globalOutputMask;
  bool breakpointPaused;
  uint16_t scanCursor;
  LogicCard cards[TOTAL_CARDS];
  inputSourceMode inputSource[TOTAL_CARDS];
  uint32_t forcedAIValue[TOTAL_CARDS];
  bool outputMaskLocal[TOTAL_CARDS];
  bool breakpointEnabled[TOTAL_CARDS];
};

QueueHandle_t gKernelCommandQueue = nullptr;
TaskHandle_t gCore0TaskHandle = nullptr;
TaskHandle_t gCore1TaskHandle = nullptr;
portMUX_TYPE gSnapshotMux = portMUX_INITIALIZER_UNLOCKED;
SharedRuntimeSnapshot gSharedSnapshot = {};
WebServer gPortalServer(80);
WebSocketsServer gWsServer(81);
char gUserSsid[33] = {};
char gUserPassword[65] = {};
bool gPortalReconnectRequested = false;
bool gPortalServerInitialized = false;
bool gWsServerInitialized = false;
volatile bool gKernelPauseRequested = false;
volatile bool gKernelPaused = false;
uint32_t gConfigVersionCounter = 1;
char gActiveVersion[16] = "v1";
char gLkgVersion[16] = "";
char gSlot1Version[16] = "";
char gSlot2Version[16] = "";
char gSlot3Version[16] = "";

runMode gRunMode = RUN_NORMAL;
uint16_t gScanCursor = 0;
bool gStepRequested = false;
bool gBreakpointPaused = false;
bool gTestModeActive = false;
bool gGlobalOutputMask = false;

bool gCardBreakpoint[TOTAL_CARDS] = {};
bool gCardOutputMask[TOTAL_CARDS] = {};
inputSourceMode gCardInputSource[TOTAL_CARDS] = {};
uint32_t gCardForcedAIValue[TOTAL_CARDS] = {};
uint32_t gScanIntervalMs = kDefaultScanIntervalMs;
uint32_t gLastCompleteScanUs = 0;

const uint32_t SLOW_SCAN_INTERVAL_MS = 250;

bool isOutputMasked(uint8_t cardId);
uint8_t scanOrderCardIdFromCursor(uint16_t cursor);
bool connectWiFiWithPolicy();
void initPortalServer();
void handlePortalServerLoop();
void initWebSocketServer();
void handleWebSocketLoop();
void publishRuntimeSnapshotWebSocket();
bool applyCommand(JsonObjectConst command);
void updateSharedRuntimeSnapshot(uint32_t nowMs, bool incrementSeq);
void handleHttpSettingsPage();
void handleHttpConfigPage();
void handleHttpGetSettings();
void handleHttpSaveSettingsWiFi();
void handleHttpSaveSettingsRuntime();
void handleHttpReconnectWiFi();
void handleHttpReboot();
void handleHttpGetActiveConfig();
void handleHttpStagedSaveConfig();
void handleHttpStagedValidateConfig();
void handleHttpCommitConfig();
void handleHttpRestoreConfig();
void serializeCardsToArray(const LogicCard* sourceCards, JsonArray& array);
void initializeCardArraySafeDefaults(LogicCard* cards);
bool deserializeCardsFromArray(JsonArrayConst array, LogicCard* outCards);
bool validateConfigCardsArray(JsonArrayConst array, String& reason);
bool writeJsonToPath(const char* path, JsonDocument& doc);
bool readJsonFromPath(const char* path, JsonDocument& doc);
bool saveCardsToPath(const char* path, const LogicCard* sourceCards);
bool loadCardsFromPath(const char* path, LogicCard* outCards);
bool copyFileIfExists(const char* srcPath, const char* dstPath);
void formatVersion(char* out, size_t outSize, uint32_t version);
bool pauseKernelForConfigApply(uint32_t timeoutMs);
void resumeKernelAfterConfigApply();
void rotateHistoryVersions();
bool applyCardsAsActiveConfig(const LogicCard* newCards);
bool extractConfigCardsFromRequest(JsonObjectConst root, JsonArrayConst& outCards,
                                   String& reason);
void writeConfigErrorResponse(int statusCode, const char* code,
                              const String& message);
bool loadPortalSettingsFromLittleFS();
bool savePortalSettingsToLittleFS();

enum kernelCommandType {
  KernelCmd_SetRunMode,
  KernelCmd_StepOnce,
  KernelCmd_SetBreakpoint,
  KernelCmd_SetTestMode,
  KernelCmd_SetInputForce,
  KernelCmd_SetOutputMask,
  KernelCmd_SetOutputMaskGlobal
};

struct KernelCommand {
  kernelCommandType type;
  uint8_t cardId;
  bool flag;
  uint32_t value;
  runMode mode;
  inputSourceMode inputMode;
};

void serializeCardToJson(const LogicCard& card, JsonObject& json) {
  json["id"] = card.id;
  json["type"] = toString(card.type);
  json["index"] = card.index;
  json["hwPin"] = card.hwPin;
  json["invert"] = card.invert;

  json["setting1"] = card.setting1;
  json["setting2"] = card.setting2;
  if (card.type == AnalogInput) {
    json["setting3"] = static_cast<double>(card.setting3) / 1000.0;
  } else {
    json["setting3"] = card.setting3;
  }

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

void deserializeCardFromJson(JsonVariantConst jsonVariant, LogicCard& card) {
  if (!jsonVariant.is<JsonObjectConst>()) return;
  JsonObjectConst json = jsonVariant.as<JsonObjectConst>();
  LogicCard before = card;

  card.id = json["id"] | card.id;
  const char* rawType = json["type"].as<const char*>();
  logicCardType parsedType = before.type;
  bool typeOk = tryParseLogicCardType(rawType, parsedType);
  card.type = typeOk ? parsedType : before.type;
  card.index = json["index"] | card.index;
  card.hwPin = json["hwPin"] | card.hwPin;
  card.invert = json["invert"] | card.invert;

  card.setting1 = json["setting1"] | card.setting1;
  card.setting2 = json["setting2"] | card.setting2;
  if (card.type == AnalogInput) {
    const double currentAlpha = static_cast<double>(card.setting3) / 1000.0;
    const double parsed = json["setting3"] | currentAlpha;
    if (parsed >= 0.0 && parsed <= 1.0) {
      const double scaled = parsed * 1000.0;
      card.setting3 = static_cast<uint32_t>(scaled + 0.5);
    } else {
      // Backward compatibility: accept legacy milliunit payloads.
      int32_t legacy = static_cast<int32_t>(parsed);
      if (legacy < 0) legacy = 0;
      if (legacy > 1000) legacy = 1000;
      card.setting3 = static_cast<uint32_t>(legacy);
    }
  } else {
    card.setting3 = json["setting3"] | card.setting3;
  }

  card.logicalState = json["logicalState"] | card.logicalState;
  card.physicalState = json["physicalState"] | card.physicalState;
  card.triggerFlag = json["triggerFlag"] | card.triggerFlag;
  card.currentValue = json["currentValue"] | card.currentValue;
  card.startOnMs = json["startOnMs"] | card.startOnMs;
  card.startOffMs = json["startOffMs"] | card.startOffMs;
  card.repeatCounter = json["repeatCounter"] | card.repeatCounter;

  const char* rawMode = json["mode"].as<const char*>();
  cardMode parsedMode = before.mode;
  bool modeOk = tryParseCardMode(rawMode, parsedMode);
  card.mode = modeOk ? parsedMode : before.mode;

  const char* rawState = json["state"].as<const char*>();
  cardState parsedState = before.state;
  bool stateOk = tryParseCardState(rawState, parsedState);
  card.state = stateOk ? parsedState : before.state;

  card.setA_ID = json["setA_ID"] | card.setA_ID;
  const char* rawSetA = json["setA_Operator"].as<const char*>();
  logicOperator parsedSetA = before.setA_Operator;
  bool setAOk = tryParseLogicOperator(rawSetA, parsedSetA);
  card.setA_Operator = setAOk ? parsedSetA : before.setA_Operator;
  card.setA_Threshold = json["setA_Threshold"] | card.setA_Threshold;
  card.setB_ID = json["setB_ID"] | card.setB_ID;
  const char* rawSetB = json["setB_Operator"].as<const char*>();
  logicOperator parsedSetB = before.setB_Operator;
  bool setBOk = tryParseLogicOperator(rawSetB, parsedSetB);
  card.setB_Operator = setBOk ? parsedSetB : before.setB_Operator;
  card.setB_Threshold = json["setB_Threshold"] | card.setB_Threshold;
  const char* rawSetCombine = json["setCombine"].as<const char*>();
  combineMode parsedSetCombine = before.setCombine;
  bool setCombineOk = tryParseCombineMode(rawSetCombine, parsedSetCombine);
  card.setCombine = setCombineOk ? parsedSetCombine : before.setCombine;

  card.resetA_ID = json["resetA_ID"] | card.resetA_ID;
  const char* rawResetA = json["resetA_Operator"].as<const char*>();
  logicOperator parsedResetA = before.resetA_Operator;
  bool resetAOk = tryParseLogicOperator(rawResetA, parsedResetA);
  card.resetA_Operator = resetAOk ? parsedResetA : before.resetA_Operator;
  card.resetA_Threshold = json["resetA_Threshold"] | card.resetA_Threshold;
  card.resetB_ID = json["resetB_ID"] | card.resetB_ID;
  const char* rawResetB = json["resetB_Operator"].as<const char*>();
  logicOperator parsedResetB = before.resetB_Operator;
  bool resetBOk = tryParseLogicOperator(rawResetB, parsedResetB);
  card.resetB_Operator = resetBOk ? parsedResetB : before.resetB_Operator;
  card.resetB_Threshold = json["resetB_Threshold"] | card.resetB_Threshold;
  const char* rawResetCombine = json["resetCombine"].as<const char*>();
  combineMode parsedResetCombine = before.resetCombine;
  bool resetCombineOk =
      tryParseCombineMode(rawResetCombine, parsedResetCombine);
  card.resetCombine = resetCombineOk ? parsedResetCombine : before.resetCombine;

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

  card.setA_ID = globalId;
  card.setB_ID = globalId;
  card.resetA_ID = globalId;
  card.resetB_ID = globalId;
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
    // DI defaults: debounced edge input behavior.
    card.setting1 = 50;   // debounce window
    card.setting2 = 0;    // reserved
    card.setting3 = 0;    // reserved
    card.mode = Mode_DI_Rising;
    card.state = State_DI_Idle;
    return;
  }

  if (globalId < AI_START) {
    card.type = DigitalOutput;
    card.index = globalId - DO_START;
    card.hwPin = DO_Pins[card.index];
    // DO defaults: safe one-shot profile, but disabled by condition defaults.
    card.setting1 = 1000;  // delay before ON
    card.setting2 = 1000;  // ON duration
    card.setting3 = 1;     // one cycle
    card.mode = Mode_DO_Normal;
    card.state = State_DO_Idle;
    return;
  }

  if (globalId < SIO_START) {
    card.type = AnalogInput;
    card.index = globalId - AI_START;
    card.hwPin = AI_Pins[card.index];
    // AI defaults: raw ADC range with moderate smoothing and 0..100.00 output.
    card.setting1 = 0;      // input minimum
    card.setting2 = 4095;   // input maximum
    card.setting3 = 250;    // EMA alpha = 0.25 (stored as 250/1000)
    card.startOnMs = 0;     // output minimum (centiunits)
    card.startOffMs = 10000;  // output maximum (centiunits)
    card.mode = Mode_AI_Continuous;
    card.state = State_AI_Streaming;
    return;
  }

  card.type = SoftIO;
  card.index = globalId - SIO_START;
  card.hwPin = SIO_Pins[card.index];
  // SoftIO defaults follow DO defaults (virtual output).
  card.setting1 = 1000;
  card.setting2 = 1000;
  card.setting3 = 1;
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
  String reason;
  if (!validateConfigCardsArray(array, reason)) return false;

  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    JsonVariantConst item = array[i];
    if (!item.is<JsonObjectConst>()) return false;
    deserializeCardFromJson(item, logicCards[i]);
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

void copySharedRuntimeSnapshot(SharedRuntimeSnapshot& outSnapshot) {
  portENTER_CRITICAL(&gSnapshotMux);
  outSnapshot = gSharedSnapshot;
  portEXIT_CRITICAL(&gSnapshotMux);
}

void appendRuntimeSnapshotCard(JsonArray& cards,
                               const SharedRuntimeSnapshot& snapshot,
                               uint8_t cardId) {
  const LogicCard& card = snapshot.cards[cardId];
  JsonObject node = cards.add<JsonObject>();
  node["id"] = card.id;
  node["type"] = toString(card.type);
  node["index"] = card.index;
  node["familyOrder"] = cardId;
  node["physicalState"] = card.physicalState;
  node["logicalState"] = card.logicalState;
  node["triggerFlag"] = card.triggerFlag;
  node["state"] = toString(card.state);
  node["mode"] = toString(card.mode);
  node["currentValue"] = card.currentValue;
  node["startOnMs"] = card.startOnMs;
  node["startOffMs"] = card.startOffMs;
  node["repeatCounter"] = card.repeatCounter;

  JsonObject forced = node["maskForced"].to<JsonObject>();
  forced["inputSource"] = toString(snapshot.inputSource[cardId]);
  forced["forcedAIValue"] = snapshot.forcedAIValue[cardId];
  forced["outputMaskLocal"] = snapshot.outputMaskLocal[cardId];
  forced["outputMasked"] =
      (snapshot.globalOutputMask || snapshot.outputMaskLocal[cardId]);
  node["breakpointEnabled"] = snapshot.breakpointEnabled[cardId];
}

void serializeRuntimeSnapshot(JsonDocument& doc, uint32_t nowMs) {
  SharedRuntimeSnapshot snapshot = {};
  copySharedRuntimeSnapshot(snapshot);

  doc["type"] = "runtime_snapshot";
  doc["schemaVersion"] = 1;
  doc["tsMs"] = (snapshot.tsMs == 0) ? nowMs : snapshot.tsMs;
  doc["scanIntervalMs"] = gScanIntervalMs;
  doc["lastCompleteScanMs"] =
      static_cast<double>(snapshot.lastCompleteScanUs) / 1000.0;
  doc["runMode"] = toString(snapshot.mode);
  doc["snapshotSeq"] = snapshot.seq;

  JsonObject testMode = doc["testMode"].to<JsonObject>();
  testMode["active"] = snapshot.testModeActive;
  testMode["outputMaskGlobal"] = snapshot.globalOutputMask;
  testMode["breakpointPaused"] = snapshot.breakpointPaused;
  testMode["scanCursor"] = snapshot.scanCursor;

  JsonArray cards = doc["cards"].to<JsonArray>();
  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    appendRuntimeSnapshotCard(cards, snapshot, scanOrderCardIdFromCursor(i));
  }
}

bool waitForWiFiConnected(uint32_t timeoutMs) {
  uint32_t startMs = millis();
  while ((millis() - startMs) < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) return true;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return false;
}

bool connectWiFiWithPolicy() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  vTaskDelay(pdMS_TO_TICKS(50));

  WiFi.begin(kMasterSsid, kMasterPassword);
  if (waitForWiFiConnected(MASTER_WIFI_TIMEOUT_MS)) {
    Serial.print("WiFi connected via MASTER SSID. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  WiFi.disconnect(true, true);
  vTaskDelay(pdMS_TO_TICKS(50));

  WiFi.begin(gUserSsid, gUserPassword);
  if (waitForWiFiConnected(USER_WIFI_TIMEOUT_MS)) {
    Serial.print("WiFi connected via USER SSID. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi offline mode (master/user connect attempts failed)");
  return false;
}

void handleHttpRoot() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    gPortalServer.send(404, "text/plain",
                       "index.html not found in LittleFS (/data upload needed)");
    return;
  }
  gPortalServer.streamFile(file, "text/html");
  file.close();
}

void handleHttpSettingsPage() {
  File file = LittleFS.open("/settings.html", "r");
  if (!file) {
    gPortalServer.send(
        404, "text/plain",
        "settings.html not found in LittleFS (/data upload needed)");
    return;
  }
  gPortalServer.streamFile(file, "text/html");
  file.close();
}

void handleHttpConfigPage() {
  File file = LittleFS.open("/config.html", "r");
  if (!file) {
    gPortalServer.send(
        404, "text/plain",
        "config.html not found in LittleFS (/data upload needed)");
    return;
  }
  gPortalServer.streamFile(file, "text/html");
  file.close();
}

void handleHttpGetSettings() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["masterSsid"] = kMasterSsid;
  doc["masterEditable"] = false;
  doc["userSsid"] = gUserSsid;
  doc["userPassword"] = gUserPassword;
  doc["scanIntervalMs"] = gScanIntervalMs;
  doc["scanIntervalMinMs"] = kMinScanIntervalMs;
  doc["scanIntervalMaxMs"] = kMaxScanIntervalMs;
  doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifiIp"] = WiFi.localIP().toString();
  doc["firmwareVersion"] = String(__DATE__) + " " + String(__TIME__);

  String body;
  serializeJson(doc, body);
  gPortalServer.send(200, "application/json", body);
}

void handleHttpSaveSettingsWiFi() {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, gPortalServer.arg("plain"));
  if (error || !doc.is<JsonObject>()) {
    gPortalServer.send(400, "application/json",
                       "{\"ok\":false,\"error\":\"INVALID_REQUEST\"}");
    return;
  }
  JsonObjectConst root = doc.as<JsonObjectConst>();
  const char* ssid = root["userSsid"] | "";
  const char* password = root["userPassword"] | "";

  if (strlen(ssid) == 0 || strlen(ssid) > 32 || strlen(password) > 64) {
    gPortalServer.send(400, "application/json",
                       "{\"ok\":false,\"error\":\"VALIDATION_FAILED\"}");
    return;
  }

  strncpy(gUserSsid, ssid, sizeof(gUserSsid) - 1);
  gUserSsid[sizeof(gUserSsid) - 1] = '\0';
  strncpy(gUserPassword, password, sizeof(gUserPassword) - 1);
  gUserPassword[sizeof(gUserPassword) - 1] = '\0';

  savePortalSettingsToLittleFS();
  gPortalServer.send(200, "application/json", "{\"ok\":true}");
}

void handleHttpSaveSettingsRuntime() {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, gPortalServer.arg("plain"));
  if (error || !doc.is<JsonObject>()) {
    gPortalServer.send(400, "application/json",
                       "{\"ok\":false,\"error\":\"INVALID_REQUEST\"}");
    return;
  }
  JsonObjectConst root = doc.as<JsonObjectConst>();
  const uint32_t requested = root["scanIntervalMs"] | 0;
  if (requested < kMinScanIntervalMs || requested > kMaxScanIntervalMs) {
    gPortalServer.send(400, "application/json",
                       "{\"ok\":false,\"error\":\"VALIDATION_FAILED\"}");
    return;
  }

  gScanIntervalMs = requested;
  savePortalSettingsToLittleFS();
  gPortalServer.send(200, "application/json", "{\"ok\":true}");
}

void handleHttpReconnectWiFi() {
  gPortalReconnectRequested = true;
  gPortalServer.send(200, "application/json", "{\"ok\":true}");
}

void handleHttpReboot() {
  gPortalServer.send(200, "application/json", "{\"ok\":true}");
  gPortalServer.client().stop();
  delay(200);
  ESP.restart();
}

void handleHttpSnapshot() {
  JsonDocument doc;
  serializeRuntimeSnapshot(doc, millis());
  String body;
  serializeJson(doc, body);
  gPortalServer.send(200, "application/json", body);
}

void handleHttpCommand() {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, gPortalServer.arg("plain"));
  if (error || !doc.is<JsonObject>()) {
    gPortalServer.send(400, "application/json",
                       "{\"ok\":false,\"error\":\"INVALID_REQUEST\"}");
    return;
  }

  bool ok = applyCommand(doc.as<JsonObjectConst>());
  if (!ok) {
    gPortalServer.send(400, "application/json",
                       "{\"ok\":false,\"error\":\"COMMAND_REJECTED\"}");
    return;
  }

  gPortalServer.send(200, "application/json", "{\"ok\":true}");
}

void handleHttpGetActiveConfig() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["schemaVersion"] = 1;
  doc["activeVersion"] = gActiveVersion;
  JsonObject config = doc["config"].to<JsonObject>();
  JsonArray cards = config["cards"].to<JsonArray>();
  serializeCardsToArray(logicCards, cards);
  doc["error"] = nullptr;
  String body;
  serializeJson(doc, body);
  gPortalServer.send(200, "application/json", body);
}

void handleHttpStagedSaveConfig() {
  JsonDocument request;
  DeserializationError parseError =
      deserializeJson(request, gPortalServer.arg("plain"));
  if (parseError || !request.is<JsonObjectConst>()) {
    writeConfigErrorResponse(400, "INVALID_REQUEST", "invalid json");
    return;
  }

  JsonObjectConst root = request.as<JsonObjectConst>();
  JsonArrayConst cards;
  String reason;
  if (!extractConfigCardsFromRequest(root, cards, reason)) {
    writeConfigErrorResponse(400, "VALIDATION_FAILED", reason);
    return;
  }

  if (!writeJsonToPath(kStagedConfigPath, request)) {
    writeConfigErrorResponse(500, "COMMIT_FAILED", "failed to save staged file");
    return;
  }

  JsonDocument response;
  response["ok"] = true;
  response["stagedVersion"] = "staged";
  response["error"] = nullptr;
  String body;
  serializeJson(response, body);
  gPortalServer.send(200, "application/json", body);
}

void handleHttpStagedValidateConfig() {
  JsonDocument request;
  JsonArrayConst cards;
  String reason;

  if (gPortalServer.hasArg("plain") && gPortalServer.arg("plain").length() > 0) {
    DeserializationError parseError =
        deserializeJson(request, gPortalServer.arg("plain"));
    if (parseError || !request.is<JsonObjectConst>()) {
      writeConfigErrorResponse(400, "INVALID_REQUEST", "invalid json");
      return;
    }
    if (!extractConfigCardsFromRequest(request.as<JsonObjectConst>(), cards,
                                       reason)) {
      writeConfigErrorResponse(400, "VALIDATION_FAILED", reason);
      return;
    }
  } else {
    JsonDocument staged;
    if (!readJsonFromPath(kStagedConfigPath, staged) || !staged.is<JsonObjectConst>()) {
      writeConfigErrorResponse(404, "NOT_FOUND", "no staged config available");
      return;
    }
    if (!extractConfigCardsFromRequest(staged.as<JsonObjectConst>(), cards,
                                       reason)) {
      writeConfigErrorResponse(400, "VALIDATION_FAILED", reason);
      return;
    }
  }

  JsonDocument response;
  response["ok"] = true;
  JsonObject validation = response["validation"].to<JsonObject>();
  validation["errors"].to<JsonArray>();
  validation["warnings"].to<JsonArray>();
  String body;
  serializeJson(response, body);
  gPortalServer.send(200, "application/json", body);
}

bool rotateHistoryFiles() {
  if (!copyFileIfExists(kSlot2ConfigPath, kSlot3ConfigPath)) return false;
  if (!copyFileIfExists(kSlot1ConfigPath, kSlot2ConfigPath)) return false;
  if (!copyFileIfExists(kLkgConfigPath, kSlot1ConfigPath)) return false;
  if (!copyFileIfExists(kConfigPath, kLkgConfigPath)) return false;
  rotateHistoryVersions();
  return true;
}

void writeHistoryHead(JsonObject& head) {
  head["lkgVersion"] = gLkgVersion;
  head["slot1Version"] = gSlot1Version;
  head["slot2Version"] = gSlot2Version;
  head["slot3Version"] = gSlot3Version;
}

bool commitCards(JsonArrayConst cards, String& reason) {
  LogicCard nextCards[TOTAL_CARDS];
  if (!deserializeCardsFromArray(cards, nextCards)) {
    reason = "failed to parse cards";
    return false;
  }

  if (!rotateHistoryFiles()) {
    reason = "failed to rotate history slots";
    return false;
  }

  if (!saveCardsToPath(kConfigPath, nextCards)) {
    reason = "failed to persist active config";
    return false;
  }

  if (!applyCardsAsActiveConfig(nextCards)) {
    reason = "failed to apply active config to runtime";
    return false;
  }

  gConfigVersionCounter += 1;
  formatVersion(gActiveVersion, sizeof(gActiveVersion), gConfigVersionCounter);
  return true;
}

void handleHttpCommitConfig() {
  JsonDocument sourceDoc;
  JsonArrayConst cards;
  String reason;
  const bool hasInlinePayload =
      gPortalServer.hasArg("plain") && gPortalServer.arg("plain").length() > 0;

  if (hasInlinePayload) {
    DeserializationError parseError =
        deserializeJson(sourceDoc, gPortalServer.arg("plain"));
    if (parseError || !sourceDoc.is<JsonObjectConst>()) {
      writeConfigErrorResponse(400, "INVALID_REQUEST", "invalid json");
      return;
    }
    if (!extractConfigCardsFromRequest(sourceDoc.as<JsonObjectConst>(), cards,
                                       reason)) {
      writeConfigErrorResponse(400, "VALIDATION_FAILED", reason);
      return;
    }
  } else {
    if (!readJsonFromPath(kStagedConfigPath, sourceDoc) ||
        !sourceDoc.is<JsonObjectConst>()) {
      writeConfigErrorResponse(404, "NOT_FOUND", "no staged config available");
      return;
    }
    if (!extractConfigCardsFromRequest(sourceDoc.as<JsonObjectConst>(), cards,
                                       reason)) {
      writeConfigErrorResponse(400, "VALIDATION_FAILED", reason);
      return;
    }
  }

  if (!commitCards(cards, reason)) {
    writeConfigErrorResponse(500, "COMMIT_FAILED", reason);
    return;
  }

  JsonDocument response;
  response["ok"] = true;
  response["activeVersion"] = gActiveVersion;
  JsonObject head = response["historyHead"].to<JsonObject>();
  writeHistoryHead(head);
  response["requiresRestart"] = false;
  response["error"] = nullptr;
  String body;
  serializeJson(response, body);
  gPortalServer.send(200, "application/json", body);
}

void handleHttpRestoreConfig() {
  JsonDocument request;
  DeserializationError parseError =
      deserializeJson(request, gPortalServer.arg("plain"));
  if (parseError || !request.is<JsonObjectConst>()) {
    writeConfigErrorResponse(400, "INVALID_REQUEST", "invalid json");
    return;
  }

  const char* source = request["source"] | "";
  const char* restorePath = nullptr;
  if (strcmp(source, "LKG") == 0) restorePath = kLkgConfigPath;
  if (strcmp(source, "SLOT1") == 0) restorePath = kSlot1ConfigPath;
  if (strcmp(source, "SLOT2") == 0) restorePath = kSlot2ConfigPath;
  if (strcmp(source, "SLOT3") == 0) restorePath = kSlot3ConfigPath;
  if (strcmp(source, "FACTORY") == 0) restorePath = kFactoryConfigPath;
  if (restorePath == nullptr) {
    writeConfigErrorResponse(400, "VALIDATION_FAILED", "invalid restore source");
    return;
  }
  if (!LittleFS.exists(restorePath)) {
    writeConfigErrorResponse(404, "NOT_FOUND", "restore source not found");
    return;
  }

  JsonDocument doc;
  if (!readJsonFromPath(restorePath, doc) || !doc.is<JsonArrayConst>()) {
    writeConfigErrorResponse(500, "RESTORE_FAILED", "failed to load restore source");
    return;
  }
  JsonArrayConst cards = doc.as<JsonArrayConst>();
  String reason;
  if (!validateConfigCardsArray(cards, reason)) {
    writeConfigErrorResponse(500, "RESTORE_FAILED", reason);
    return;
  }
  if (!commitCards(cards, reason)) {
    writeConfigErrorResponse(500, "RESTORE_FAILED", reason);
    return;
  }

  JsonDocument response;
  response["ok"] = true;
  response["restoredFrom"] = source;
  response["activeVersion"] = gActiveVersion;
  response["requiresRestart"] = false;
  response["error"] = nullptr;
  String body;
  serializeJson(response, body);
  gPortalServer.send(200, "application/json", body);
}

void initPortalServer() {
  if (gPortalServerInitialized) return;
  gPortalServer.on("/", HTTP_GET, handleHttpRoot);
  gPortalServer.on("/config", HTTP_GET, handleHttpConfigPage);
  gPortalServer.on("/settings", HTTP_GET, handleHttpSettingsPage);
  gPortalServer.on("/api/snapshot", HTTP_GET, handleHttpSnapshot);
  gPortalServer.on("/api/command", HTTP_POST, handleHttpCommand);
  gPortalServer.on("/api/config/active", HTTP_GET, handleHttpGetActiveConfig);
  gPortalServer.on("/api/config/staged/save", HTTP_POST,
                   handleHttpStagedSaveConfig);
  gPortalServer.on("/api/config/staged/validate", HTTP_POST,
                   handleHttpStagedValidateConfig);
  gPortalServer.on("/api/config/commit", HTTP_POST, handleHttpCommitConfig);
  gPortalServer.on("/api/config/restore", HTTP_POST, handleHttpRestoreConfig);
  gPortalServer.on("/api/settings", HTTP_GET, handleHttpGetSettings);
  gPortalServer.on("/api/settings/wifi", HTTP_POST, handleHttpSaveSettingsWiFi);
  gPortalServer.on("/api/settings/runtime", HTTP_POST,
                   handleHttpSaveSettingsRuntime);
  gPortalServer.on("/api/settings/reconnect", HTTP_POST, handleHttpReconnectWiFi);
  gPortalServer.on("/api/settings/reboot", HTTP_POST, handleHttpReboot);
  gPortalServer.on("/favicon.ico", HTTP_GET,
                   []() { gPortalServer.send(204, "text/plain", ""); });
  gPortalServer.begin();
  gPortalServerInitialized = true;
  Serial.println("Portal HTTP server started on :80");
}

void handlePortalServerLoop() { gPortalServer.handleClient(); }

void handleWebSocketEvent(uint8_t clientNum, WStype_t type, uint8_t* payload,
                          size_t length) {
  if (type == WStype_CONNECTED) {
    IPAddress ip = gWsServer.remoteIP(clientNum);
    Serial.printf("WS client connected #%u from %u.%u.%u.%u\n", clientNum, ip[0],
                  ip[1], ip[2], ip[3]);
    return;
  }
  if (type == WStype_DISCONNECTED) {
    Serial.printf("WS client disconnected #%u\n", clientNum);
    return;
  }
  if (type != WStype_TEXT) return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(
      doc, reinterpret_cast<const char*>(payload), length);
  if (error || !doc.is<JsonObject>()) {
    gWsServer.sendTXT(clientNum,
                      "{\"type\":\"command_result\",\"ok\":false,"
                      "\"error\":{\"code\":\"INVALID_REQUEST\"}}");
    return;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  const char* typeStr = root["type"] | "";
  if (strcmp(typeStr, "command") != 0) {
    gWsServer.sendTXT(clientNum,
                      "{\"type\":\"command_result\",\"ok\":false,"
                      "\"error\":{\"code\":\"INVALID_REQUEST\"}}");
    return;
  }

  const char* requestId = root["requestId"] | "";
  bool ok = applyCommand(root);

  JsonDocument result;
  result["type"] = "command_result";
  result["schemaVersion"] = 1;
  result["requestId"] = requestId;
  result["ok"] = ok;
  if (!ok) {
    JsonObject err = result["error"].to<JsonObject>();
    err["code"] = "COMMAND_REJECTED";
  } else {
    result["error"] = nullptr;
  }
  String body;
  serializeJson(result, body);
  gWsServer.sendTXT(clientNum, body);
}

void initWebSocketServer() {
  if (gWsServerInitialized) return;
  gWsServer.begin();
  gWsServer.onEvent(handleWebSocketEvent);
  gWsServerInitialized = true;
  Serial.println("WebSocket server started on :81");
}

void handleWebSocketLoop() { gWsServer.loop(); }

void publishRuntimeSnapshotWebSocket() {
  static uint32_t lastPublishMs = 0;
  static uint32_t lastSeq = 0;

  SharedRuntimeSnapshot snapshot = {};
  copySharedRuntimeSnapshot(snapshot);
  uint32_t nowMs = millis();

  bool hasUpdate = (snapshot.seq != lastSeq);
  bool dueHeartbeat = (nowMs - lastPublishMs) >= 1000;
  if (!hasUpdate && !dueHeartbeat) return;
  if ((nowMs - lastPublishMs) < 200 && hasUpdate) return;

  JsonDocument doc;
  serializeRuntimeSnapshot(doc, nowMs);
  String payload;
  serializeJson(doc, payload);
  gWsServer.broadcastTXT(payload);

  lastPublishMs = nowMs;
  lastSeq = snapshot.seq;
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
  // Keep factory baseline aligned with current firmware defaults.
  {
    LogicCard factoryCards[TOTAL_CARDS];
    initializeCardArraySafeDefaults(factoryCards);
    saveCardsToPath(kFactoryConfigPath, factoryCards);
  }

  initializeAllCardsSafeDefaults();
  if (loadLogicCardsFromLittleFS()) {
    strncpy(gActiveVersion, "v1", sizeof(gActiveVersion) - 1);
    gActiveVersion[sizeof(gActiveVersion) - 1] = '\0';
    gConfigVersionCounter = 1;
    Serial.println("Loaded config from /config.json");
    return;
  }

  initializeAllCardsSafeDefaults();
  if (saveLogicCardsToLittleFS()) {
    strncpy(gActiveVersion, "v1", sizeof(gActiveVersion) - 1);
    gActiveVersion[sizeof(gActiveVersion) - 1] = '\0';
    gConfigVersionCounter = 1;
    Serial.println("Saved default config to /config.json");
  } else {
    Serial.println("Failed to save default JSON to /config.json");
  }
}

LogicCard* getCardById(uint8_t id) {
  if (id >= TOTAL_CARDS) return nullptr;
  return &logicCards[id];
}

bool isDigitalInputCard(uint8_t id) { return id < DO_START; }

bool isDigitalOutputCard(uint8_t id) { return id >= DO_START && id < AI_START; }

bool isAnalogInputCard(uint8_t id) { return id >= AI_START && id < SIO_START; }

bool isSoftIOCard(uint8_t id) { return id >= SIO_START && id < TOTAL_CARDS; }

bool isInputCard(uint8_t id) {
  return isDigitalInputCard(id) || isAnalogInputCard(id);
}

void initializeRuntimeControlState() {
  strncpy(gUserSsid, kDefaultUserSsid, sizeof(gUserSsid) - 1);
  gUserSsid[sizeof(gUserSsid) - 1] = '\0';
  strncpy(gUserPassword, kDefaultUserPassword, sizeof(gUserPassword) - 1);
  gUserPassword[sizeof(gUserPassword) - 1] = '\0';

  gRunMode = RUN_NORMAL;
  gScanIntervalMs = kDefaultScanIntervalMs;
  gScanCursor = 0;
  gStepRequested = false;
  gBreakpointPaused = false;
  gTestModeActive = false;
  gGlobalOutputMask = false;
  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    gCardBreakpoint[i] = false;
    gCardOutputMask[i] = false;
    gCardInputSource[i] = InputSource_Real;
    gCardForcedAIValue[i] = 0;
  }
}

bool isOutputMasked(uint8_t cardId) {
  if (!isDigitalOutputCard(cardId)) return false;
  return gGlobalOutputMask || gCardOutputMask[cardId];
}

bool setRunModeCommand(runMode mode) {
  gRunMode = mode;
  if (mode != RUN_BREAKPOINT) {
    gBreakpointPaused = false;
  }
  return true;
}

void serializeCardsToArray(const LogicCard* sourceCards, JsonArray& array) {
  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    JsonObject obj = array.add<JsonObject>();
    serializeCardToJson(sourceCards[i], obj);
  }
}

void initializeCardArraySafeDefaults(LogicCard* cards) {
  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    initializeCardSafeDefaults(cards[i], i);
  }
}

bool deserializeCardsFromArray(JsonArrayConst array, LogicCard* outCards) {
  if (array.size() != TOTAL_CARDS) return false;
  initializeCardArraySafeDefaults(outCards);
  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    JsonVariantConst item = array[i];
    if (!item.is<JsonObjectConst>()) return false;
    deserializeCardFromJson(item, outCards[i]);
  }
  return true;
}

bool validateConfigCardsArray(JsonArrayConst array, String& reason) {
  if (array.size() != TOTAL_CARDS) {
    reason = "cards size mismatch";
    return false;
  }

  auto cardTypeFromString = [](const char* s) -> logicCardType {
    return parseOrDefault(s, DigitalInput);
  };
  auto isModeAllowed = [](logicCardType type, const char* mode) -> bool {
    if (mode == nullptr) return false;
    if (type == DigitalInput) {
      return strcmp(mode, "Mode_DI_Rising") == 0 ||
             strcmp(mode, "Mode_DI_Falling") == 0 ||
             strcmp(mode, "Mode_DI_Change") == 0;
    }
    if (type == AnalogInput) return strcmp(mode, "Mode_AI_Continuous") == 0;
    if (type == DigitalOutput || type == SoftIO) {
      return strcmp(mode, "Mode_DO_Normal") == 0 ||
             strcmp(mode, "Mode_DO_Immediate") == 0 ||
             strcmp(mode, "Mode_DO_Gated") == 0;
    }
    return false;
  };
  auto isAlwaysOp = [](const char* op) -> bool {
    return op != nullptr &&
           (strcmp(op, "Op_AlwaysTrue") == 0 ||
            strcmp(op, "Op_AlwaysFalse") == 0);
  };
  auto isNumericOp = [](const char* op) -> bool {
    return op != nullptr &&
           (strcmp(op, "Op_GT") == 0 || strcmp(op, "Op_LT") == 0 ||
            strcmp(op, "Op_EQ") == 0 || strcmp(op, "Op_NEQ") == 0 ||
            strcmp(op, "Op_GTE") == 0 || strcmp(op, "Op_LTE") == 0);
  };
  auto isStateOp = [](const char* op) -> bool {
    return op != nullptr &&
           (strcmp(op, "Op_LogicalTrue") == 0 ||
            strcmp(op, "Op_LogicalFalse") == 0 ||
            strcmp(op, "Op_PhysicalOn") == 0 ||
            strcmp(op, "Op_PhysicalOff") == 0);
  };
  auto isTriggerOp = [](const char* op) -> bool {
    return op != nullptr &&
           (strcmp(op, "Op_Triggered") == 0 ||
            strcmp(op, "Op_TriggerCleared") == 0);
  };
  auto isProcessOp = [](const char* op) -> bool {
    return op != nullptr &&
           (strcmp(op, "Op_Running") == 0 ||
            strcmp(op, "Op_Finished") == 0 ||
            strcmp(op, "Op_Stopped") == 0);
  };
  auto isOperatorAllowedForTarget = [&](logicCardType targetType,
                                        const char* op) -> bool {
    if (isAlwaysOp(op)) return true;
    if (targetType == AnalogInput) return isNumericOp(op);
    if (targetType == DigitalInput) return isStateOp(op) || isTriggerOp(op) || isNumericOp(op);
    if (targetType == DigitalOutput || targetType == SoftIO) {
      return isStateOp(op) || isTriggerOp(op) || isNumericOp(op) ||
             isProcessOp(op);
    }
    return false;
  };
  auto isNonNegativeNumber = [](JsonVariantConst v) -> bool {
    if (v.is<uint64_t>()) return true;
    if (v.is<int64_t>()) return v.as<int64_t>() >= 0;
    if (v.is<double>()) return v.as<double>() >= 0.0;
    if (v.is<float>()) return v.as<float>() >= 0.0f;
    return false;
  };
  auto ensureNonNegativeField = [&](JsonObjectConst card, const char* fieldName,
                                    const char* label) -> bool {
    JsonVariantConst v = card[fieldName];
    if (!isNonNegativeNumber(v)) {
      reason = String(label) + " must be non-negative";
      return false;
    }
    return true;
  };

  bool seenId[TOTAL_CARDS] = {};
  logicCardType typeById[TOTAL_CARDS] = {};
  bool typeKnown[TOTAL_CARDS] = {};
  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    JsonVariantConst item = array[i];
    if (!item.is<JsonObjectConst>()) {
      reason = "cards[] item is not object";
      return false;
    }
    JsonObjectConst card = item.as<JsonObjectConst>();
    uint8_t id = card["id"] | 255;
    if (id >= TOTAL_CARDS) {
      reason = "card id out of range";
      return false;
    }
    if (seenId[id]) {
      reason = "duplicate card id";
      return false;
    }
    seenId[id] = true;
    typeById[id] = cardTypeFromString(card["type"] | "DigitalInput");
    typeKnown[id] = true;

    uint8_t setAId = card["setA_ID"] | 255;
    uint8_t setBId = card["setB_ID"] | 255;
    uint8_t resetAId = card["resetA_ID"] | 255;
    uint8_t resetBId = card["resetB_ID"] | 255;
    if (setAId >= TOTAL_CARDS || setBId >= TOTAL_CARDS ||
        resetAId >= TOTAL_CARDS || resetBId >= TOTAL_CARDS) {
      reason = "set/reset reference id out of range";
      return false;
    }
  }

  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    JsonObjectConst card = array[i].as<JsonObjectConst>();
    uint8_t id = card["id"] | 255;
    if (id >= TOTAL_CARDS || !typeKnown[id]) {
      reason = "card id/type map error";
      return false;
    }

    const char* mode = card["mode"] | "";
    if (!isModeAllowed(typeById[id], mode)) {
      reason = "mode not valid for card type (id=" + String(id) +
               ", type=" + String(toString(typeById[id])) +
               ", mode=" + String(mode) + ")";
      return false;
    }

    if (!ensureNonNegativeField(card, "hwPin", "hwPin")) return false;
    if (!ensureNonNegativeField(card, "setting1", "setting1")) return false;
    if (!ensureNonNegativeField(card, "setting2", "setting2")) return false;
    if (!ensureNonNegativeField(card, "setting3", "setting3")) return false;
    if (!ensureNonNegativeField(card, "startOnMs", "startOnMs")) return false;
    if (!ensureNonNegativeField(card, "startOffMs", "startOffMs")) return false;
    if (!ensureNonNegativeField(card, "setA_Threshold", "setA_Threshold"))
      return false;
    if (!ensureNonNegativeField(card, "setB_Threshold", "setB_Threshold"))
      return false;
    if (!ensureNonNegativeField(card, "resetA_Threshold", "resetA_Threshold"))
      return false;
    if (!ensureNonNegativeField(card, "resetB_Threshold", "resetB_Threshold"))
      return false;

    if (typeById[id] == AnalogInput) {
      const double alpha = card["setting3"] | 0.0;
      if (alpha < 0.0 || alpha > 1.0) {
        reason = "AI setting3 alpha out of range (0..1)";
        return false;
      }
    }

    uint8_t setAId = card["setA_ID"] | 255;
    uint8_t setBId = card["setB_ID"] | 255;
    uint8_t resetAId = card["resetA_ID"] | 255;
    uint8_t resetBId = card["resetB_ID"] | 255;
    const char* setAOp = card["setA_Operator"] | "";
    const char* setBOp = card["setB_Operator"] | "";
    const char* resetAOp = card["resetA_Operator"] | "";
    const char* resetBOp = card["resetB_Operator"] | "";

    if (!isOperatorAllowedForTarget(typeById[setAId], setAOp) ||
        !isOperatorAllowedForTarget(typeById[setBId], setBOp) ||
        !isOperatorAllowedForTarget(typeById[resetAId], resetAOp) ||
        !isOperatorAllowedForTarget(typeById[resetBId], resetBOp)) {
      reason = "operator not valid for referenced card type";
      return false;
    }
  }

  reason = "";
  return true;
}

bool writeJsonToPath(const char* path, JsonDocument& doc) {
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  size_t written = serializeJson(doc, file);
  file.close();
  return written > 0;
}

bool readJsonFromPath(const char* path, JsonDocument& doc) {
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  return !error;
}

bool loadPortalSettingsFromLittleFS() {
  if (!LittleFS.exists(kPortalSettingsPath)) return false;
  JsonDocument doc;
  if (!readJsonFromPath(kPortalSettingsPath, doc)) return false;
  if (!doc.is<JsonObjectConst>()) return false;
  JsonObjectConst root = doc.as<JsonObjectConst>();

  const char* userSsid = root["userSsid"] | "";
  const char* userPassword = root["userPassword"] | "";
  const uint32_t scanIntervalMs =
      root["scanIntervalMs"] | static_cast<uint32_t>(kDefaultScanIntervalMs);

  if (strlen(userSsid) > 0 && strlen(userSsid) <= 32) {
    strncpy(gUserSsid, userSsid, sizeof(gUserSsid) - 1);
    gUserSsid[sizeof(gUserSsid) - 1] = '\0';
  }
  if (strlen(userPassword) <= 64) {
    strncpy(gUserPassword, userPassword, sizeof(gUserPassword) - 1);
    gUserPassword[sizeof(gUserPassword) - 1] = '\0';
  }
  if (scanIntervalMs >= kMinScanIntervalMs &&
      scanIntervalMs <= kMaxScanIntervalMs) {
    gScanIntervalMs = scanIntervalMs;
  }
  return true;
}

bool savePortalSettingsToLittleFS() {
  JsonDocument doc;
  doc["userSsid"] = gUserSsid;
  doc["userPassword"] = gUserPassword;
  doc["scanIntervalMs"] = gScanIntervalMs;
  return writeJsonToPath(kPortalSettingsPath, doc);
}

bool saveCardsToPath(const char* path, const LogicCard* sourceCards) {
  JsonDocument doc;
  JsonArray cards = doc.to<JsonArray>();
  serializeCardsToArray(sourceCards, cards);
  return writeJsonToPath(path, doc);
}

bool loadCardsFromPath(const char* path, LogicCard* outCards) {
  JsonDocument doc;
  if (!readJsonFromPath(path, doc)) return false;
  if (!doc.is<JsonArrayConst>()) return false;
  return deserializeCardsFromArray(doc.as<JsonArrayConst>(), outCards);
}

bool copyFileIfExists(const char* srcPath, const char* dstPath) {
  if (!LittleFS.exists(srcPath)) return true;
  File src = LittleFS.open(srcPath, "r");
  if (!src) return false;
  File dst = LittleFS.open(dstPath, "w");
  if (!dst) {
    src.close();
    return false;
  }
  uint8_t buffer[256];
  while (true) {
    size_t n = src.read(buffer, sizeof(buffer));
    if (n == 0) break;
    if (dst.write(buffer, n) != n) {
      src.close();
      dst.close();
      return false;
    }
  }
  src.close();
  dst.close();
  return true;
}

void formatVersion(char* out, size_t outSize, uint32_t version) {
  snprintf(out, outSize, "v%lu", static_cast<unsigned long>(version));
}

bool pauseKernelForConfigApply(uint32_t timeoutMs) {
  gKernelPauseRequested = true;
  uint32_t start = millis();
  while (!gKernelPaused && (millis() - start) < timeoutMs) {
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  return gKernelPaused;
}

void resumeKernelAfterConfigApply() { gKernelPauseRequested = false; }

void rotateHistoryVersions() {
  strncpy(gSlot3Version, gSlot2Version, sizeof(gSlot3Version) - 1);
  gSlot3Version[sizeof(gSlot3Version) - 1] = '\0';
  strncpy(gSlot2Version, gSlot1Version, sizeof(gSlot2Version) - 1);
  gSlot2Version[sizeof(gSlot2Version) - 1] = '\0';
  strncpy(gSlot1Version, gLkgVersion, sizeof(gSlot1Version) - 1);
  gSlot1Version[sizeof(gSlot1Version) - 1] = '\0';
  strncpy(gLkgVersion, gActiveVersion, sizeof(gLkgVersion) - 1);
  gLkgVersion[sizeof(gLkgVersion) - 1] = '\0';
}

bool applyCardsAsActiveConfig(const LogicCard* newCards) {
  if (!pauseKernelForConfigApply(1000)) {
    resumeKernelAfterConfigApply();
    return false;
  }
  memcpy(logicCards, newCards, sizeof(logicCards));
  memset(gPrevSetCondition, 0, sizeof(gPrevSetCondition));
  memset(gPrevDISample, 0, sizeof(gPrevDISample));
  memset(gPrevDIPrimed, 0, sizeof(gPrevDIPrimed));
  updateSharedRuntimeSnapshot(millis(), false);
  resumeKernelAfterConfigApply();
  return true;
}

bool extractConfigCardsFromRequest(JsonObjectConst root, JsonArrayConst& outCards,
                                   String& reason) {
  if (!root["config"].is<JsonObjectConst>()) {
    reason = "missing config object";
    return false;
  }
  JsonObjectConst config = root["config"].as<JsonObjectConst>();
  if (!config["cards"].is<JsonArrayConst>()) {
    reason = "missing config.cards array";
    return false;
  }
  outCards = config["cards"].as<JsonArrayConst>();
  if (!validateConfigCardsArray(outCards, reason)) return false;
  reason = "";
  return true;
}

void writeConfigErrorResponse(int statusCode, const char* code,
                              const String& message) {
  JsonDocument doc;
  doc["ok"] = false;
  JsonObject error = doc["error"].to<JsonObject>();
  error["code"] = code;
  error["message"] = message;
  String body;
  serializeJson(doc, body);
  gPortalServer.send(statusCode, "application/json", body);
}

bool requestStepCommand() {
  gStepRequested = true;
  gBreakpointPaused = false;
  gRunMode = RUN_STEP;
  return true;
}

bool setBreakpointCommand(uint8_t cardId, bool enabled) {
  if (cardId >= TOTAL_CARDS) return false;
  gCardBreakpoint[cardId] = enabled;
  if (!enabled) gBreakpointPaused = false;
  return true;
}

bool setTestModeCommand(bool active) {
  gTestModeActive = active;
  if (!active) {
    for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
      gCardInputSource[i] = InputSource_Real;
      gCardOutputMask[i] = false;
      gCardForcedAIValue[i] = 0;
    }
    gGlobalOutputMask = false;
  }
  return true;
}

bool setOutputMaskCommand(uint8_t cardId, bool masked) {
  if (cardId >= TOTAL_CARDS) return false;
  if (!isDigitalOutputCard(cardId)) return false;
  gCardOutputMask[cardId] = masked;
  return true;
}

bool setGlobalOutputMaskCommand(bool masked) {
  gGlobalOutputMask = masked;
  return true;
}

bool setInputForceCommand(uint8_t cardId, inputSourceMode mode,
                          uint32_t forcedValue) {
  if (cardId >= TOTAL_CARDS) return false;
  if (!isInputCard(cardId)) return false;

  if (isDigitalInputCard(cardId)) {
    if (mode == InputSource_ForcedValue) return false;
  } else if (isAnalogInputCard(cardId)) {
    if (mode == InputSource_ForcedHigh || mode == InputSource_ForcedLow) {
      return false;
    }
  }

  gCardInputSource[cardId] = mode;
  if (mode == InputSource_ForcedValue) gCardForcedAIValue[cardId] = forcedValue;
  if (mode == InputSource_Real) gCardForcedAIValue[cardId] = 0;
  return true;
}

bool enqueueKernelCommand(const KernelCommand& command) {
  if (gKernelCommandQueue == nullptr) return false;
  return xQueueSend(gKernelCommandQueue, &command, 0) == pdTRUE;
}

bool applyKernelCommand(const KernelCommand& command) {
  switch (command.type) {
    case KernelCmd_SetRunMode:
      return setRunModeCommand(command.mode);
    case KernelCmd_StepOnce:
      return requestStepCommand();
    case KernelCmd_SetBreakpoint:
      return setBreakpointCommand(command.cardId, command.flag);
    case KernelCmd_SetTestMode:
      return setTestModeCommand(command.flag);
    case KernelCmd_SetInputForce:
      return setInputForceCommand(command.cardId, command.inputMode,
                                  command.value);
    case KernelCmd_SetOutputMask:
      return setOutputMaskCommand(command.cardId, command.flag);
    case KernelCmd_SetOutputMaskGlobal:
      return setGlobalOutputMaskCommand(command.flag);
    default:
      return false;
  }
}

void processKernelCommandQueue() {
  if (gKernelCommandQueue == nullptr) return;
  KernelCommand command = {};
  while (xQueueReceive(gKernelCommandQueue, &command, 0) == pdTRUE) {
    applyKernelCommand(command);
  }
}

void updateSharedRuntimeSnapshot(uint32_t nowMs, bool incrementSeq) {
  portENTER_CRITICAL(&gSnapshotMux);
  if (incrementSeq) gSharedSnapshot.seq += 1;
  gSharedSnapshot.tsMs = nowMs;
  gSharedSnapshot.lastCompleteScanUs = gLastCompleteScanUs;
  gSharedSnapshot.mode = gRunMode;
  gSharedSnapshot.testModeActive = gTestModeActive;
  gSharedSnapshot.globalOutputMask = gGlobalOutputMask;
  gSharedSnapshot.breakpointPaused = gBreakpointPaused;
  gSharedSnapshot.scanCursor = gScanCursor;
  memcpy(gSharedSnapshot.cards, logicCards, sizeof(logicCards));
  memcpy(gSharedSnapshot.inputSource, gCardInputSource, sizeof(gCardInputSource));
  memcpy(gSharedSnapshot.forcedAIValue, gCardForcedAIValue,
         sizeof(gCardForcedAIValue));
  memcpy(gSharedSnapshot.outputMaskLocal, gCardOutputMask,
         sizeof(gCardOutputMask));
  memcpy(gSharedSnapshot.breakpointEnabled, gCardBreakpoint,
         sizeof(gCardBreakpoint));
  portEXIT_CRITICAL(&gSnapshotMux);
}

uint8_t scanOrderCardIdFromCursor(uint16_t cursor) {
  uint16_t pos = (TOTAL_CARDS == 0) ? 0 : (cursor % TOTAL_CARDS);
  if (pos < NUM_DI) return static_cast<uint8_t>(DI_START + pos);
  pos -= NUM_DI;
  if (pos < NUM_AI) return static_cast<uint8_t>(AI_START + pos);
  pos -= NUM_AI;
  if (pos < NUM_SIO) return static_cast<uint8_t>(SIO_START + pos);
  pos -= NUM_SIO;
  return static_cast<uint8_t>(DO_START + pos);
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
  inputSourceMode sourceMode = InputSource_Real;
  if (card.id < TOTAL_CARDS) {
    sourceMode = gCardInputSource[card.id];
  }

  if (sourceMode == InputSource_ForcedHigh) {
    sample = true;
  } else if (sourceMode == InputSource_ForcedLow) {
    sample = false;
  } else if (card.hwPin != 255) {
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
  inputSourceMode sourceMode = InputSource_Real;
  if (card.id < TOTAL_CARDS) {
    sourceMode = gCardInputSource[card.id];
  }

  if (sourceMode == InputSource_ForcedValue) {
    raw = gCardForcedAIValue[card.id];
  } else if (card.hwPin != 255) {
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

void forceDOIdle(LogicCard& card, bool clearCounter = false) {
  card.logicalState = false;
  card.physicalState = false;
  card.triggerFlag = false;
  card.startOnMs = 0;
  card.startOffMs = 0;
  card.repeatCounter = 0;
  if (clearCounter) card.currentValue = 0;
  card.state = State_DO_Idle;
}

void driveDOHardware(const LogicCard& card, bool driveHardware, bool level,
                     bool masked) {
  if (!driveHardware) return;
  if (card.hwPin == 255) return;
  if (masked) return;
  digitalWrite(card.hwPin, level ? HIGH : LOW);
}

void processDOCard(LogicCard& card, uint32_t nowMs, bool driveHardware) {
  const bool previousPhysical = card.physicalState;
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
    forceDOIdle(card, true);
    driveDOHardware(card, driveHardware, false, isOutputMasked(card.id));
    return;
  }

  const bool retriggerable =
      (card.state == State_DO_Idle || card.state == State_DO_Finished);
  // Re-arm behavior: when DO/SIO is idle/finished, allow retrigger even if
  // setCondition stays high (level retrigger), while still supporting edge
  // trigger semantics for normal transitions.
  card.triggerFlag = retriggerable && (setRisingEdge || setCondition);

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
    driveDOHardware(card, driveHardware, false, isOutputMasked(card.id));
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

  // DO/SIO cycle counter: count each OFF->ON transition of effective output.
  if (!previousPhysical && effectiveOutput) {
    card.currentValue += 1;
  }

  card.physicalState = effectiveOutput;
  driveDOHardware(card, driveHardware, effectiveOutput, isOutputMasked(card.id));
}

void processSIOCard(LogicCard& card, uint32_t nowMs) {
  processDOCard(card, nowMs, false);
}

void processCardById(uint8_t cardId, uint32_t nowMs) {
  if (cardId >= TOTAL_CARDS) return;
  if (isDigitalInputCard(cardId)) {
    processDICard(logicCards[cardId], nowMs);
    return;
  }
  if (isAnalogInputCard(cardId)) {
    processAICard(logicCards[cardId]);
    return;
  }
  if (isSoftIOCard(cardId)) {
    processSIOCard(logicCards[cardId], nowMs);
    return;
  }
  if (isDigitalOutputCard(cardId)) {
    processDOCard(logicCards[cardId], nowMs, true);
  }
}

void processOneScanOrderedCard(uint32_t nowMs, bool honorBreakpoints) {
  uint8_t cardId = scanOrderCardIdFromCursor(gScanCursor);
  processCardById(cardId, nowMs);

  gScanCursor = static_cast<uint16_t>((gScanCursor + 1) % TOTAL_CARDS);

  if (honorBreakpoints && gRunMode == RUN_BREAKPOINT && gCardBreakpoint[cardId]) {
    gBreakpointPaused = true;
  }
}

bool runFullScanCycle(uint32_t nowMs, bool honorBreakpoints) {
  for (uint8_t i = 0; i < TOTAL_CARDS; ++i) {
    processOneScanOrderedCard(nowMs, honorBreakpoints);
    if (gBreakpointPaused) return false;
  }
  return true;
}

void runEngineIteration(uint32_t nowMs, uint32_t& lastScanMs) {
  processKernelCommandQueue();
  if (gKernelPauseRequested) {
    gKernelPaused = true;
    updateSharedRuntimeSnapshot(nowMs, false);
    return;
  }
  gKernelPaused = false;
  if (lastScanMs == 0) {
    lastScanMs = nowMs;
  }

  uint32_t scanInterval = (gRunMode == RUN_SLOW) ? SLOW_SCAN_INTERVAL_MS
                                                 : gScanIntervalMs;
  if ((nowMs - lastScanMs) < scanInterval) {
    updateSharedRuntimeSnapshot(nowMs, false);
    return;
  }
  lastScanMs += scanInterval;

  if (gRunMode == RUN_STEP) {
    if (gStepRequested) {
      processOneScanOrderedCard(nowMs, false);
      gStepRequested = false;
      updateSharedRuntimeSnapshot(nowMs, true);
      return;
    }
    updateSharedRuntimeSnapshot(nowMs, false);
    return;
  }

  if (gRunMode == RUN_BREAKPOINT && gBreakpointPaused) {
    updateSharedRuntimeSnapshot(nowMs, false);
    return;
  }

  uint32_t scanStartUs = micros();
  bool completedFullScan = runFullScanCycle(nowMs, gRunMode == RUN_BREAKPOINT);
  uint32_t scanEndUs = micros();
  if (completedFullScan) {
    gLastCompleteScanUs = (scanEndUs - scanStartUs);
  }
  updateSharedRuntimeSnapshot(nowMs, true);
}

void core0EngineTask(void* param) {
  (void)param;
  uint32_t lastScanMs = 0;
  for (;;) {
    runEngineIteration(millis(), lastScanMs);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void core1PortalTask(void* param) {
  (void)param;
  bool wifiOk = connectWiFiWithPolicy();
  if (wifiOk) {
    initPortalServer();
    initWebSocketServer();
  }
  for (;;) {
    if (wifiOk) {
      if (gPortalReconnectRequested) {
        gPortalReconnectRequested = false;
        wifiOk = connectWiFiWithPolicy();
        if (wifiOk) {
          initPortalServer();
          initWebSocketServer();
        }
      }
      handlePortalServerLoop();
      handleWebSocketLoop();
      publishRuntimeSnapshotWebSocket();
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    // Optional low-frequency retry in offline mode.
    static uint32_t lastRetryMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastRetryMs >= 30000) {
      lastRetryMs = nowMs;
      wifiOk = connectWiFiWithPolicy();
      if (wifiOk) {
        initPortalServer();
        initWebSocketServer();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  Serial.begin(115200);
  configureHardwarePinsSafeState();
  initializeRuntimeControlState();

  bool fsReady = LittleFS.begin(true);
  if (!fsReady) {
    Serial.println("LittleFS mount failed");
    initializeAllCardsSafeDefaults();
  } else {
    if (!loadPortalSettingsFromLittleFS()) {
      savePortalSettingsToLittleFS();
    }
    bootstrapCardsFromStorage();
  }

  gKernelCommandQueue = xQueueCreate(16, sizeof(KernelCommand));
  if (gKernelCommandQueue == nullptr) {
    Serial.println("Failed to create kernel command queue");
    return;
  }

  updateSharedRuntimeSnapshot(millis(), false);

  xTaskCreatePinnedToCore(core0EngineTask, "core0_engine", 8192, nullptr, 3,
                          &gCore0TaskHandle, 0);
  xTaskCreatePinnedToCore(core1PortalTask, "core1_portal", 8192, nullptr, 1,
                          &gCore1TaskHandle, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

bool applyCommand(JsonObjectConst command) {
  const char* name = command["name"] | "";
  JsonObjectConst payload = command["payload"].as<JsonObjectConst>();
  KernelCommand kernelCommand = {};

  if (strcmp(name, "set_run_mode") == 0) {
    const char* mode = payload["mode"] | "RUN_NORMAL";
    kernelCommand.type = KernelCmd_SetRunMode;
    bool modeMatched = false;
    if (strcmp(mode, "RUN_NORMAL") == 0) {
      kernelCommand.mode = RUN_NORMAL;
      modeMatched = true;
    }
    if (strcmp(mode, "RUN_STEP") == 0) {
      kernelCommand.mode = RUN_STEP;
      modeMatched = true;
    }
    if (strcmp(mode, "RUN_BREAKPOINT") == 0) {
      kernelCommand.mode = RUN_BREAKPOINT;
      modeMatched = true;
    }
    if (strcmp(mode, "RUN_SLOW") == 0) {
      kernelCommand.mode = RUN_SLOW;
      modeMatched = true;
    }
    if (!modeMatched) return false;
    return enqueueKernelCommand(kernelCommand);
  }

  if (strcmp(name, "step_once") == 0) {
    kernelCommand.type = KernelCmd_StepOnce;
    return enqueueKernelCommand(kernelCommand);
  }

  if (strcmp(name, "set_breakpoint") == 0) {
    kernelCommand.type = KernelCmd_SetBreakpoint;
    kernelCommand.cardId = payload["cardId"] | 255;
    kernelCommand.flag = payload["enabled"] | false;
    return enqueueKernelCommand(kernelCommand);
  }

  if (strcmp(name, "set_test_mode") == 0) {
    kernelCommand.type = KernelCmd_SetTestMode;
    kernelCommand.flag = payload["active"] | false;
    return enqueueKernelCommand(kernelCommand);
  }

  if (strcmp(name, "set_input_force") == 0) {
    uint8_t cardId = payload["cardId"] | 255;
    bool forced = payload["forced"] | false;
    kernelCommand.type = KernelCmd_SetInputForce;
    kernelCommand.cardId = cardId;
    if (!forced) {
      kernelCommand.inputMode = InputSource_Real;
      kernelCommand.value = 0;
      return enqueueKernelCommand(kernelCommand);
    }

    if (isDigitalInputCard(cardId)) {
      bool value = payload["value"] | false;
      kernelCommand.inputMode =
          value ? InputSource_ForcedHigh : InputSource_ForcedLow;
      kernelCommand.value = 0;
      return enqueueKernelCommand(kernelCommand);
    }
    if (isAnalogInputCard(cardId)) {
      kernelCommand.inputMode = InputSource_ForcedValue;
      kernelCommand.value = payload["value"] | 0;
      return enqueueKernelCommand(kernelCommand);
    }
    return false;
  }

  if (strcmp(name, "set_output_mask") == 0) {
    kernelCommand.type = KernelCmd_SetOutputMask;
    kernelCommand.cardId = payload["cardId"] | 255;
    kernelCommand.flag = payload["masked"] | false;
    return enqueueKernelCommand(kernelCommand);
  }

  if (strcmp(name, "set_output_mask_global") == 0) {
    kernelCommand.type = KernelCmd_SetOutputMaskGlobal;
    kernelCommand.flag = payload["masked"] | false;
    return enqueueKernelCommand(kernelCommand);
  }

  return false;
}
