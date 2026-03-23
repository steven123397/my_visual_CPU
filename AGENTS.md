# AGENTS.md

## Project scope

This repository currently contains `myCPU`, a small RISC-V simulator that began as a C prototype and is now in an incremental C++ refactor. It already supports:

- ELF64 and flat binary loading
- RV64 integer execution for a minimal bare-metal environment
- CSR access
- M-mode trap handling
- initial M/S/U privilege groundwork
- UART MMIO output
- CLINT timer interrupt basics

The long-term goal is not just to extend this into a larger simulator, but to eventually use that simulator as the platform for a self-built operating system.

## Current status

The project is still at the "functional simulator prototype" stage.

What exists now:

- A direct fetch-decode-execute loop
- A simple physical memory model plus minimal MMIO
- Basic exception and interrupt handling
- Initial M/S/U privilege groundwork, including `MPP` tracking, `sret`, minimal supervisor exception delegation via a constrained `medeleg`, minimal supervisor timer interrupt delivery via a constrained `mideleg`, constrained `mie` / `mip` / `sie` / `sip` alias semantics, read-only `misa` reporting, WARL-constrained `satp.MODE` handling, machine-counter CSR support for `mcycle` / `minstret`, `mcounteren` / `scounteren` gating for `cycle` / `time` / `instret`, `time` CSR reads now aligned to CLINT `mtime`, retired-instruction counting for `instret`, and `sstatus.SUM` / `sstatus.MXR` handling relevant to supervisor virtual-memory access
- A `Machine + Ram + Uart16550 + Clint + Plic + SimpleStorage + Bus` C++ platform skeleton around the reference path, plus a first shared MMIO platform contract header/doc for OS-facing device constants
- Explicit `ElfLoader + BinaryLoader` C++ loader boundaries above raw RAM backing
- A first `CoreState + CsrFile` state split inside the CPU path
- A first `TrapController` boundary for trap / interrupt routing inside the CPU path
- An `AddressSpace` boundary between CPU fetch/load/store and the physical `Bus`, supporting both bare-mode passthrough and Sv39 virtual memory translation
- Instruction semantics are now split by family into explicit execution modules for integer, control-flow, memory, and system/CSR handling outside the monolithic CPU execution file
- CPU fetch/load/store paths routed through `Bus` instead of directly through the legacy `Memory*` interface
- `Bus::tick()` now aggregates device-driven platform events instead of hard-coding CLINT state in the CPU step path
- `Ram` now exposes explicit bulk write/fill interfaces for image loading and participates in bus dispatch as a normal attached device
- Assembly regression tests with output checking for:
  - basic UART output
  - control flow (`beq` / `jal` / `jalr` / backward branches)
  - CSR access and `ecall` / `mret` smoke coverage
  - CLINT timer interrupt delivery and `mret` return
  - CLINT split-width `mtime` / `mtimecmp` access behavior and `time` / `mtime` consistency
  - supervisor timer interrupt delegation via `mideleg`, `stvec`, and `sret`
  - PLIC-backed machine/supervisor external interrupt delivery and claim/complete via a minimal UART THRE interrupt source
  - host-backed simple storage image attachment through a minimal block-oriented MMIO storage device with `LBA` / `block_count` / `command` / `status` registers and a fixed data window
  - a first shared guest-side supervisor platform layer plus smoke/demo coverage that consumes the shared MMIO contract for UART output, supervisor PLIC setup/claim/complete, supervisor timer handling through CLINT `mtime` / `mtimecmp`, block-0 storage reads, and a minimal C-based supervisor runtime with unified trap dispatch, registered interrupt/exception handlers, a guest-side VM-owned page-fault policy path for demand-mapped and recoverable faults, explicit linker-defined memory layout, an early bump allocator, a bitmap-backed physical page manager, and a minimal guest-side Sv39 page-table builder with page-granular kernel mappings, fault-range-backed page-fault bring-up, stricter `vm_map_range` / `vm_unmap_page` semantics, and first kernel/user range-split helper coverage
  - interrupt CSR alias constraints for `mie` / `mip` / `sie` / `sip`, plus delegation edge behavior around `medeleg` / `mideleg`
  - unmapped instruction/load/store access-fault behavior
  - `mtvec` direct / vectored mode routing
  - Sv39 virtual memory: identity mapping, page faults on unmapped addresses, non-canonical and cross-page edge faults, `sstatus.MXR` / `sstatus.SUM` access semantics, stale-translation invalidation via `sfence.vma`, and leaf-PTE `A/D` maintenance across TLB hits
