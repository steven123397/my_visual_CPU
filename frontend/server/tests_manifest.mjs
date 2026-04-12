import fs from 'node:fs';
import path from 'node:path';

import { interactiveOsBudgets } from './debug_budget.mjs';

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
    menuLabel: name,
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

function withPresentation(entry, presentation = {}) {
  return {
    ...entry,
    menuLabel: presentation.menuLabel ?? entry.menuLabel ?? entry.name,
    title: presentation.title ?? entry.name,
    badge: presentation.badge ?? null,
    summary: presentation.summary ?? null,
    workload: presentation.workload ?? null,
  };
}

export function listTests(repoRoot) {
  const myCpuRoot = path.join(repoRoot, 'myCPU');
  const asmTests = parseAsmTestsFromMakefile(myCpuRoot);
  const manifest = asmTests.map((name) => ({
    name,
    menuLabel: name,
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
    bootUntilUartText: interactiveOsBudgets.prompt,
    bootMaxSteps: interactiveOsBudgets.bootMaxSteps,
    terminalPrompt: interactiveOsBudgets.prompt,
    commandMaxSteps: interactiveOsBudgets.commandMaxSteps,
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
  manifest.push(withPresentation(
    guestEntry(myCpuRoot, 'guest_vector_demo'),
    {
      menuLabel: 'guest_vector_demo · V-lite ops',
      title: 'V-lite Operator Demo',
      badge: 'Vector + ML',
      summary: '用一个最小 guest workload 串起 dot / GEMM / Conv / ReLU，并以 V2OK 作为闭环 marker。',
      workload: {
        stage: 'P0-P3',
        category: 'vector-demo',
        expectedMarker: 'V2OK',
        ops: ['vsetcfg', 'vle.v', 'vdot.vv', 'vmax.vv', 'vse.v'],
        pipelineNote: '当前 frontend 只把已经落地的向量边界可视化：non-memory vector ALU 可进入最小 vector-aware path；config / memory 仍保守 serializing。',
        registerFocus: [3, 6, 9, 16],
      },
    },
  ));
  manifest.push(withPresentation(
    guestEntry(myCpuRoot, 'guest_vector_cnn_demo'),
    {
      menuLabel: 'guest_vector_cnn_demo · conv->relu',
      title: 'Minimal CNN Demo',
      badge: 'Vector + NN',
      summary: '固定输入与固定卷积核的 conv -> relu 样本，用 V3OK 验证当前最小 CNN-style guest 闭环。',
      workload: {
        stage: 'P0-P3',
        category: 'vector-cnn-demo',
        expectedMarker: 'V3OK',
        ops: ['vsetcfg', 'vle.v', 'vdot.vv', 'vmax.vv', 'vse.v'],
        pipelineNote: '这条 demo 重点展示当前仓库已经落地的 conv -> relu 闭环；pipeline 侧仍只在 non-memory vector ALU 上脱离统一 serializing fallback。',
        registerFocus: [3, 4, 5],
        cnn: {
          input: [2, -1, 3, 4, -2, 1],
          kernel: [1, 0, -1, 2],
          conv: [7, -9, 7],
          relu: [7, 0, 7],
          liveConvReg: 4,
          liveReluReg: 5,
        },
      },
    },
  ));

  return manifest;
}
