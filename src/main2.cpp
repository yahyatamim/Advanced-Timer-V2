
// Gemini Basic Command : please dont give me any code or anything. you have to
// remember to always keep your answers very short

#include <Arduino.h>
#include <ArduinoJson.h>
#include <TickTwo.h>

// --- Card counts (can be changed later) ---
const uint8_t NUM_DI = 4;
const uint8_t NUM_DO = 4;
const uint8_t NUM_AI = 2;
const uint8_t NUM_SIO = 4;
const uint8_t NUM_TIMER = 2;
const uint8_t NUM_COUNTER = 2;

const uint8_t DI_Pins[NUM_DI] = {13, 12, 14, 27};  // Digital Input pins
const uint8_t DO_Pins[NUM_DO] = {26, 25, 33, 32};  // Digital Output pins
const uint8_t AI_Pins[NUM_AI] = {35, 34};          // Analog Input pins

// This enum defines the fundamental types of "logic cards" in the system.
// Each represents an independent state machine with SET/RESET logic.
enum LogicCardType {
  DigitalInput,   // A simple ON/OFF input signal from a sensor or switch
  DigitalOutput,  // A Relay that can be turned ON/OFF based on conditions
  AnalogInput,    // A Variable Signal that can be compared against thresholds,
  SoftIO,         // Will act like a Digital Output but withoutphysical relay
  Timer,          // A General Purpose Timer Counter // A
  Counter         // A General Purpose Counter
};

// Defines the comparison logic for input signals
enum LogicOperator {
  Op_None,        // Output is always false (used for unused conditions)
  Op_AlwaysTrue,  // Output is always true (used for non-conditional actions)
  // Boolean operators on logicalState
  Op_LogicalTrue,   // True if logicalState is ON
  Op_LogicalFalse,  // True if logicalState is OFF

  // Boolean operators on physicalState
  Op_PhysicalOn,   // True if physicalState is ON
  Op_PhysicalOff,  // True if physicalState is OFF

  // Boolean/event operators on triggerFlag
  Op_Triggered,       // True if triggerFlag set this scan cycle
  Op_TriggerCleared,  // True if triggerFlag cleared this scan cycle

  // Numeric operators on currentValue only
  Op_GT,   // Greater than threshold (currentValue > threshold)
  Op_LT,   // Less than threshold
  Op_EQ,   // Equal to threshold
  Op_NEQ,  // Not equal to threshold
  Op_GTE,  // Greater or equal
  Op_LTE,  // Less or equal

  // Timer/Counter state operators (on state or logicalState)
  Op_Running,   // Timer/Counter running
  Op_Finished,  // Timer finished
  Op_Stopped    // Timer stopped
};

// Defines how conditions are combined (AND/OR logic)
enum LogicCombine {
  Combine_None,  // Only use primary condition
  Combine_AND,   // Both primary and secondery conditions must be true
  Combine_OR,    // Either primary or secondary condition must be true
};

// Represents the "Condition Builder" for SET or RESET
struct LogicCondition {
  int sourceA_ID;        // Index of the signal source on the Global Bus
  LogicOperator opA;     // The comparison to perform for the primary signal
  uint32_t thresholdA;   // Threshold value for comparisons (if applicable)
  int sourceB_ID;        // Optional second signal for comparison
  LogicOperator opB;     // Comparison for the second signal (if used)
  uint32_t thresholdB;   // Threshold value for the second signal (if used)
  LogicCombine combine;  // How to combine with other conditions
};

enum CardMode {
  Mode_None,  // Deactivate card (ignore inputs, outputs off) - default state
  // DigitalInput modes (DI)
  DI_Immediate,  // For DI: Update state immediately based on input signal
  DI_Debounced,  // For DI: Only change state if signal is stable for setting1
                 // cycles
  // DigitalOutput modes (DO/SIO)
  Out_NoDelay,  // For DO: Activate immediately when set condition is met
  Out_OnDelay,  // For DO: Activate after delay time (setting1) once set
                // condition is met
  Out_AutoOff,  // For DO: Automatically turn off after duration (setting2) once
                // activated
  Out_OnOff,    // For DO: Use both onDelay and autoOff
  // AnalogInput modes (AI)
  AI_Immediate,  // For AI: Update currentValue immediately from the bus signal
  AI_Smoothed,   // For AI: Update currentValue as a moving average (window size
                 // = setting2)
  // Timer modes (T)
  OneShot,    // For Timer: Run once when set condition is met, then stop
  Repeating,  // For Timer: Automatically restart after finishing until reset
  // Counter modes (C)
  Count_Up,   // For Counter: Increment count when set condition is met
  Count_Down  // For Counter: Decrement count when set condition is met
};