- M-mode trap-state behavior (`mstatus` / `mepc`)
- `ebreak` and illegal-instruction exception behavior
- privilege transitions across `U/S/M`, `sret`, minimal supervisor exception delegation, CSR access-control guardrails, read-only `misa`, WARL-constrained `satp.MODE`, constrained interrupt CSR alias semantics for `mie` / `mip` / `sie` / `sip`, machine-counter CSR support for `mcycle` / `minstret`, retired-instruction counting for `instret`, and `mcounteren` / `scounteren` gating for `cycle` / `time` / `instret`
- Sv39 virtual memory with 3-level page table walk, a minimal Sv39 TLB, canonical-address checks, permission checks (R/W/X/U plus `SUM` / `MXR` effects), superpage support (4KB/2MB/1GB), page fault handling, cross-page fault handling, and A/D bit updates
- `satp` CSR support for bare-mode and Sv39 mode switching, with unsupported `MODE` values masked through a constrained WARL path
- `sfence.vma` instruction with minimal legal-encoding acceptance and full local TLB invalidation

What does not yet exist:

- Full privileged architecture support (complete CSR coverage)
- A realistic device platform for OS bring-up beyond the newly landed minimal PLIC/block-storage path (broader peripherals, richer storage protocols)
- Pipeline, cache, multicore, or out-of-order models

Additional current planning notes:

- The existing `myCPU` prototype was authored by the project owner and should be treated as completed prior work, not as a hypothetical design.
- Before large new architectural features are added, the codebase should be restructured from the current small C prototype into a modular C++ codebase that can support significantly higher system complexity.
- This C-to-C++ transition must be justified by structural gains such as module boundaries, type safety, state management, ownership clarity, and backend extensibility, not by language preference alone.
- The initial C++ restructuring is already underway: `Machine` / `Bus` / `Ram`, explicit `Uart16550` / `Clint` device objects, explicit `ElfLoader` / `BinaryLoader` image-loading boundaries, the first `CoreState + CsrFile` split, and a first `TrapController` boundary are now landed and should be treated as current baseline, not future proposal.
- The immediate next structural step is to continue tightening the platform-side split: preserve `Bus` as the physical CPU-facing access path, deepen device/platform boundaries, and keep shrinking the remaining legacy responsibilities around raw RAM access and image loading.
- Sv39 virtual memory is now implemented with 3-level page table walk, a minimal TLB, permission checks, and page fault handling. On the guest/runtime side, a minimal PMM, guest-side Sv39 page-table builder, S-mode page-fault dispatch, reusable guest VM/trap page-fault handling, VM-owned page-fault policy registration for demand-mapped and recoverable faults, stricter `vm_map_range` / `vm_unmap_page` semantics, and first kernel/user range-split helpers are now landed. The immediate next functional step is to keep broadening reusable fault/mapping management and continue preparing a cleaner kernel/user address-space split for OS bring-up.
- When producing summaries, proposals, or report-style material for this project, describe the current implementation as an already working simulator prototype and the C++ refactor as the next enabling engineering step.

## Primary direction

When making design decisions, prioritize these goals in this order:

1. Preserve a correct and debuggable ISA-level reference model.
2. Extend the simulator until it can support a small custom OS.
3. Only after that, add microarchitectural models such as pipelines, OoO execution, and cache hierarchies.

Do not mix all three goals into one implementation step.

At the current stage, an additional near-term priority applies before major Phase 1 feature growth:

0. Reorganize the current prototype into a C++ architecture that remains simple and debuggable but is better prepared for privilege, MMU, device-platform, and future backend expansion.

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
- Expand device platform:
  - UART
  - CLINT
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

Current baseline expectation for local validation:

- Keep `make test` green
- Treat the existing `hello`, `sum`, `control_flow`, `csr_trap`, `timer_interrupt`, `mtvec_modes`, `trap_state`, `exception_traps`, `access_faults`, `loads_signed_unsigned`, `alu_word`, `branches_signed_unsigned`, `muldiv`, `fence_noop`, `privilege_transitions`, `sret_transitions`, `supervisor_exception_delegation`, `supervisor_timer_interrupt`, `csr_access_control`, `counteren_access_control`, `instret_counting`, `machine_counter_csrs`, `interrupt_csr_views`, `delegation_edge_cases`, `plic_machine_external_interrupt`, `plic_supervisor_external_interrupt`, `storage_device_basic`, `sv39_basic`, `sv39_page_fault`, `sv39_edge_faults`, `sv39_sum_mxr`, `sv39_tlb_flush`, `sv39_tlb_ad_bits`, `csr_semantic_consistency`, and `clint_split_access` assembly regressions, plus the flat-binary `hello` load path, as required guardrails when touching the reference path
- If a refactor changes observable UART output or causes hangs, update tests only when the behavior change is intentional and justified

If a feature is too complex to test, the design is probably still too large.

## Codebase evolution notes

The current codebase is still small, but it is no longer purely C-oriented. It now mixes a C reference semantic core with incremental C++ structure around platform assembly and state ownership. That is acceptable for Phase 1.

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

For the current project stage, this migration is no longer only a distant option; it is now part of the intended roadmap before the simulator grows substantially in complexity. The migration should still be incremental and should preserve a working functional reference path throughout.

What is already landed in that migration:

