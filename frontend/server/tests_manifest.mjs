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
  'muldiv_edge_cases',
  'illegal_integer_encodings',
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

function guestEntry(myCpuRoot, name, diskMode = 'none') {
  const withDisk = diskMode !== 'none';
  return {
    name,
    image: path.join(myCpuRoot, 'guest', `${name.replace(/^guest_/, '')}.elf`),
    disk:
      withDisk
        ? path.join(myCpuRoot, 'tests', 'data', 'storage_basic.txt')
        : null,
    diskReady: diskMode !== 'not-ready',
    diskMagicValid: diskMode !== 'bad-magic',
    kind: 'guest',
  };
}

export function listTests(repoRoot) {
  const myCpuRoot = path.join(repoRoot, 'myCPU');
  const manifest = asmTests.map((name) => ({
    name,
    image: path.join(myCpuRoot, 'tests', 'asm', `${name}.elf`),
    disk:
      name === 'storage_device_basic' || name === 'supervisor_platform_smoke'
        ? path.join(myCpuRoot, 'tests', 'data', 'storage_basic.txt')
        : null,
    diskReady: true,
    diskMagicValid: true,
    kind: 'asm',
  }));

  manifest.push(guestEntry(myCpuRoot, 'guest_supervisor_demo', 'ready'));
  manifest.push(guestEntry(myCpuRoot, 'guest_kernel_alpha_demo', 'ready'));
  manifest.push(guestEntry(myCpuRoot, 'guest_kernel_alpha_fault_demo'));
  manifest.push(guestEntry(myCpuRoot, 'guest_kernel_alpha_storage_no_media_demo'));
  manifest.push(guestEntry(myCpuRoot, 'guest_kernel_alpha_storage_not_ready_demo', 'not-ready'));
  manifest.push(guestEntry(myCpuRoot, 'guest_kernel_alpha_storage_bad_magic_demo', 'bad-magic'));
  manifest.push(guestEntry(myCpuRoot, 'guest_kernel_alpha_storage_bad_block_count_demo', 'ready'));
  manifest.push(guestEntry(myCpuRoot, 'guest_kernel_alpha_storage_lba_range_demo', 'ready'));
  manifest.push(guestEntry(myCpuRoot, 'guest_kernel_alpha_storage_bad_command_demo', 'ready'));
  manifest.push(guestEntry(myCpuRoot, 'guest_kernel_alpha_plic_not_ready_demo'));
  manifest.push(guestEntry(myCpuRoot, 'guest_kernel_alpha_timer_not_ready_demo'));

  return manifest;
}
