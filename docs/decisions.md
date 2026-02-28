# Decision Log

Date: 2026-02-28
Purpose: Minimal "vibe-safe" change log for decisions that affect behavior, contracts, or validation.
Status: Active

## 1. When To Add An Entry

Add an entry when any of the following changes:

- Runtime behavior or deterministic semantics.
- API/schema fields or payload meaning.
- Validation rules and accepted/rejected values.
- Safety, rollback, or fault-handling policy.

## 2. Entry Format (Keep It Small)

Use one short entry per decision with this structure:

```md
## DEC-XXXX: Short Title
- Date: YYYY-MM-DD
- Status: Proposed | Accepted | Superseded
- Context: one paragraph
- Decision: one paragraph
- Impact: 2-5 bullets (behavior, tests, migration, docs)
- References: list of touched files/sections
```

## 3. Workflow Rules

- Contract-first: update docs before or with implementation.
- One decision per intent: avoid bundling unrelated choices.
- Link every behavior change to at least one decision ID.
- If a decision is replaced, keep history and mark old one `Superseded`.

## 4. Decisions

## DEC-0001: Expose Per-Card Evaluation Timing In Runtime Snapshot
- Date: 2026-02-28
- Status: Accepted
- Context: Runtime snapshots already include `lastEvalUs` in examples, but the field intent was not explicitly documented.
- Decision: Keep `lastEvalUs` as a standard per-card runtime snapshot field to expose card evaluation duration in microseconds.
- Impact: Supports deterministic observability and timing regression detection; enables tooling/UI to detect outlier cards without recomputing runtime internals; keeps this field runtime-only and out of config payload requirements.
- References: `docs/api-contract-v2.md` (Section 5.1 rules), `requirements-v2-contract.md` (artifact set and change-control linkage), `README.md` (working method linkage).
<<<<<<< HEAD

## DEC-0002: Compile-Time Family Capacities Across All Card Types
- Date: 2026-02-28
- Status: Accepted
- Context: Product models will vary by available channels and optional capabilities; all card families (physical and virtual) must follow one consistent profile-capacity model.
- Decision: Define explicit compile-time capacities for every family (`DI`, `DO`, `AI`, `SIO`, `MATH`, `RTC` alarm channels), allowing `0..N` instances per family by active hardware profile.
- Impact: Enables deterministic multi-model product line support; removes assumptions that any family must exist; formalizes RTC as schedule-alarm channel capacity rather than a special-case family.
- References: `docs/hardware-profile-v2.md`, `requirements-v2-contract.md` (Sections 6.4, 7.1, 8.6), `docs/schema-v2.md` (family presence/capacity), `docs/acceptance-matrix-v2.md` (`AT-HW-005..007`).
=======
>>>>>>> 549b1e14679d699bc6e73313db0fe34b58c71af1
