Stepping & Live Monitoring Architecture
Overview

The Configuration Portal includes a Card-Level Stepping and Live Monitoring System designed to provide deterministic, visual debugging of the LogicCard engine without introducing time virtualization or alternate execution semantics.

This functionality enables safe inspection of decision flow while preserving the integrity of the production kernel.

Stepping is an execution control feature — not a simulation engine.

Design Philosophy

The stepping system follows these principles:

Card-Level Atomic Evaluation

Each LogicCard is evaluated as a complete unit.

No partial card execution is exposed.

No subsystem-level stepping is supported.

Deterministic Order

Cards are evaluated in a fixed, deterministic sequence.

Evaluation order must remain identical in normal and debug modes.

Single Engine Architecture

Stepping does not use a separate simulation engine.

The same runtime kernel is used in both normal and debug modes.

Only scan progression behavior changes.

No Time Virtualization

The system does not implement a virtual clock.

Timer behavior decisions are deferred and may be defined separately.

Stepping does not alter internal timekeeping architecture.

Execution Modes

The engine supports multiple execution modes:

RUN_NORMAL
Full scan executes continuously at fixed scan interval (e.g., 10 ms).

RUN_STEP
One LogicCard is evaluated per user trigger.

RUN_BREAKPOINT
Execution pauses automatically after a specified LogicCard index completes.

RUN_SLOW (optional future mode)
Sequential card evaluation with controlled delay between cards.

All modes use the same evaluation logic and kernel implementation.

Card-Level Stepping Behavior

In Step Mode:

The engine evaluates exactly one LogicCard.

All internal decisions of that card complete fully.

Output states and internal state transitions are committed.

The engine pauses.

The UI updates to reflect the new system state.

The next step command advances to the next card in sequence.

No other cards are evaluated during that step.

Breakpoint Behavior

Breakpoints operate at the LogicCard level.

If a breakpoint is configured on Card N:

The engine executes normally until Card N completes.

After completion, the engine enters paused state.

The system state at that moment is exposed to the UI.

Breakpoints do not interrupt partial card execution.

Live Monitoring Model

The portal exposes real-time state during:

Normal runtime

Step mode

Breakpoint pauses

For each LogicCard, the following state may be visible:

Current input condition values

Decision result (true/false)

Output state

Latch state (if applicable)

Timer state (if applicable)

Inhibition or dominant override status

State streaming must be read-only and must not interfere with execution timing.

Safety Model (Recommended)

When stepping or breakpoint mode is active:

The system should automatically enable Dry Run Mode.

Physical outputs must not energize.

Logical output states remain fully visible.

This prevents unintended actuation during debugging.

Kernel Requirements

To support stepping functionality, the kernel must:

Represent LogicCards as independently evaluable units.

Allow evaluation of a single card by index.

Maintain an internal currentCardIndex.

Expose engine state (running / paused).

Preserve identical logic semantics across all modes.

Guarantee that card evaluation is atomic and deterministic.

Non-Goals

The stepping system does NOT:

Provide time rewind capability.

Provide state rollback.

Implement alternate timing engines.

Alter logical semantics between debug and normal modes.

Support sub-card or subsystem granularity stepping.

Future Extensions (Optional)

The architecture allows future addition of:

Visual dependency tracing

"Why" decision introspection tooltips

Execution trace logging

Per-card execution timing metrics

Logic signature preview overlays

These features must not require modification of core stepping architecture.

Summary

The stepping system is a deterministic, card-level execution control mechanism that enhances debugging visibility without compromising runtime integrity or introducing simulation complexity.

It preserves:

Single kernel architecture

Fixed evaluation order

Industrial determinism

Safety-first design


Simulation & Test Mode Architecture
Overview

Simulation Mode provides a safe, real-time environment for validating automation logic without energizing physical relay outputs.

Simulation Mode does not introduce:

A separate execution engine

A virtual clock

Logical vs physical output separation

Alternate timing semantics

It operates on the same deterministic logic engine used in Normal operation.

Core Design Principles

Single Engine Architecture

Normal, Step, and Simulation modes all use the same logic kernel.

No shadow engine or simulation-only logic exists.

All evaluation semantics remain identical.

Real-Time Always

All timers continue using real system time.

No time scaling or virtualization is introduced.

Minimal Output Intervention

The logic engine computes output states normally.

In Simulation Mode, relay energizing code is conditionally skipped.

No additional output state abstraction layer is added.

Per-IO Control

Simulation Mode supports per-IO masking and input forcing.

Each IO channel may independently operate in real or simulated mode.

Execution Modes

The system supports three primary execution modes:

MODE_NORMAL

MODE_STEP

MODE_SIMULATION

Simulation Mode may operate in continuous or step-controlled execution.

Simulation Mode Behavior

When Simulation Mode is active:

1️⃣ Output Masking

Logic engine evaluates outputs normally.

Internal output states are updated exactly as in Normal mode.

Hardware relay energizing calls are conditionally skipped.

Masking may be applied globally or per output channel.

Example conceptual behavior:

if (simulationMode && outputMasked[channel])
{
    // Skip relay energizing
}
else
{
    energizeRelay(channel, state);
}

No additional logical/physical state duplication is introduced.

2️⃣ Input Forcing

Each input channel supports:

Real input value

Forced override value

Force enable flag

During Simulation Mode:

effectiveInput = isForced ? forcedValue : realValue;

The engine always evaluates effectiveInput.

Input forcing is available per channel and does not alter stored configuration.

3️⃣ Timing Behavior

Timers continue to accumulate using real elapsed time.

Entering or exiting Simulation Mode does not reset timers.

No alternate timing engine is created.

4️⃣ State Preservation

Simulation Mode does not reset logic state.

Latches, counters, and timers retain current values.

Mode switching does not require engine restart.

Per-IO Masking Model

In Simulation Mode:

Each Digital Output (DO) may be individually masked or allowed.

Each Input (DI/AI/SIO) may be individually forced or real.

Masking configuration applies only during active Simulation Mode.

This allows:

Testing specific outputs while keeping others active.

Commissioning partial systems.

Controlled relay validation.

Simulation + Stepping Interaction

Simulation Mode may be combined with Step Mode.

When combined:

One full LogicCard is evaluated per step.

Outputs remain masked according to per-IO settings.

Input forcing remains active.

Timing remains real.

Stepping does not alter simulation semantics.

UI Indication Requirements

When Simulation Mode is active:

A persistent visual indicator must be displayed.

Masked outputs should be visually distinguishable.

Forced inputs should be clearly marked.

Technician must be able to see which IO is real vs simulated.

Recommended UI terminology:

TEST MODE ACTIVE

OUTPUT MASKED

INPUT FORCED

Safety Considerations

Simulation Mode guarantees:

No relay energization for masked outputs.

No unintended actuation on masked channels.

Engine logic remains fully operational.

Emergency stop and safety dominance logic must remain fully functional.

Non-Goals

Simulation Mode does NOT provide:

Time rewind

State rollback

Virtual execution environment

Alternate evaluation semantics

Separate simulation kernel

Simulation Mode is strictly:

Real-time execution with conditional IO masking and input forcing.

Intended Use Cases

Simulation Mode enables technicians to:

Test safety interlocks safely

Validate sequence logic

Simulate sensor conditions

Commission systems incrementally

Debug without disconnecting field wiring

Summary

Simulation Mode is a real-time, IO-virtualized testing layer built on the production logic engine.

It:

Preserves determinism

Avoids architectural duplication

Maintains timing integrity

Enables safe commissioning

Supports per-IO control

Without increasing kernel complexity.