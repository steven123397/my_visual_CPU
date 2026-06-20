// 测试/工作流 manifest 构建：组装 asm/guest/linux 全量测试条目、Linux console 诊断与展示元数据。
import fs from 'node:fs';
import path from 'node:path';

import {
  courseOsShellBudgets,
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

// 从 Makefile 解析 ASM_TESTS 测试名并缓存结果。
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

// 构造一个 guest 类型测试条目对象（含磁盘模式）。
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

// 为条目叠加菜单标签/标题/徽章等展示元数据。
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

// 为条目叠加终端 prompt 与运行预算元数据。
function withTerminalMetadata(entry, {
  prompt,
  bootMaxSteps,
  bootRequestTimeoutMs,
  commandMaxSteps,
  commandRequestTimeoutMs,
  commandWait = 'activity',
  commandUntilUartText = commandWait === 'prompt' ? prompt : undefined,
  presentation = {},
  workload = {},
  terminal = {},
}) {
  return withPresentation(
    {
      ...entry,
      bootUntilUartText: prompt,
      bootMaxSteps,
      ...(bootRequestTimeoutMs == null ? {} : { bootRequestTimeoutMs }),
      terminalPrompt: prompt,
      ...(commandUntilUartText == null ? {} : { commandUntilUartText }),
      commandMaxSteps,
      ...(commandRequestTimeoutMs == null ? {} : { commandRequestTimeoutMs }),
    },
    {
      ...presentation,
      workload: {
        ...workload,
        expectedMarker: workload.expectedMarker ?? prompt,
        terminal: {
          kind: terminal.kind,
          target: terminal.target,
          commandWait,
          prompt,
          ...(terminal.title == null ? {} : { title: terminal.title }),
        },
      },
    },
  );
}

// 从主/备环境变量解析 Linux console 镜像路径。
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

// 检查 Linux console 镜像就绪状态并返回诊断（env/路径/可读性）。
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

// 构建受控 Linux 串口 console 的测试条目（gated runtime route）。
function linuxConsoleEntry(myCpuRoot, diagnostic) {
  if (!diagnostic.ready) {
    return null;
  }

  const linuxImage = diagnostic.path;
  const linuxProtoRoot = path.join(myCpuRoot, 'workloads', 'linux_proto');
  return withTerminalMetadata(
    {
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
    },
    {
      prompt: linuxConsoleBoot.prompt,
      bootMaxSteps: linuxConsoleBoot.bootMaxSteps,
      bootRequestTimeoutMs: LINUX_CONSOLE_BOOT_BUDGET.requestTimeoutMs,
      commandMaxSteps: LINUX_CONSOLE_COMMAND_BUDGET.maxSteps,
      commandRequestTimeoutMs: LINUX_CONSOLE_COMMAND_BUDGET.requestTimeoutMs,
      commandWait: 'prompt',
      presentation: {
        menuLabel: 'linux_proto_console · Linux serial',
        title: 'Linux Serial Console',
        badge: 'Linux runtime',
        summary: '启动受控 linux_proto runtime，进入 UART 串口 console，观察 Linux userland smoke marker。',
      },
      terminal: {
        kind: 'linux-serial',
        target: 'Linux serial console',
        title: 'Linux serial terminal',
      },
      workload: {
        stage: 'Wave 7',
        category: 'linux-serial-console',
        ops: ['flat SBI shim', 'Linux Image payload', 'DTB', 'virtio-blk rootfs'],
        pipelineNote: '配置本机 Linux Image 后才可运行；前端桥接 UART 与现有 debug session，并进入 linux_proto mini shell prompt；不在浏览器内运行 Linux。',
        assetEnvVar: linuxConsolePrimaryEnv,
        assetNote: 'Set MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image before starting the frontend server.',
        progress: [
          ['Boot', 'flat SBI shim 加载 Linux Image、DTB 和 rootfs'],
          ['UART', 'serial console 输出通过 debug server 增量投影到浏览器'],
          ['Control', 'Load / Run / Pause / Reset 复用现有 session 控制'],
          ['Guardrail', '缺少 runtime Image 时不创建虚假 Linux session'],
        ],
      },
    },
  );
}

// 构建 Stage 11 host-only 工作流命令清单。
function stage11HostOnlyWorkflowManifest() {
  return {
    enabled: true,
    route: 'host-only',
    title: 'Stage 11 external workflow',
    externalRootfsEnv: 'MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS',
    target: 'test-host-course_os_linux_compat_external_workflow_smoke',
    boundary: 'Visible for host smoke and CI evidence only; the browser console does not run external rootfs workflows.',
    commands: [
      {
        command: 'git init stage11repo',
        markers: [
          'linux-compat: rootfs=external',
          'linux-compat: path=/usr/bin/git',
          'Initialized',
          'course-os> ',
        ],
      },
      {
        command: 'vim stage11repo/hello.c',
        markers: [
          'linux-compat: path=/usr/bin/vim',
          'course-os> ',
        ],
      },
      {
        command: 'git -c safe.directory=/stage11repo -C stage11repo add hello.c',
        markers: [
          'linux-compat: path=/usr/bin/git',
          'course-os> ',
        ],
      },
      {
        command: 'git -C stage11repo -c safe.directory=/stage11repo -c user.name=stage11 -c user.email=stage11@example.invalid commit -m init',
        markers: [
          'file changed',
          'create mode',
          'course-os> ',
        ],
      },
      {
        command: 'git -C stage11repo -c safe.directory=/stage11repo --no-pager log --oneline',
        markers: [
          'linux-compat: path=/usr/bin/git',
          'init',
          'course-os> ',
        ],
      },
      {
        command: 'cd stage11repo && gcc hello.c && ./a.out',
        markers: [
          'linux-compat: path=/usr/bin/gcc',
          'stage11 hello',
          'exec=real',
          'course-os> ',
        ],
      },
    ],
  };
}

// 组装 asm/guest/linux 全量测试 manifest 并返回（含 diagnostics 与展示元数据）。
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
  manifest.push(withTerminalMetadata(
    guestEntry(myCpuRoot, 'guest_interactive_os_demo', 'ready', 'interactive_os'),
    {
      prompt: interactiveOsBudgets.prompt,
      bootMaxSteps: interactiveOsBudgets.bootMaxSteps,
      commandMaxSteps: interactiveOsBudgets.commandMaxSteps,
      commandWait: 'activity',
      presentation: {
        menuLabel: 'guest_interactive_os_demo · interactive OS',
        title: 'Interactive OS Monitor',
        badge: 'Monitor',
        summary: '进入 interactive_os guest monitor，通过 UART 操作 help、echo、time、uptime、disk、regs、peek、pagewalk 和 pte 命令。',
      },
      terminal: {
        kind: 'monitor',
        target: 'guest monitor',
        title: 'interactive_os terminal',
      },
      workload: {
        stage: 'interactive_os',
        category: 'interactive-monitor',
        ops: ['UART terminal', 'guest monitor', 'storage probe', 'runtime counters', 'page table inspection'],
        pipelineNote: '保留 monitor> 命令语义；manifest 只声明 prompt、terminal presentation 和 command budget。',
      },
    },
  ));
  manifest.push(withTerminalMetadata(
    {
      ...guestEntry(myCpuRoot, 'guest_course_os_shell_demo', 'ready', 'course_os_shell'),
      backend: 'pipeline',
    },
    {
      prompt: courseOsShellBudgets.prompt,
      bootMaxSteps: courseOsShellBudgets.bootMaxSteps,
      bootRequestTimeoutMs: courseOsShellBudgets.bootRequestTimeoutMs,
      commandMaxSteps: courseOsShellBudgets.commandMaxSteps,
      commandRequestTimeoutMs: courseOsShellBudgets.commandRequestTimeoutMs,
      commandWait: 'prompt',
      presentation: {
        menuLabel: 'guest_course_os_shell_demo · Course OS shell',
        title: 'Course OS Shell',
        badge: 'Course OS',
        summary: '打开课程 OS Stage 4 交互 shell，通过 UART terminal 操作 procfs、FD / FS、pipe、ELF / libc、COW 与 crash evidence。',
      },
      terminal: {
        kind: 'course-os',
        target: 'Course OS shell',
        title: 'Course OS shell terminal',
      },
      workload: {
        stage: 'Course OS Stage 4',
        category: 'course-os-shell',
        ops: ['UART terminal', 'course shell', 'procfs shortcuts', 'ELF / libc programs', 'COW / crash evidence'],
        pipelineNote: '默认使用 pipeline backend；前端只复用 manifest、debug session、terminal prompt 和 snapshot 合同，不新增专用执行协议。',
        hostOnlyWorkflow: stage11HostOnlyWorkflowManifest(),
        progress: [
          ['Boot', '独立 guest_course_os_shell_demo 进入 course-os> prompt'],
          ['Shell', 'help、文件重定向、pipe 和 exec 命令经 guest shell 执行'],
          ['Evidence', 'procfs 快捷命令展示 Stage 1 / Stage 2 / Stage 3 证据面'],
          ['Control', 'Load / Run / Pause / Step / Reset / Terminate 复用现有 session 控制'],
        ],
      },
    },
  ));
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
