# PoC Gap Log V2

Date: 2026-02-26
Purpose: Trace PoC behavior/docs into V2 artifacts and classify gaps.
Sources reviewed: `README.md`, `src/main.cpp`, `GEMINI.md`, `production-firmware-kickoff-plan.md`, `firmware-rewrite-foundations.md`.

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
| GAP-005 | Condition operator set in PoC includes state/trigger operators (`Op_Running`, `Op_Triggered`, etc.) | V2 condition model does not freeze full operator catalog | Defer | Finalize allowed set in `docs/dependency-topology-rules.md` + tests |
| GAP-006 | AI alpha in PoC stored as milliunits (`0..1000`) | V2 schema draft uses centiunits (`0..100`) | Defer | Freeze representation rule and add migration transform note |
| GAP-007 | Snapshot/API envelope fields in PoC are concrete (`schemaVersion`, runtime snapshot shape, error codes) | V2 API contract is high-level only | Defer | Finalize in `docs/api-contract-v2.md` |
| GAP-008 | Restore sources (`LKG`,`SLOT1..3`,`FACTORY`) concrete in PoC | V2 requires rollback slots but not source enum shape | Defer | Freeze restore-source payload enum in API contract |
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

1. Freeze deferred semantic gaps (`GAP-005` to `GAP-008`) in writing before coding API/validator.
2. Update `docs/schema-v2.md` once AI-alpha storage representation is finalized.
3. Generate `docs/api-contract-v2.md` to lock snapshot/command/error payload structure.
