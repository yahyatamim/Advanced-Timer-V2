# Schema V2

Date: 2026-02-26
Source Contract: `requirements-v2-contract.md` (v2.0.0-draft)
Related Tests: `docs/acceptance-matrix-v2.md`
Status: Draft for implementation

## 1. Purpose

This document defines the configuration payload schema for Advanced Timer V2.
It is the implementation-facing contract for staged config validation and commit.

## 2. Global Conventions

- All decimal-like numeric config values are stored as unsigned centiunits (`value_x100`).
- All numeric config values are non-negative.
- All IDs are unsigned integers.
- `cardId` is unique across all cards.
- Card evaluation order is by ascending `cardId`.

## 3. Top-Level Configuration Model

```json
{
  "schemaVersion": "2.0.0",
  "configId": "uuid-string",
  "createdAt": "2026-02-26T00:00:00Z",
  "scan": {
    "intervalMs": 10,
    "jitterBudgetUs": 500,
    "overrunBudgetUs": 1000
  },
  "cards": [],
  "bindings": [],
  "wifi": {
    "master": { "ssid": "factory-ssid", "password": "***", "timeoutSec": 3, "editable": false },
    "user": { "ssid": "user-ssid", "password": "***", "timeoutSec": 180 },
    "retryBackoffSec": 30,
    "staOnly": true
  }
}
```

## 4. Top-Level Field Definitions

- `schemaVersion`: required, semver string. Current: `2.0.0`.
- `configId`: required, unique immutable ID for this staged/committed payload.
- `createdAt`: required, RFC3339 UTC timestamp.
- `scan`: required object.
- `cards`: required non-empty array.
- `bindings`: optional array (default empty).
- `wifi`: required object.

### 4.1 `scan`

- `intervalMs`: required `uint32`, range `10..1000`.
- `jitterBudgetUs`: required `uint32`, implementation range.
- `overrunBudgetUs`: required `uint32`, implementation range.

### 4.2 `wifi`

- `master`: required object.
- `user`: required object.
- `retryBackoffSec`: required `uint32`.
- `staOnly`: required bool, must be `true`.

`master`:
- `ssid`: required string.
- `password`: required string.
- `timeoutSec`: required `uint32`, expected short timeout (2-3 sec).
- `editable`: required bool, must be `false`.

`user`:
- `ssid`: required string.
- `password`: required string.
- `timeoutSec`: required `uint32`, expected long timeout (about 180 sec).

## 5. Card Base Schema

Every card object includes base fields plus a typed `config` object.

```json
{
  "cardId": 10,
  "cardType": "DI",
  "enabled": true,
  "label": "Tank Level Switch",
  "faultPolicy": "WARN",
  "config": {}
}
```

Base fields:
- `cardId`: required `uint32`, unique.
- `cardType`: required enum: `DI|AI|SIO|DO|MATH|RTC`.
- `enabled`: required bool.
- `label`: required string, non-empty.
- `faultPolicy`: required enum: `INFO|WARN|CRITICAL`.
- `config`: required typed object by `cardType`.

## 6. Condition Block Schema (`set`/`reset`)

Used by `DI`, `DO`, `SIO`, and `MATH`.

```json
{
  "clauseA": {
    "source": { "cardId": 5, "field": "currentValue", "type": "NUMBER" },
    "operator": "GT",
    "threshold": 200
  },
  "clauseB": {
    "source": { "cardId": 6, "field": "logicalState", "type": "BOOL" },
    "operator": "EQ",
    "threshold": 1
  },
  "combiner": "AND"
}
```

- `combiner`: enum `NONE|AND|OR`.
- `clauseA`: required.
- `clauseB`: required only when `combiner != NONE`.
- Clause `operator` enum: `GT|GTE|LT|LTE|EQ|NEQ`.

## 7. Card-Type Schemas

## 7.1 DI

```json
{
  "channel": 0,
  "invert": false,
  "debounceTime": 50,
  "edgeMode": "RISING",
  "set": {},
  "reset": {},
  "counterVisible": true
}
```

- `channel`: required `uint32`.
- `invert`: required bool.
- `debounceTime`: required `uint32` (centiunit time).
- `edgeMode`: required enum `RISING|FALLING|CHANGE`.
- `set`: required condition block.
- `reset`: required condition block.
- `counterVisible`: required bool.

## 7.2 AI

```json
{
  "channel": 1,
  "engineeringUnit": "bar",
  "inputRange": { "min": 0, "max": 10000 },
  "clampRange": { "min": 0, "max": 10000 },
  "outputRange": { "min": 0, "max": 10000 },
  "emaAlpha": 100
}
```

- `channel`: required `uint32`.
- `engineeringUnit`: required string.
- `inputRange`, `clampRange`, `outputRange`: required min/max objects (`uint32`).
- `emaAlpha`: required `uint32`, range `0..100` (represents `0.00..1.00`).

## 7.3 SIO

```json
{
  "mode": "Normal",
  "delayBeforeON": 0,
  "onDuration": 500,
  "repeatCount": 1,
  "set": {},
  "reset": {},
  "writePolicy": {
    "allowedRoles": ["OPERATOR", "ENGINEER", "ADMIN"]
  }
}
```

- `mode`: required enum `Normal|Immediate|Gated`.
- `delayBeforeON`: required `uint32`.
- `onDuration`: required `uint32`.
- `repeatCount`: required `uint32`; `0` means infinite.
- `set`, `reset`: required condition blocks.
- `writePolicy`: required object.

## 7.4 DO

```json
{
  "channel": 2,
  "mode": "Normal",
  "delayBeforeON": 0,
  "onDuration": 500,
  "repeatCount": 1,
  "set": {},
  "reset": {}
}
```

