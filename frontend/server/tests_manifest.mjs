import path from 'node:path';

const asmTests = [
  'hello',
  'sum',
  'control_flow',
  'csr_trap',
  'timer_interrupt',
  'mtvec_modes',
  'trap_state',
  'exception_traps',
  'access_faults',
  'loads_signed_unsigned',
  'alu_word',
  'branches_signed_unsigned',
  'muldiv',
  'fence_noop',
  'privilege_transitions',
  'sret_transitions',
  'supervisor_exception_delegation',
  'supervisor_timer_interrupt',
  'csr_access_control',
  'counteren_access_control',
  'instret_counting',
  'machine_counter_csrs',
  'interrupt_csr_views',
  'delegation_edge_cases',
  'plic_machine_external_interrupt',
  'plic_supervisor_external_interrupt',
  'storage_device_basic',
  'supervisor_platform_smoke',
  'sv39_basic',
  'sv39_page_fault',
  'sv39_edge_faults',
  'sv39_sum_mxr',
  'sv39_tlb_flush',
  'sv39_tlb_ad_bits',
  'csr_semantic_consistency',
  'clint_split_access',
];

export function listTests(repoRoot) {
  const myCpuRoot = path.join(repoRoot, 'myCPU');
  const manifest = asmTests.map((name) => ({
    name,
    image: path.join(myCpuRoot, 'tests', 'asm', `${name}.elf`),
    disk:
      name === 'storage_device_basic' || name === 'supervisor_platform_smoke'
        ? path.join(myCpuRoot, 'tests', 'data', 'storage_basic.txt')
        : null,
    kind: 'asm',
  }));

  manifest.push({
    name: 'guest_supervisor_demo',
    image: path.join(myCpuRoot, 'guest', 'supervisor_demo.elf'),
    disk: path.join(myCpuRoot, 'tests', 'data', 'storage_basic.txt'),
    kind: 'guest',
  });

  return manifest;
}
