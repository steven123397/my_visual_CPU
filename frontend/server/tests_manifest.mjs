import fs from 'node:fs';
import path from 'node:path';

import {
  interactiveOsBudgets,
  LINUX_CONSOLE_BOOT_BUDGET,
  LINUX_CONSOLE_COMMAND_BUDGET,
} from './debug_budget.mjs';

const asmTestsCache = new Map();
const linuxConsoleBoot = Object.freeze({
  loadAddr: '0x80000000',
  kernelAddr: '0x80200000',
  dtbAddr: '0x87f00000',
  marker: 'mycpu-linux# ',
  bootMaxSteps: 300000000,
  prompt: 'mycpu-linux# ',
});
const linuxConsolePrimaryEnv = 'MYCPU_LINUX_PROTO_CONSOLE_IMAGE';
const linuxConsoleFallbackEnv = 'MYCPU_LINUX_PROTO_RUNTIME_IMAGE';

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

function resolveLinuxConsoleConfig() {
  if (process.env[linuxConsolePrimaryEnv]) {
    return {
      envVar: linuxConsolePrimaryEnv,
      path: path.resolve(process.env[linuxConsolePrimaryEnv]),
    };
  }
  if (process.env[linuxConsoleFallbackEnv]) {
    return {
      envVar: linuxConsoleFallbackEnv,
      path: path.resolve(process.env[linuxConsoleFallbackEnv]),
    };
  }
  return null;
}

export function linuxConsoleDiagnostic() {
  const config = resolveLinuxConsoleConfig();
  if (!config) {
    return {
      status: 'missing-env',
      ready: false,
      envVar: linuxConsolePrimaryEnv,
      message: 'Set MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image before starting the frontend server.',
    };
  }

  let stat = null;
  try {
    stat = fs.statSync(config.path);
  } catch {
    return {
      status: 'not-found',
      ready: false,
      envVar: config.envVar,
      path: config.path,
      message: `Image path does not exist: ${config.path}`,
    };
  }

  if (!stat.isFile()) {
    return {
      status: 'not-file',
      ready: false,
      envVar: config.envVar,
      path: config.path,
      message: `Image path is not a file: ${config.path}`,
    };
  }

  try {
    fs.accessSync(config.path, fs.constants.R_OK);
  } catch {
    return {
      status: 'not-readable',
      ready: false,
      envVar: config.envVar,
      path: config.path,
      message: `Image path is not readable: ${config.path}`,
    };
  }

  return {
    status: 'ready',
    ready: true,
    envVar: config.envVar,
    path: config.path,
    message: 'Linux serial console Image is configured.',
  };
}

function linuxConsoleEntry(myCpuRoot, diagnostic) {
  if (!diagnostic.ready) {
    return null;
  }

  const linuxImage = diagnostic.path;
  const linuxProtoRoot = path.join(myCpuRoot, 'workloads', 'linux_proto');
  return {
    name: 'linux_proto_console',
    menuLabel: 'linux_proto_console · Linux serial',
    backend: 'functional',
    image: path.join(linuxProtoRoot, 'linux_sbi_shim.bin'),
    imageFormat: 'flat',
    loadAddr: linuxConsoleBoot.loadAddr,
    disk: path.join(linuxProtoRoot, 'rootfs.ext4'),
    diskReady: true,
    diskMagicValid: true,
    blockTransport: 'virtio-blk',
    kind: 'linux',
    payloads: [
      { image: linuxImage, addr: linuxConsoleBoot.kernelAddr },
      { image: path.join(linuxProtoRoot, 'mycpu_virt.dtb'), addr: linuxConsoleBoot.dtbAddr },
    ],
    gprSeeds: [
      { reg: 'a0', value: '0x0' },
      { reg: 'a1', value: linuxConsoleBoot.dtbAddr },
      { reg: 'a2', value: linuxConsoleBoot.kernelAddr },
    ],
    bootUntilUartText: linuxConsoleBoot.marker,
    bootMaxSteps: linuxConsoleBoot.bootMaxSteps,
    bootRequestTimeoutMs: LINUX_CONSOLE_BOOT_BUDGET.requestTimeoutMs,
    commandUntilUartText: linuxConsoleBoot.prompt,
    commandMaxSteps: LINUX_CONSOLE_COMMAND_BUDGET.maxSteps,
    commandRequestTimeoutMs: LINUX_CONSOLE_COMMAND_BUDGET.requestTimeoutMs,
    terminalPrompt: linuxConsoleBoot.prompt,
    title: 'Linux Serial Console',
    badge: 'Linux runtime',
    summary: '启动受控 linux_proto runtime，进入 UART 串口 console，观察 Linux userland smoke marker。',
    workload: {
      stage: 'Wave 7',
      category: 'linux-serial-console',
      expectedMarker: linuxConsoleBoot.prompt,
      ops: ['flat SBI shim', 'Linux Image payload', 'DTB', 'virtio-blk rootfs'],
      pipelineNote: '配置本机 Linux Image 后才可运行；前端桥接 UART 与现有 debug session，并进入 linux_proto mini shell prompt；不在浏览器内运行 Linux。',
      assetNote: 'Set MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image before starting the frontend server.',
      progress: [
        ['Boot', 'flat SBI shim 加载 Linux Image、DTB 和 rootfs'],
        ['UART', 'serial console 输出通过 debug server 增量投影到浏览器'],
        ['Control', 'Load / Run / Pause / Reset 复用现有 session 控制'],
        ['Guardrail', '缺少 runtime Image 时不创建虚假 Linux session'],
      ],
    },
  };
}

export function listTests(repoRoot) {
  const myCpuRoot = path.join(repoRoot, 'myCPU');
  const linuxConsole = linuxConsoleDiagnostic();
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
  const linuxConsoleTest = linuxConsoleEntry(myCpuRoot, linuxConsole);
  if (linuxConsoleTest) {
    manifest.push(linuxConsoleTest);
  }
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
    guestEntry(myCpuRoot, 'guest_ai_accel_demo'),
    {
      menuLabel: 'guest_ai_accel_demo · AI accel MMIO',
      title: 'AI Accelerator Demo',
      badge: 'AI Accelerator',
      summary: '通过 MMIO 提交一个最小 graph package，并用 KMVAI 验证 guest 到设备的闭环。',
      workload: {
        stage: 'Wave 4',
        category: 'ai-accelerator-demo',
        expectedMarker: 'KMVAI',
        ops: ['graph package', 'MMIO doorbell', 'DMA load/store', 'timed-simple profile'],
        pipelineNote: '当前 frontend 只展示 debug snapshot 中的 aggregate counters；op summary 和真实 DMA overlap 后移到后续专项阶段。',
        progress: [
          ['Queue', '单 entry submission / completion queue'],
          ['DMA', 'load/store bytes 来自 debug snapshot'],
          ['Compute', 'timed-simple compute / stall attribution'],
          ['Profile', '只读 aggregate counters'],
        ],
      },
    },
  ));
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

  manifest.diagnostics = {
    linuxConsole,
  };
  return manifest;
}
