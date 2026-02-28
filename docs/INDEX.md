# Documentation Index

Date: 2026-02-28
Purpose: Canonical map of documentation for clean V3 transition.

## 1. Source Of Truth

- V2/V3 production rewrite contract: `requirements-v2-contract.md` (root)
- PoC frozen legacy contract: `README.md` (root)

## 2. Active V2 Docs (`docs/`)

- `docs/schema-v2.md` - Config schema and validation rules.
- `docs/api-contract-v2.md` - Transport/API envelope contract.
- `docs/acceptance-matrix-v2.md` - Acceptance test mapping.
- `docs/hardware-profile-v2.md` - Hardware profile/capability model.
- `docs/poc-gap-log-v2.md` - PoC-to-V2 gap traceability.
- `docs/decisions.md` - Decision log (accepted/superseded changes).
- `docs/worklog.md` - Consolidated session worklog.

## 3. Legacy Planning Archive (`docs/legacy/`)

These files are retained for historical/planning context and are not the active production contract:

- `docs/legacy/firmware-rewrite-foundations.md`
- `docs/legacy/production-firmware-kickoff-plan.md`
- `docs/legacy/plc-problem-statements.md`

## 4. Root Folder Policy

- Keep only canonical anchors at repo root: `README.md`, `requirements-v2-contract.md`, and tool-specific root docs (for example `GEMINI.md`).
- Place all evolving design/spec/test docs under `docs/`.
- Place historical/planning artifacts under `docs/legacy/`.
