# Worklog

Status: Canonical consolidated session log for V2 documentation and implementation tracking.

## 2026-02-26

### Session Summary

Today we converted planning into implementation-grade documentation for V2.

### Completed

- Updated `requirements-v2-contract.md` with final decisions:
  - MATH uses arithmetic operators only (no comparison operators).
  - RTC switched to field-based schedule (`year/month/day/hour/minute/second/weekday`) with wildcard semantics.
  - RTC uses `triggerDuration` for asserted output window.
  - DO/SIO state comparison model finalized:
    - only `missionState` for DO/SIO
    - states: `IDLE | ACTIVE | FINISHED`
    - STATE comparison operator: `EQ` only
    - non-match => `false`
  - Restore model simplified to single rollback + factory:
    - `LKG` (last known good)
    - `FACTORY`

- Created/updated docs:
  - `docs/schema-v2.md` (schema draft aligned with above decisions)
  - `docs/acceptance-matrix-v2.md` (acceptance coverage expanded)
  - `docs/poc-gap-log-v2.md` (PoC->V2 gap tracking updated)
  - `docs/api-contract-v2.md` (new, payload-level API freeze draft)

- Gap status:
  - GAP-005 resolved (STATE condition model)
  - GAP-006 resolved (`emaAlpha` frozen to centiunits `0..100` for `0.00..1.00`)
  - GAP-008 resolved (restore sources frozen to `LKG|FACTORY`)
  - GAP-007 drafted as resolved via `docs/api-contract-v2.md`

### Current Blocker

- Unresolved merge markers exist in:
  - `docs/acceptance-matrix-v2.md` (lines around 112-116)
- This must be cleaned first before further edits.

### Exact Start Plan For Next Session

1. Resolve merge markers in `docs/acceptance-matrix-v2.md`.
2. Re-run consistency pass across:
   - `requirements-v2-contract.md`
   - `docs/schema-v2.md`
   - `docs/api-contract-v2.md`
   - `docs/poc-gap-log-v2.md`
   - `docs/acceptance-matrix-v2.md`
3. Confirm final gap table status (all intended gaps closed, or explicitly deferred).
4. Start implementation phase:
   - validator rules (`V-CFG-*`)
   - API envelope validation and error mapping
   - acceptance test scaffolding for `AT-API-*` and `AT-CFG-006`

### Notes

- Keep `RUN_SLOW` rejected in V2 API/command validation.
- Keep restore source validation strict: only `LKG` and `FACTORY`.
- Keep STATE comparison strict: DO/SIO only, `EQ` only.

## 2026-02-28

### Session Summary

Established a minimal "vibe-safe" documentation workflow and aligned V2 docs with the decision to keep `lastEvalUs` as runtime observability metadata.

### Completed

- Added `docs/decisions.md`:
  - Minimal decision-entry template.
  - Workflow rules for contract-first changes.
  - `DEC-0001` documenting `lastEvalUs` as per-card evaluation timing metric.

- Updated `docs/api-contract-v2.md`:
  - Added `docs/decisions.md` in related docs.
  - Clarified `lastEvalUs` semantics:
    - microsecond unit
    - non-negative integer
    - runtime-only (not required in config payloads)

- Updated process docs:
  - `README.md` Section 18.4 now requires decision-log updates for behavior/API/validation changes.
  - `requirements-v2-contract.md` now includes decision-log requirement and lists `docs/decisions.md` as a required artifact.

- Cleaned unresolved merge markers in `docs/acceptance-matrix-v2.md`.

### Next Session Starting Point

1. Add validator tests that assert `lastEvalUs` handling as runtime observability metadata.
2. Ensure portal/runtime UI rendering treats `lastEvalUs` as optional forward-compatible telemetry.

## 2026-02-28 (Hardware Profile Addendum)

### Session Summary

Added V2 hardware-profile gating contract so build targets can define available IO families/channels and optional capabilities (for example RTC) without changing card logic semantics.

### Completed

- Updated `platformio.ini` with hardware-profile build flag scaffold:
  - profile name
  - per-family card gates
  - RTC gate
  - AI backend selection
  - optional IO pin list macros

- Added `docs/hardware-profile-v2.md`:
  - build-time hardware profile model
  - empty-array semantics for unavailable IO families
  - RTC gating rules
  - backend abstraction contract
  - plugin boundary for remote/protocol-specific IO

- Updated `requirements-v2-contract.md`:
  - Added Section `6.4 Build-Time Hardware Profile Contract`
  - Added `docs/hardware-profile-v2.md` to required artifacts

- Updated `docs/schema-v2.md`:
  - Linked hardware profile contract
  - Added validation IDs `V-CFG-017..019` for profile gates/capacity/RTC support

- Updated `docs/acceptance-matrix-v2.md`:
  - Added hardware-profile acceptance tests `AT-HW-001..004`

## 2026-02-28 (Capacity Model Clarification)

### Session Summary

Finalized the family-capacity model: every card family (physical and virtual) is compile-time capacity-bound by hardware profile, including RTC as schedule-alarm channels.

### Completed

- Updated `docs/hardware-profile-v2.md`:
  - Added explicit compile-time capacities per family.
  - Clarified RTC as schedule-based alarm channel capacity (`0..N` by profile/hardware design).
  - Clarified that all families can be `0` and are profile-optional.

- Updated `requirements-v2-contract.md`:
  - Added compile-time capacity language in hardware profile requirements.
  - Clarified RTC section with alarm-channel capacity gating.

- Updated `docs/schema-v2.md`:
  - Added compile-time family-capacity statement for all families.

- Updated `docs/acceptance-matrix-v2.md`:
  - Added `AT-HW-007` for RTC alarm-capacity enforcement.

- Updated `docs/decisions.md`:
  - Added `DEC-0002` to record this capacity-model decision.
