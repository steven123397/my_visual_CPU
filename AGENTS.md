# AGENTS.md

## Project scope

This repository currently contains `myCPU`, a small RISC-V simulator written in C. It already supports:

- ELF64 and flat binary loading
- RV64 integer execution for a minimal bare-metal environment
- CSR access
- M-mode trap handling
- UART MMIO output
- CLINT timer interrupt basics

The long-term goal is not just to extend this into a larger simulator, but to eventually use that simulator as the platform for a self-built operating system.

## Current status

The project is still at the "functional simulator prototype" stage.

What exists now:

- A direct fetch-decode-execute loop
- A simple physical memory model plus minimal MMIO
- Basic exception and interrupt handling
- Assembly smoke tests

What does not yet exist:

- Full privileged architecture support
- Virtual memory and page table walking
- A realistic device platform for OS bring-up
- Pipeline, cache, multicore, or out-of-order models

## Primary direction

When making design decisions, prioritize these goals in this order:

1. Preserve a correct and debuggable ISA-level reference model.
2. Extend the simulator until it can support a small custom OS.
3. Only after that, add microarchitectural models such as pipelines, OoO execution, and cache hierarchies.

Do not mix all three goals into one implementation step.

## Development phases

### Phase 1: solid functional simulator for OS bring-up

Focus here first. This phase is the shortest path toward running a custom OS.

Planned work:

- Complete and validate RV64I/RV64M support
- Revisit claimed ISA support and keep README/docs aligned with real behavior
- Add proper privileged mode support: `M/S/U`
- Extend CSR coverage needed for supervisor execution
- Implement trap delegation where appropriate
- Add `sret`, supervisor traps, and privilege transitions
- Add virtual memory with Sv39 page tables
- Add page faults and access faults
- Add TLB support
- Expand device platform:
  - UART
  - CLINT
  - PLIC or equivalent external interrupt controller
  - At least one storage device model for OS experiments
- Improve image/kernel loading flow
- Add stronger test coverage and differential validation

Definition of done for Phase 1:

- The simulator can boot and run a small custom OS kernel with timer interrupts, basic memory management, and console output.

### Phase 2: alternate CPU execution models

Only begin after Phase 1 is stable.

Goal:

- Keep one ISA semantic source of truth while introducing multiple execution backends.

Planned execution models:

- Single-cycle model
- Multi-cycle model
- Classic 5-stage pipeline

Requirements:

- Avoid copying instruction semantics into multiple code paths.
- Separate "architectural effect of an instruction" from "how many cycles and stages it takes".
- Preserve a reference functional core for debugging.

### Phase 3: advanced microarchitecture

Only begin after the pipeline model is correct and testable.

Planned work:

- Dynamic branch prediction
- Register renaming
- Reservation stations
- Reorder buffer
- Load/store queue
- Recovery from branch mispredicts
- Precise exceptions in speculative execution

Requirements:

- Maintain precise architectural commit behavior.
- Build rollback and recovery paths explicitly.
- Add tracing and debug hooks before adding complexity.

### Phase 4: memory hierarchy and multicore

Only begin after single-core advanced execution is stable.

Planned work:

- L1 I-cache
- L1 D-cache
- L2 cache
- Cache replacement and write policies
- Shared interconnect or bus model
- Multicore execution
- Cache coherence protocol
- DMA interactions with memory and cache hierarchy

Requirements:

- Add these incrementally.
- Prefer single-core plus cache before multicore.
- Prefer coherent architectural definitions before optimization.

## Architectural guidance

### Preserve a reference model

Always keep one simple, correct architectural reference path. This is the ground truth used for:

- correctness checks
- trap behavior validation
- CSR behavior validation
- memory translation validation
- later comparison against pipelined or OoO models

Do not delete the simple reference core when adding more advanced execution backends.

### Separate architectural semantics from timing

Instruction meaning should not be rewritten separately for:

- functional execution
- multi-cycle execution
- pipeline execution
- OoO execution

The repository should eventually evolve toward shared instruction semantics plus pluggable execution models.

### Design toward components

As the system grows, favor clear module boundaries:

- ISA decode
- architectural state
- privilege and trap handling
- address translation
- memory subsystem
- device/bus subsystem
- execution backend
- debug/trace/test infrastructure

### Prefer incremental bring-up

For any large feature, land it in the smallest correct form first.

Examples:

- MMU before full OS
- single-core cache before multicore coherence
- static branch prediction before complex dynamic predictors
- in-order pipeline before OoO

## Testing expectations

Testing is mandatory for all architecture features.

Minimum expectations:

- Keep small assembly tests for each instruction family
- Add tests for traps, CSR behavior, and privilege transitions
- Add tests for page table translation and page faults
- Add regression tests for device MMIO behavior
- Add reference-vs-backend comparisons once multiple execution models exist

If a feature is too complex to test, the design is probably still too large.

## Codebase evolution notes

The current codebase is small and C-oriented. That is acceptable for Phase 1.

If the project grows into:

- multiple execution backends
- MMU and device platform layers
- caches and multicore
- speculative microarchitecture

then a move toward a stronger modular design is expected. That may still be in C, but a C++ refactor can become reasonable once complexity starts being dominated by architecture composition rather than raw instruction semantics.

Do not perform a cosmetic C-to-C++ rewrite with no structural gain. Any migration should improve:

- ownership and resource management
- module boundaries
- type safety
- backend extensibility

## Editing guidance for future work

- Keep README claims aligned with real implemented behavior.
- Favor correctness and observability over speed.
- Document architectural assumptions in code and docs.
- Add new modules rather than inflating one giant `cpu.c`.
- Avoid speculative abstractions unless a second real use case exists.