enum CardState {
  State_None,
  // For Digital Outputs (DO/SIO)
  State_Off,      // Output is currently OFF
  State_OnDelay,  // Set condition met, waiting for on-delay timer
  State_AutoOff,  // Set condition met, waiting for auto-off timer
  State_On,       // Output is currently ON
  // For Digital Inputs (DI)
  // For Analog Inputs (AI)
  // For Timers (T)
  State_Ready,     // Timer is ready to be triggered
  State_Running,   // Timer is currently running
  State_Finished,  // Timer has completed its duration
  State_Stopped,   // Timer is not running
  // For Counters (C)
  State_Idle,          // Counter is idle, waiting for set condition
  State_Counting,      // Counter is actively counting
  State_TargetReached  // Counter has reached its target count
};

// The "DNA" of a card: State, Logic, and Timing
struct LogicCard {
  uint8_t id;          // Unique identifier for the card
  LogicCardType type;  // Type of card (DI, DO, AI, SIO, Timer, Counter)
  uint8_t index;       // Index within its type category
  uint8_t hwPin;       // Hardware pin or driver ID (if applicable)
  float constant1;  // Generic constant for scaling or other use (e.g. AI scale
                    // factor)
  float constant2;  // Another generic constant (e.g. AI offset)

  /*
    logicalState:
    DI: Debounced input state (ON/OFF)
    DO: Desired relay state (ON/OFF)
    AI: Threshold evaluation result (true/false)
    SoftIO: Virtual relay state (ON/OFF)
    Timer: Running/completed state (ON=running)
    Counter: Active/target reached state (ON=active)
  */
  bool logicalState;

  /*
    physicalState:
    DI: Raw hardware input state
    DO: Actual relay hardware state
    AI: SKIP
    SoftIO: Reflects On-delay / Auto-off / current output state
    Timer: SKIP
    Counter: SKIP
  */
  bool physicalState;

  /*
    triggerFlag:
    DI: Edge detected within scan cycle (interrupt or event)
    DO: One-shot set/reset trigger latch
    AI: Threshold-crossing event latch
    SoftIO: Anti-retrigger latch
    Timer: Start-trigger latch
    Counter: Increment/overflow trigger latch
  */
  bool triggerFlag;

  // setCondition / resetCondition: Used by all except DI/AI (UI-hidden)
  LogicCondition setCondition;
  LogicCondition resetCondition;

  /*
    setting1 (Primary Config):
    DI: Debounce time (ms)
    DO: On-delay time (ms)
    AI: Threshold value
    SoftIO: On-delay time (ms)
    Timer: Duration (ms)
    Counter: Target count
  */
  uint32_t setting1;

  /*
    setting2 (Secondary Config):
    DI: Filter window / stability cycles
    DO: Auto-off time (ms)
    AI: Moving average window
    SoftIO: Auto-off time (ms)
    Timer: Optional auto-reset or hold
    Counter: Reset mode / step size
  */
  uint32_t setting2;

  /*
    currentValue (Runtime Data):
    DI: Debounce counter / event counter
    DO: Elapsed on-delay / auto-off timer
    AI: Current analog reading (scaled)
    SoftIO: Elapsed on-delay / auto-off timer (mirrors DO)
    Timer: Elapsed time
    Counter: Current count
  */
  uint32_t currentValue;

  /*
    mode: Behavior option
    DI: DI_Immediate / DI_Debounced
    DO: Out_NoDelay / Out_OnDelay / Out_AutoOff / Out_OnOff
    AI: AI_Immediate / AI_Smoothed
    SoftIO: Same as DO
    Timer: OneShot / Repeating
    Counter: Count_Up / Count_Down
  */
  CardMode mode;

  /*
    state: Current internal flow state
    DI: SKIP
    DO: State_Off / State_OnDelay / State_AutoOff / State_On
    AI: SKIP
    SoftIO: Same as DO
    Timer: State_Ready / State_Running / State_Finished / State_Stopped
    Counter: State_Idle / State_Counting / State_TargetReached
  */
  CardState state;

  /*
    allowRetrigger: Optional retrigger behavior
    DI: Edge retrigger enable
    DO: Allow repeated SET before RESET
    AI: Allow repeated threshold triggers
    SoftIO: Same as DO
    Timer: Restart while running allowed?
    Counter: Allow overflow / wrap-around
  */
  bool allowRetrigger;
};

// void setup() {}

// void loop() {}