- `Machine`, `Bus`, and `Ram` provide the first explicit platform assembly layer
- `Uart16550`, `Clint`, a first minimal `Plic`, and a host-backed block-oriented `SimpleStorage` device now exist as explicit device objects behind `Bus`
- A shared MMIO platform contract now exists in code/docs, along with a reusable guest-side platform library, an assembly shim consumed by tests, and a first minimal supervisor demo that exercises storage, supervisor external interrupts, supervisor timer interrupts, registered trap handlers through a unified dispatch path, a VM-owned guest-side page-fault policy path, a linker-backed early allocator plus bitmap-backed physical page manager, and guest-side Sv39 page-table bring-up with fault-range-backed page-fault handling, recoverable page-fault policy registration, stricter `vm_map_range` / `vm_unmap_page` semantics, page-granular kernel mappings, and first kernel/user range-split helpers from S-mode
- Image loading now passes through explicit `ElfLoader` / `BinaryLoader` modules, keeping `Ram` focused on backing storage
- ELF and flat-binary loading now write through explicit `Ram` interfaces rather than reaching into raw `Memory` state from loader code
- CPU state is no longer just one flat struct; a first `CoreState + CsrFile` boundary now exists
- Trap logic now has a first explicit `TrapController` boundary, with current regression coverage around trap entry, return, timer interrupts, `mtvec` modes, basic M-mode exception semantics, and minimal supervisor timer interrupt delivery
- Privilege groundwork now includes `MPP` tracking, `ecall` cause separation by privilege, `sret`, minimal constrained-`medeleg` supervisor exception delegation, constrained-`mideleg` supervisor timer interrupt delivery, constrained `mie` / `mip` / `sie` / `sip` alias semantics, CSR privilege/read-only access checks, read-only `misa`, WARL-constrained `satp.MODE`, machine-counter CSR support for `mcycle` / `minstret`, `mcounteren` / `scounteren` gating for `cycle` / `time` / `instret`, `time` CSR reads aligned to CLINT `mtime`, retired-instruction counting for `instret`, and `sstatus.SUM` / `sstatus.MXR` handling for supervisor virtual-memory access
- `AddressSpace` boundary now exists between CPU fetch/load/store and the physical `Bus`, supporting both bare-mode passthrough and Sv39 virtual memory with 3-level page table walk, a minimal TLB, canonical-address checks, permission checks, superpage support, cross-page fault handling, page fault handling, and A/D bit updates
- CPU fetch/load/store now routes through `Bus`, and platform tick events now flow through `TrapController`, so RAM/device dispatch and timer-event routing are no longer hard-coded in the CPU step path
- Instruction-family splits now exist for integer, control-flow, memory, and system/CSR execution, so semantic extraction has started without introducing multi-backend abstraction yet
- The repository still intentionally keeps a simple architectural reference execution path

When planning the C++ restructuring, favor boundaries such as:

- architectural state / CPU core state
- CSR and privilege management
- trap and interrupt routing
- memory and bus/device dispatch
- address translation and MMU support
- program/image loading
- execution semantics versus execution backend control
- testing/debug/trace utilities

## Editing guidance for future work

- Keep README claims aligned with real implemented behavior.
- Favor correctness and observability over speed.
- Document architectural assumptions in code and docs.
- Add new modules rather than inflating one giant `cpu.c`.
- Preserve the existing `CoreState + CsrFile` split and deepen it rather than collapsing state back into a monolithic CPU object.
- Keep the reference execution path simple; do not introduce backend abstractions before there is a concrete second execution model to justify them.
- Treat the current assembly regression suite as part of the architectural contract for the reference path.
- Avoid speculative abstractions unless a second real use case exists.

## Documentation and reporting guidance

- When writing report-style content, clearly distinguish:
  - completed prior work already implemented by the project owner
  - completed C++ restructuring work already landed in the repository
  - the immediate next engineering task of continued C++ restructuring
  - later functional milestones such as privilege support, MMU, and OS bring-up
- Describe the current project honestly as a working functional simulator prototype, not as a mere idea.
- Do not claim support for ISA or platform features unless they are actually implemented and validated.
- When discussing the C++ migration in documents, frame it as a structural response to growing architectural complexity, not as a cosmetic language rewrite.
- At the current repository state, describe `Machine` / `Bus` / `Ram`, explicit `Uart16550` / `Clint` / minimal `Plic` / host-backed block-oriented `SimpleStorage` devices, the shared MMIO platform contract header/doc, explicit `ElfLoader` / `BinaryLoader` loader boundaries, the `CoreState + CsrFile` split, the `TrapController` boundary, the `AddressSpace` boundary with Sv39 virtual memory support, a minimal TLB, edge-fault coverage, and minimal `SUM` / `MXR` handling, plus the guest-side VM/trap layer with reusable VM-owned page-fault policy/handling, stricter range/unmap semantics, and first kernel/user range-split helpers as already completed incremental steps; describe expanded CSR coverage and further device-platform targets beyond the minimal storage path as the next structural targets for OS bring-up.