- `channel`: required `uint32`.
- `mode`: required enum `Normal|Immediate|Gated`.
- `delayBeforeON`: required `uint32`.
- `onDuration`: required `uint32`.
- `repeatCount`: required `uint32`; `0` means infinite.
- `set`, `reset`: required condition blocks.

## 7.5 MATH

```json
{
  "mode": "Mode_Standard_Pipeline",
  "set": {},
  "reset": {},
  "fallbackValue": 0,
  "standard": {
    "inputA": { "sourceMode": "CONSTANT", "value": 100 },
    "inputB": { "sourceMode": "VARIABLE_REF", "ref": { "cardId": 2, "field": "currentValue", "type": "NUMBER" } },
    "operator": "ADD",
    "rateLimit": 0,
    "clampMin": 0,
    "clampMax": 10000,
    "scaleMin": 0,
    "scaleMax": 10000,
    "emaAlpha": 100
  },
  "pid": null
}
```

- `mode`: required enum `Mode_Standard_Pipeline|Mode_PID_Controller`.
- `set`, `reset`: required condition blocks.
- `fallbackValue`: required `uint32`.

`standard` mode fields:
- `inputA`, `inputB`: required source descriptors.
- `operator`: required enum `ADD|SUB|MUL|DIV|MOD|POW|MIN|MAX`.
- `rateLimit`: required `uint32`; `0` disables stage.
- `clampMin`, `clampMax`: required `uint32`.
- `scaleMin`, `scaleMax`: required `uint32`.
- `emaAlpha`: required `uint32`, range `0..100`; `100` disables EMA stage.

`pid` mode fields:
- `processVariable`: required `VARIABLE_REF`.
- `setpoint`: required `CONSTANT` or `VARIABLE_REF`.
- `gainP`, `gainI`, `gainD`: required `uint32` (centiunits).
- `outputMin`, `outputMax`: required `uint32`.
- `integralResetPolicy`: required enum `ON_RESET|ON_SIGN_CHANGE|NEVER`.

## 7.6 RTC

```json
{
  "schedule": {
    "year": 2026,
    "month": 0,
    "day": 0,
    "hour": 14,
    "minute": 30,
    "second": 0,
    "weekday": 0
  },
  "triggerDuration": 6000
}
```

- `schedule`: required object.
- `triggerDuration`: required `uint32` (centiunit time).

`schedule` fields:
- `year`: optional `uint32` (if omitted, wildcard all years).
- `month`: optional `uint32` (if omitted, wildcard all months).
- `day`: optional `uint32` (if omitted, wildcard all days).
- `hour`: required `uint32`.
- `minute`: required `uint32`.
- `second`: required `uint32`.
- `weekday`: optional `uint32` (if omitted, wildcard all weekdays).

Wildcard encoding:
- Optional fields may be omitted.
- If present, they must be valid values (see validation rules).

## 8. Binding Schema

Top-level `bindings` allows typed parameter binding.

```json
{
  "bindingId": "B-101",
  "target": { "cardId": 12, "path": "config.standard.inputA" },
  "source": {
    "sourceMode": "VARIABLE_REF",
    "ref": { "cardId": 3, "field": "currentValue", "type": "NUMBER" }
  }
}
```

- `bindingId`: required unique string.
- `target`: required (`cardId` + parameter `path`).
- `source.sourceMode`: required enum `CONSTANT|VARIABLE_REF`.
- `source.ref`: required when `VARIABLE_REF`.
- Compatibility checks must pass type/range/unit/topology validation.

## 9. Cross-Field Validation Rules

- V-CFG-001: reject unknown `schemaVersion`.
- V-CFG-002: reject duplicate `cardId`.
- V-CFG-003: reject card references to missing card/field.
- V-CFG-004: reject any negative numeric value.
- V-CFG-005: reject `scan.intervalMs` outside `10..1000`.
- V-CFG-006: reject `combiner=NONE` with non-empty `clauseB` if policy requires strict form.
- V-CFG-007: reject missing `clauseB` for `AND|OR`.
- V-CFG-008: reject AI set/reset fields (unsupported).
- V-CFG-009: reject RTC set/reset fields (unsupported).
- V-CFG-010: reject MATH operators outside arithmetic enum.
- V-CFG-011: reject `clampMin > clampMax` unless explicit bypass policy is allowed.
- V-CFG-012: reject type/range/unit-incompatible bindings.
- V-CFG-013: reject dependency cycles.
- V-CFG-014: reject non-owner write-binding attempts.
- V-CFG-015: reject `wifi.staOnly=false`.

## 10. Open Decisions To Freeze

- D-SCH-001: RTC value ranges are not yet frozen in contract text.
  - Proposed: `month 1..12`, `day 1..31`, `hour 0..23`, `minute 0..59`, `second 0..59`, `weekday 1..7`.
- D-SCH-002: RTC retrigger behavior during active `triggerDuration` is not frozen.
  - Options: `IGNORE_WHILE_ACTIVE`, `RESTART_WINDOW`, `EXTEND_WINDOW`.
- D-MATH-001: Exact overflow/domain behavior for `POW`, `DIV`, and `MOD` should be fixed as deterministic policy.

## 11. Migration Notes

- Any prior RTC recurrence/holiday policy structures are removed in V2.
- Migration to V2 must transform old RTC schedule representation to field-based schedule + `triggerDuration`.
- Any MATH comparison operators in legacy configs must fail validation or be migrated by explicit rule.

## 12. Next Implementation Artifacts

- `docs/dependency-topology-rules.md`
- `docs/api-contract-v2.md`
- `docs/timing-budget-v2.md`
- Validation error catalog in code aligned with rule IDs (`V-CFG-*`).
