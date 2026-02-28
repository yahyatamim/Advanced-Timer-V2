# PoC Gap Log V2

Date: 2026-02-26
Purpose: Trace PoC behavior/docs into V2 artifacts and classify gaps.
Sources reviewed: `README.md`, `src/main.cpp`, `GEMINI.md`, `docs/legacy/production-firmware-kickoff-plan.md`, `docs/legacy/firmware-rewrite-foundations.md`.

## 1. Summary

- V2 core architecture and lifecycle direction are aligned with PoC foundations.
- Most gaps are in detailed protocol/schema semantics, not in core behavior.
- Two high-priority carry-forwards were patched immediately:
  - DO non-retriggerable mission rule in Normal/Immediate mode.
  - Explicit global output mask command class + acceptance coverage.

## 2. Gap Classification

| ID | Legacy Behavior (PoC) | V2 Status | Classification | Action |
| --- | --- | --- | --- | --- |
| GAP-001 | DO/SIO ignore new triggers while mission active unless idle/finished | Missing in V2 text | Carry forward | Patched in `requirements-v2-contract.md` and `AT-DO-007` |
| GAP-002 | Global output mask command (`set_output_mask_global`) exists | Implicit only | Carry forward | Patched command contract and `AT-API-005` |
| GAP-003 | Run mode includes `RUN_SLOW` | V2 removed dedicated slow mode | Intentionally dropped | Keep dropped; V2 uses configurable scan interval (`10..1000 ms`) |
| GAP-004 | PoC scan order is family order DI->AI->SIO->DO | V2 uses ascending `cardId` order | Intentionally changed | Keep V2 order; ensure migration docs call out behavior change |
| GAP-005 | Condition operator set in PoC includes state/trigger operators (`Op_Running`, `Op_Triggered`, etc.) | V2 now supports typed `STATE` comparisons for DO/SIO `missionState` (`IDLE|ACTIVE|FINISHED`) | Carry forward | Resolved in contract/schema/acceptance docs |
| GAP-006 | AI alpha in PoC stored as milliunits (`0..1000`) | V2 now freezes centiunit storage (`0..100` for `0.00..1.00`) | Carry forward | Resolved in contract/schema; migration transform required for legacy payloads |
| GAP-007 | Snapshot/API envelope fields in PoC are concrete (`schemaVersion`, runtime snapshot shape, error codes) | V2 now has a frozen API payload contract and stable error model | Carry forward | Resolved in `docs/api-contract-v2.md` and acceptance matrix |
| GAP-008 | Restore sources (`LKG`,`SLOT1..3`,`FACTORY`) concrete in PoC | V2 now freezes restore sources to `LKG` (single rollback slot) and `FACTORY` | Carry forward | Resolved in contract/schema/acceptance docs |
| GAP-009 | PoC had compatibility parser tolerance for enum token noise | Not specified in V2 | Intentionally dropped | No contract requirement unless field data proves needed |
| GAP-010 | PoC had AI set/reset placeholders in payload | V2 correctly marks AI set/reset unsupported | Resolved | No action needed |

## 3. Files Updated During This Audit

- `requirements-v2-contract.md`
  - Added explicit global output mask mention in command class.
  - Added explicit DO non-retriggerable rule for active mission windows.
- `docs/acceptance-matrix-v2.md`
  - Added `AT-DO-007` (active mission retrigger ignored).
  - Added `AT-API-005` (global output mask command behavior).

## 4. Recommended Next Actions

1. Begin implementation of API validators and transport adapters against frozen API/schema contracts.
2. Add CI tests for `AT-API-001..007` and `AT-CFG-006`.
