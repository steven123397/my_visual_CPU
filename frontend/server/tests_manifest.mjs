import fs from 'node:fs';
import path from 'node:path';

const asmTestsCache = new Map();

function parseAsmTestsFromMakefile(myCpuRoot) {
  const cacheKey = myCpuRoot;
  if (asmTestsCache.has(cacheKey)) {
    return asmTestsCache.get(cacheKey);
  }

  const makefilePath = path.join(myCpuRoot, 'Makefile');
  const makefile = fs.readFileSync(makefilePath, 'utf8');
  const match = makefile.match(/^ASM_TESTS\s*=\s*(.+)$/m);
  if (!match) {
    throw new Error(`ASM_TESTS not found in ${makefilePath}`);
  }

  const asmTests = match[1].trim().split(/\s+/).filter(Boolean);
  asmTestsCache.set(cacheKey, asmTests);
  return asmTests;
}

function guestEntry(myCpuRoot, name, diskMode = 'none', imageName = null) {
  const withDisk = diskMode !== 'none';
  const resolvedImageName = imageName ?? name.replace(/^guest_/, '');
  return {
    name,
    image: path.join(myCpuRoot, 'guest', `${resolvedImageName}.elf`),
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
  const asmTests = parseAsmTestsFromMakefile(myCpuRoot);
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
  manifest.push({
    ...guestEntry(myCpuRoot, 'guest_interactive_os_demo', 'ready', 'interactive_os'),
    bootUntilUartText: 'monitor> ',
    bootMaxSteps: 5000000,
    terminalPrompt: 'monitor> ',
  });
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
