# Hardware Profile Contract V2

Date: 2026-02-28
Source Contract: `requirements-v2-contract.md` (v2.0.0-draft)
Status: Draft for implementation

## 1. Purpose

Define build-time hardware capabilities in one place so firmware behavior, schema validation, and exposed card families are deterministic per build target.

## 2. Core Model

A build target must define a **Hardware Profile** with:

- IO channel availability (pin arrays or backend channel arrays)
- Explicit compile-time family capacities
- Feature gates (for example RTC presence)
- Backend selections per IO family

If a profile array is empty, that IO family has zero available channels in that build.

## 3. Required Profile Fields

Minimum required fields:

- `profileName` (string)
- `platformVariant` (string, for example `ESP32`, `ESP32-S3`, `ESP32-P4+C6`)
- `diCapacity` (uint32)
- `doCapacity` (uint32)
- `aiCapacity` (uint32)
- `sioCapacity` (uint32)
- `mathCapacity` (uint32)
- `rtcAlarmCapacity` (uint32)
- `diChannels[]` (array)
- `doChannels[]` (array)
- `aiChannels[]` (array)
- `hasRtc` (bool)
- `enableCardDi|Do|Ai|Sio|Rtc|Math` (bool gates)
- `aiBackend` (`INTERNAL_ADC|I2C_ADC|PLUGIN`)
- `ioRemoteBackend` (`NONE|PLUGIN`)

Rules:

- Every family is optional by profile (`DI`, `DO`, `AI`, `SIO`, `MATH`, `RTC`).
- Firmware contracts must not assume one fixed MCU/board family.
- Profiles may represent single-MCU or split-MCU designs, as long as deterministic contract timing and API/schema behavior are preserved.
- Family capacities are fixed at compile time for each build target.
- Valid capacity range is `0..N` per family, where `N` is any value supported by target hardware and product design.
- If `diChannels[]` is empty, `DI` cards are unsupported in this build.
- If `doChannels[]` is empty, `DO` cards are unsupported in this build.
- If `aiChannels[]` is empty, `AI` cards are unsupported in this build.
- `SIO` and `MATH` are virtual families but still use compile-time capacities and can be `0`.
- `RTC` functionality is schedule-based alarm functionality; `rtcAlarmCapacity` defines alarm channel count in range `0..N` for that build.
- `RTC` cards are optional and require `hasRtc=true` and `enableCardRtc=true` when enabled.
- If family capacity is `0`, config validation must reject that family.

## 4. Validation And Runtime Gates

Validation must enforce hardware-profile constraints:

- Reject config containing card types disabled by profile gates.
- Reject channel bindings that exceed available profile channel arrays.
- Reject `RTC` cards when RTC gate is disabled.
- Reject per-family card counts that exceed compile-time capacities.
- Return stable validation error code: `VALIDATION_FAILED` with field path.

Runtime contract:

- Unsupported families must not be instantiated.
- API snapshots must only include cards from enabled families.
- Family count in runtime must never exceed active profile capacity.

## 5. Hardware-Agnostic IO Backend Contract

IO card logic must be backend-agnostic:

- Card logic reads/writes via adapter interfaces, not direct driver calls.
- Backends can be swapped without changing card semantics.

Example backend classes:

- `DI`: GPIO direct, expander-backed, plugin-backed.
- `AI`: internal ADC, I2C ADC, plugin-backed remote provider.
- `DO`: GPIO relay driver, expander-backed, plugin-backed.

## 6. Modbus And Plugin Boundary

Modbus is intentionally excluded from core deterministic firmware in current V2 scope.

Rules:

- No mandatory Modbus dependency in kernel or core card logic.
- If remote IO is added later, it must be through installable plugin adapters.
- Plugin adapters must implement bounded-time read/write behavior and explicit failure modes.

## 7. PlatformIO Mapping

A profile may be represented by build flags (or a generated header) such as:

- `AT_HAS_RTC`
- `AT_ENABLE_CARD_RTC`
- `AT_ENABLE_CARD_DI|DO|AI|SIO|MATH`
- `AT_AI_BACKEND_INTERNAL_ADC|I2C_ADC|PLUGIN`
- `AT_DI_GPIO_LIST`, `AT_DO_GPIO_LIST`, `AT_AI_GPIO_LIST`

Current rollout policy:

- Keep `platformio.ini` clean for now.
- Final flag names and mapping strategy are intentionally deferred to future design decisions.
- This contract only fixes behavior and constraints, not the final build-flag syntax.

## 8. Product-Line Direction

Advanced Timer must remain hardware-agnostic so multiple hardware models can be supported with one deterministic logic contract.

Rules:

- Different product models may expose different family capacities and optional features.
- Hardware differences must be represented by profiles/adapters, not by changing card semantics.
- API/schema behavior remains stable while capabilities vary by profile.
- Platform variants may include ESP32-class derivatives and multi-chip combinations; profile adapters isolate these differences from card logic.

Recommended implementation path:

1. Build flags select profile.
2. Profile header materializes typed channel arrays and gates.
3. Validator and card factory consume this profile as single source of truth.
