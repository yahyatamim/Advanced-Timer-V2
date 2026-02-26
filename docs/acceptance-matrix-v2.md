# Acceptance Matrix V2

Date: 2026-02-26
Source Contract: `requirements-v2-contract.md` (v2.0.0-draft)
Status: Draft for implementation kickoff

## 1. Scope And Gate

- This matrix maps contract requirements to executable acceptance tests.
- Release gate: all tests marked `Gate=Yes` must pass in CI, and all tests marked `HIL=Yes` must pass on hardware-in-loop smoke.

## 2. Test Matrix

| Test ID | Contract Ref | Scenario | Expected Result | CI | HIL | Gate |
| --- | --- | --- | --- | --- | --- | --- |
| AT-CORE-001 | Sec 5, 6.2 | Kernel attempts direct portal/filesystem call | Build/lint rule rejects forbidden coupling | Yes | No | Yes |
| AT-CORE-002 | Sec 6.1 | Run at configured scan interval 10 ms | Mean interval at target, jitter/overrun within configured budget | Yes | Yes | Yes |
| AT-CORE-003 | Sec 6.1 | Run at configured scan interval 1000 ms | Stable scan cadence with bounded jitter and no drift spikes | Yes | Yes | Yes |
| AT-CORE-004 | Sec 6.2 | Card evaluation order with mixed IDs | Execution order strictly ascending cardId | Yes | No | Yes |
| AT-CORE-005 | Sec 6.2 | Snapshot generation per scan | Exactly one immutable snapshot revision per completed scan | Yes | No | Yes |
| AT-CORE-006 | Sec 6.2 | Commit topology rebuild disabled during run | Topology unchanged during scans until next commit | Yes | No | Yes |
| AT-CORE-007 | Sec 6.3 | RUN_STEP mode | Exactly one card-eval step per step command | Yes | Yes | Yes |
| AT-CORE-008 | Sec 6.3 | RUN_BREAKPOINT mode | Execution halts at configured boundary; resumes on continue | Yes | Yes | Yes |
| AT-DATA-001 | Sec 4.1 | Centiunit encode/decode timing threshold values | Stored/transmitted values are integer x100 equivalents | Yes | No | Yes |
| AT-DATA-002 | Sec 4.2 | Negative numeric field in staged config | Validation rejects payload with machine-readable error | Yes | No | Yes |
| AT-DATA-003 | Sec 4.1, 4.2 | Portal decimal entry and firmware integer handling | Portal converts; firmware receives unsigned integer form only | Yes | No | Yes |
| AT-STATE-001 | Sec 5.1-5.3 | Non-owner tries to write another card currentValue | Validation rejects write-binding with ownership violation | Yes | No | Yes |
| AT-STATE-002 | Sec 5.2 | Owner card updates own runtime variable | Update accepted and visible to readers in next snapshot | Yes | No | Yes |
| AT-DI-001 | Sec 8.1 DI | Debounce qualification before logic update | logicalState updates only after stable debounceTime | Yes | Yes | Yes |
| AT-DI-002 | Sec 8.1 DI | Edge mode RISING/FALLING/CHANGE counter behavior | Counter increments only for configured qualified edge type | Yes | Yes | Yes |
| AT-DI-003 | Sec 8.3, 8.1 DI | set=false, reset=false on input transitions | DI blocks logical updates and counter increments | Yes | Yes | Yes |
| AT-DI-004 | Sec 8.3, 8.1 DI | reset=true while set=true and input changes | Reset dominates: counter reset and updates inhibited | Yes | Yes | Yes |
| AT-DI-005 | Sec 8.1 DI | invert flag processing order | Invert applied before debounce and edge qualification | Yes | Yes | Yes |
| AT-DI-006 | Sec 8.1 DI | Force mode transitions REAL/FORCED_HIGH/FORCED_LOW | Force changes do not corrupt edge counter integrity | Yes | Yes | Yes |
| AT-AI-001 | Sec 8.2 AI | Normal pipeline raw->clamp->map/scale->EMA | Output matches exact stage order and expected value | Yes | Yes | Yes |
| AT-AI-002 | Sec 8.2 AI | emaAlpha=1.0 | EMA effectively bypassed (no smoothing lag) | Yes | No | No |
| AT-AI-003 | Sec 8.2 AI | Out-of-range raw sample | Clamp and quality flag become `CLAMPED` | Yes | Yes | Yes |
| AT-AI-004 | Sec 8.2 AI | Invalid forced value command | Command rejected by validation | Yes | No | Yes |
| AT-AI-005 | Sec 8.2 AI, 8.2 matrix | Apply set/reset blocks to AI | AI ignores set/reset semantics by design | Yes | No | Yes |
| AT-DO-001 | Sec 8.4 DO | Normal mode set rising edge mission start | Mission latches and runs timer phases to completion | Yes | Yes | Yes |
| AT-DO-002 | Sec 8.4 DO | Immediate mode timing start behavior | Active phase starts per Immediate mode contract | Yes | Yes | Yes |
| AT-DO-003 | Sec 8.4 DO, 8.3 | set and reset true same eval | Reset precedence mandatory; mission aborted/idle | Yes | Yes | Yes |
| AT-DO-004 | Sec 8.4 DO | Mask mode `MASKED` during active mission | Logical mission continues, physical drive suppressed | Yes | Yes | Yes |
| AT-DO-005 | Sec 8.4 DO | repeatCount finite and zero (infinite) | Finite stops at count; zero continues until reset | Yes | Yes | Yes |
| AT-DO-006 | Sec 8.4 DO | Integrated timer/counter presence | No external timer/counter dependency required | Yes | No | Yes |
| AT-DO-007 | Sec 8.4 DO | New set trigger while mission already `OnDelay`/`Active` | Retrigger ignored until `Idle` or `Finished` | Yes | Yes | Yes |
| AT-SIO-001 | Sec 8.3 SIO | SIO mission timing parity with DO | Phase and counter behavior matches DO semantics | Yes | Yes | Yes |
| AT-SIO-002 | Sec 8.3 SIO | SIO hardware drive output | No GPIO/relay drive attempts occur | Yes | Yes | Yes |
| AT-SIO-003 | Sec 8.3 SIO | Mask command against SIO | Mask semantics unavailable/rejected for SIO | Yes | No | Yes |
| AT-SIO-004 | Sec 8.3 SIO | Unauthorized write attempt | Write denied and audit record emitted | Yes | No | Yes |
| AT-MATH-001 | Sec 8.5.3 | reset=true regardless of set | currentValue forced to fallbackValue; no further processing | Yes | No | Yes |
| AT-MATH-002 | Sec 8.5.3 | set=false, reset=false | currentValue holds last value (HOLDING) | Yes | No | Yes |
| AT-MATH-003 | Sec 8.5.1.1 | Standard operator correctness: ADD,SUB,MUL,DIV,MOD,POW,MIN,MAX | Arithmetic stage result correct for each operator | Yes | No | Yes |
| AT-MATH-004 | Sec 8.5.1.1 | Non-arithmetic operator configured | Validation rejects unsupported comparison/logical operators | Yes | No | Yes |
| AT-MATH-005 | Sec 8.5.3 | Standard pipeline execution order | RateLimit->Clamp->Scale->EMA order preserved | Yes | No | Yes |
| AT-MATH-006 | Sec 8.5.3 | Inert parameters disable stage behavior | Disabled stage leaves value unchanged as specified | Yes | No | Yes |
| AT-MATH-007 | Sec 8.5.3 | Divide-by-zero fault | fallbackValue emitted and fault flag set in scan | Yes | No | Yes |
| AT-MATH-008 | Sec 8.5.1.2, 8.5.3 | PID mode with output limits | Output obeys limits and anti-windup behavior | Yes | No | Yes |
| AT-MATH-009 | Sec 8.5.1.2 | PID reset behavior | Integral term cleared per reset/integralResetPolicy | Yes | No | Yes |
| AT-RTC-001 | Sec 8.6.3 | RTC evaluates against system clock service | logicalState follows authoritative wall-clock only | Yes | Yes | Yes |
| AT-RTC-002 | Sec 8.6.1, 8.6.2 | Exact timestamp schedule match | Match starts active window and logicalState transitions as expected | Yes | Yes | Yes |
| AT-RTC-003 | Sec 8.6.1, 8.6.3 | Field-based match with optional month/weekday omitted | Omitted fields act as wildcards; schedule triggers correctly | Yes | Yes | Yes |
| AT-RTC-004 | Sec 8.6.1, 8.6.3 | Trigger duration timing | logicalState stays true for `triggerDuration` then deasserts | Yes | Yes | Yes |
| AT-RTC-005 | Sec 8.6.3 | Attempt set/reset on RTC | RTC rejects/ignores set-reset as unsupported | Yes | No | Yes |
| AT-BIND-001 | Sec 9.1-9.2 | CONSTANT and VARIABLE_REF source binding | Both bind modes accepted when type-compatible | Yes | No | Yes |
| AT-BIND-002 | Sec 9.2 | Type mismatch in binding | Validation fails with structured error | Yes | No | Yes |
| AT-BIND-003 | Sec 9.2 | Range/unit mismatch in numeric binding | Validation fails; active config unchanged | Yes | No | Yes |
| AT-BIND-004 | Sec 9.2, 10 | Forward-cycle dependency via binding | Topology cycle detected; commit blocked | Yes | No | Yes |
| AT-BIND-005 | Sec 9.3 | Invalid binding among valid edits | Whole commit transaction aborted atomically | Yes | No | Yes |
| AT-CFG-001 | Sec 11.2 | Missing schemaVersion in payload | Validation rejects with explicit code/message | Yes | No | Yes |
| AT-CFG-002 | Sec 11.2 | Legacy schema migration to current | Migration deterministic and auditable | Yes | No | Yes |
| AT-CFG-003 | Sec 11.3, 12.2 | Full config lifecycle state flow | ACTIVE->STAGED->VALIDATED->COMMITTED transitions valid | Yes | No | Yes |
| AT-CFG-004 | Sec 12.2 | Failure at each commit protocol step | Active remains previous valid config; error surfaced | Yes | Yes | Yes |
| AT-CFG-005 | Sec 12.1, 12.2 | Rollback slot rotation and LKG preservation | Minimum rollback slots maintained with correct ordering | Yes | Yes | Yes |
| AT-WIFI-001 | Sec 13.1, 13.2 | Boot connection sequence | Master SSID short timeout then User SSID long timeout | Yes | Yes | Yes |
| AT-WIFI-002 | Sec 13.2 | Both SSIDs unavailable | Device enters offline mode, kernel remains operational | Yes | Yes | Yes |
| AT-WIFI-003 | Sec 13.3 | WiFi mode verification | STA-only operation; AP mode disabled | Yes | Yes | Yes |
| AT-WIFI-004 | Sec 13.3 | Heavy WiFi traffic under scan load | Core0 scan timing budget remains within limits | Yes | Yes | Yes |
| AT-UI-001 | Sec 14.1 | Mobile viewport runtime header persistence | Fixed header always visible with required status fields | Yes | No | No |
| AT-UI-002 | Sec 14.2 | Card order rendering | UI order matches firmware deterministic scan order | Yes | No | Yes |
| AT-UI-003 | Sec 14.2 | Portal state derivation check | UI uses snapshot authoritative states only | Yes | No | Yes |
| AT-UI-004 | Sec 14.2 | Local staged edits vs active config visibility | User cannot confuse staged and active configurations | Yes | No | Yes |
| AT-API-001 | Sec 15.2, 15.3 | Mutating command success/failure payload shape | requestId,timestamp,status fields always present | Yes | No | Yes |
| AT-API-002 | Sec 15.2 | Validation failure payload | machine-readable errorCode + human message returned | Yes | No | Yes |
| AT-API-003 | Sec 15.2 | Snapshot API behavior | Always returns latest complete snapshot revision | Yes | No | Yes |
| AT-API-004 | Sec 15.2 | WebSocket revision ordering under burst events | Event stream preserves increasing revision sequence | Yes | Yes | Yes |
| AT-API-005 | Sec 15.1 | Global output mask command path | Command applies global mask state and reports acknowledged status | Yes | Yes | Yes |
| AT-SEC-001 | Sec 16.1, 16.2 | Role-based access matrix for protected ops | VIEWER/OPERATOR/ENGINEER/ADMIN permissions enforced | Yes | No | Yes |
| AT-SEC-002 | Sec 16.3 | Successful protected operation audit | Audit record contains all mandatory fields | Yes | No | Yes |
| AT-SEC-003 | Sec 16.3 | Failed protected operation audit | Failure recorded with actor/role/action/result | Yes | No | Yes |
| AT-REL-001 | Sec 17.1, 17.2 | Runtime fault severity policy application | INFO/WARN/CRITICAL behavior matches policy | Yes | Yes | Yes |
| AT-REL-002 | Sec 17.2 | Critical unsafe output fault path | Unsafe outputs forced to safe state; fault latched | Yes | Yes | Yes |
| AT-REL-003 | Sec 18 | Reboot/fault counters persistence across reboot | Required counters and reboot reason retained | Yes | Yes | Yes |
| AT-REL-004 | Sec 19 | Scan path allocation policy | No dynamic heap allocation in kernel per-scan path | Yes | No | Yes |
| AT-REL-005 | Sec 19 | Snapshot generation boundedness | Snapshot generation does not block deterministic scan | Yes | Yes | Yes |

## 3. Clarification-Driven Tests (TBD Details)

These tests are defined as acceptance placeholders and should be finalized when syntax-level specs are frozen:

- `AT-MATH-010` (TBD): Full arithmetic operator edge-case tables (overflow, precision, domain rules for `POW`/`MOD`).
- `AT-RTC-006` (TBD): Exact RTC trigger semantics for repeated matches within an active `triggerDuration` window.

## 4. Coverage Check

- Required suite prefixes from contract are present: `AT-CORE`, `AT-DI/AI/SIO/DO`, `AT-MATH`, `AT-RTC`, `AT-BIND`, `AT-CFG`, `AT-SEC`, `AT-REL`.
- Every major contract section (4 through 20) has at least one mapped acceptance test.
