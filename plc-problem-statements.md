# PLC Automation Demo Problem Statements

Ordered from easiest to most difficult in terms of system-level thinking.

## 1. Safety-Gated Mixer Startup

### Scenario
A small industrial mixer has a pre-heater, a main drive motor, multiple safety gate interlock switches, a Start pushbutton, a Reset pushbutton, and a visible alarm indicator. The process requires a strict startup order to protect equipment and operators. The area is operator-accessible, so any loss of guard integrity must force immediate shutdown.

### Required Functionality
When the operator presses Start, energize the pre-heater for exactly 45 seconds, then start the main motor only after pre-heat completes. Safety gates must remain closed during startup and run. If any gate opens, de-energize all outputs immediately, latch the alarm, and block restart until Reset is pressed under safe conditions. At every successful startup completion, issue a 200 ms ready pulse output.

## 2. Pneumatic Pick-and-Place with Timed Batch Cycle

### Scenario
A compact assembly cell uses two pneumatic cylinders, position limit switches, a spray valve, an unload beacon, a light-curtain safety sensor, Start/Stop/Reset buttons, and operator-facing cycle count visibility. The machine combines motion sequencing and timed process phases, and must fail safe on intrusion.

### Required Functionality
On Start, execute the sequence: Cylinder A extend, confirm A extended, Cylinder B extend, hold press dwell for 3 seconds, then retract both cylinders and verify both home sensors before allowing another cycle. For coating mode, run timed phases of spray 60 seconds, dwell 25 seconds, unload beacon 10 seconds, and repeat up to 30 cycles unless Stop is pressed. If the light curtain is interrupted during any hazardous phase, abort immediately, de-energize all outputs, latch fault, and require Reset. Generate one completion pulse per full cycle and maintain a cycle counter.

## 3. Conveyor Inspection, Counting, and Rejection Control

### Scenario
A parts conveyor includes infeed photo-eyes, jam detection, an assembly-station quality sensor, reject solenoid, conveyor drive, stack light, and downstream transfer signal. The line must count only good parts, remove defective parts, and keep production moving without jams.

### Required Functionality
Count part arrivals on photo-eye rising edges. Stop conveyor for short marking/inspection windows where required, then resume automatically. If jam sensor remains blocked longer than 5 seconds, stop conveyor, pulse red stack light at 1 Hz, latch fault, and require manual reset. For each defective part, fire reject solenoid for 800 ms, exclude it from good-count totals, and toggle a reject-indicator output. Index conveyor for 4 seconds when 12 good parts are accumulated at the loading stage. When 8 good assembled parts are completed, pulse an advance output for 300 ms and reset batch count. Emergency stop must drop all outputs instantly; clear/reset must restore operation and reset counters.

## 4. Ventilation and Climate Control with Override Modes

### Scenario
A controlled environment (greenhouse or equipment room) uses 4-20 mA temperature and humidity transmitters, primary exhaust fan, optional booster fan, occupancy input, manual Fan On/Fan Off pushbuttons, emergency stop, and facility override signals such as fire mode. The system must balance equipment protection, energy use, and safe manual servicing.

### Required Functionality
Start ventilation when temperature exceeds 27 C or humidity exceeds 82 percent. Stop only when temperature is below 23 C and humidity is below 75 percent to enforce dual-variable hysteresis. Once started, enforce minimum fan runtime of 90 seconds. Enable booster fan at high temperature thresholds, but allow occupancy/maintenance mode to disable automatic behavior and permit local manual fan control. Emergency stop must force immediate fan shutdown and hold outputs off until reset. Provide alarm/fault signaling for failed fan proof or forced facility override states.

## 5. Single-Pump Level Control with Dry-Run Protection

### Scenario
A buffer tank or sump uses a single transfer pump, a 4-20 mA level transmitter, high-level alarm, low-level dry-run condition, emergency stop, reset input, and optional manual override switch. Process demand is variable and the pump must avoid short-cycling and cavitation.

### Required Functionality
In automatic mode, start pump when level rises above 65 percent and stop when level falls below 25 percent, with anti-cycling delays (minimum 8-second on-delay and 5-second off-delay). If level falls below 8 percent while pump is commanded on, stop immediately, latch dry-run alarm, and inhibit automatic restart until reset. Manual override may force pump on, but dry-run protection remains active. Provide independent high-level alarm output at 90 percent and maintain deterministic behavior after power restoration.

## 6. Dual-Pump Lift Station with Alternation, Assist, and Hazard Interlocks

### Scenario
A wastewater lift station has two pumps (duty/standby), 4-20 mA wet-well level and/or pressure feedback, pump run/trip and thermal overload inputs, high-high/low-low level switches, gas hazard input, enclosure door status, ventilation fan, audible/visual alarms, and telemetry output. The station operates outdoors with flood risk, corrosive atmosphere, and unstable power, requiring high availability and fail-safe behavior.

### Required Functionality
Run one lead pump under normal load and automatically alternate lead assignment after each completed high-level event to balance wear. If inflow exceeds single-pump capacity (rising level/pressure trend), start the second pump as assist. On thermal overload or trip of one pump, transfer duty to the remaining healthy pump and alarm the fault. Enforce high-high overflow alarm and low-low protection. If hazardous gas alarm is active, force ventilation and block non-essential operations according to safety policy. Latch critical faults for remote telemetry and require controlled reset/recovery logic to prevent unsafe automatic restart.
