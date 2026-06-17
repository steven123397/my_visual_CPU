# myCPU 结题汇报十分钟演讲稿

> 按 PPT 页序讲，整体约 10 分钟。口播时不用逐字背，抓住每页的第一句和最后一句即可。

## 第 1 页：封面

时间：0:00 - 0:45

各位老师好，我们小组汇报的项目是 **myCPU：基于 RISC-V 的系统级模拟器设计与实现**。

这个项目最初目标是做一个 RISC-V 指令集模拟器，但结题时它已经不只是能跑几条指令的解释器，而是一套已经可运行、可调试、可验证的系统级模拟器原型。

右侧的几个 marker 是开场想强调的证据：`KMVPETDS` 来自自制内核 `kernel_alpha`，`init: starting sh` 表示 xv6 已经能进 shell，`V3OK` 来自向量 CNN workload。这说明项目已经从单条指令执行，推进到了 guest OS、真实 workload 和系统级验证。

## 第 2 页：汇报内容

时间：0:45 - 1:20

今天我按三部分讲。

第一部分是底层实现：ISA、CSR、Sv39、MMIO、执行后端、guest 和前端怎么连成一个系统。

第二部分是创新点：我们不是只做解释器，而是把项目做成了可运行、可观察、可验证、可扩展的实验平台。

第三部分是协作方式：三个人如何分工，Git 主线如何维护，以及如何靠测试门禁保证项目持续可运行。

## 第 3 页：系统总体架构

时间：1:20 - 2:10

这一页从下往上看。

底层是 Host Simulator，也就是 C++17 模拟器主体，包含译码、共享语义、functional 参考后端、pipeline 后端，以及 JIT / DBT 原型。

中间是 Platform MMIO。CPU 不是随意访问设备，而是通过统一的 AddressSpace 和 Bus 访问 RAM、UART、CLINT、PLIC、块设备和 AI accelerator。

再往上是 Guest Runtime，包括 boot、PMM、Sv39、trap、`kernel_alpha`、`interactive_os`、xv6 和 Linux-facing probe。

最上层是 Browser Lab。Node debug server 把底层状态暴露出来，前端展示 terminal、pipeline、寄存器、CSR 和设备 profile。关键点是：前端只读观察，不参与执行语义。

## 第 4 页：底层执行链路

时间：2:10 - 3:00

这一页解释一条指令在 myCPU 里怎么变成系统行为。

首先根据 PC 取指并译码，得到 opcode、寄存器编号和立即数。之后进入共享的 `InstructionSemantics`。这是项目的关键设计：ISA 语义集中在一处，而不是 functional 写一套、pipeline 再写一套。

执行时会更新 GPR、CSR、特权级和 trap 状态。如果发生访存，就继续经过 Sv39、TLB、AddressSpace 和 Bus，最后访问 RAM 或 MMIO 设备。

functional 后端负责正确性基线，pipeline 后端负责 rename、ROB、LSQ、乱序执行和提交边界观察。二者共享语义，所以后续扩展时不容易出现多套语义漂移。

## 第 5 页：底层实现重点

时间：3:00 - 4:00

这一页讲三个底层重点。

第一是 ISA 语义。项目以 RV64I / RV64M 为主体，`InstructionSemantics` 是真值来源；functional 先保证正确性，pipeline 和 JIT 原型只消费共享语义。

第二是特权与 trap。为了运行系统软件，只有整数指令不够，还要有 M / S / U 三级特权、CSR、trap delegation、`mret` 和 `sret`，这些机制决定 guest 内核如何处理中断和异常。

第三是 Sv39 与平台。三级页表、TLB、page fault 和 `sfence.vma` 让项目进入操作系统语境；UART、CLINT、PLIC 和块设备则通过统一 Bus 接入，形成 guest 能看到的硬件平台。

右侧截图说明 pipeline 不是只存在于代码里，而是能在前端观察到 stage、commit、stall 和指令流。

## 第 6 页：系统软件 bring-up

时间：4:00 - 5:00

这一页说明为什么它是系统级模拟器。

`kernel_alpha` 是自制 guest 内核，`KMVPETDS` 对应 boot、内存初始化、Sv39、PLIC、外部中断、定时器和存储路径，说明最小系统链路已经跨过去了。

`interactive_os` 证明 UART 输入输出和浏览器 terminal 可以闭环。截图里的命令输出是实时交互，不是静态 mock。

xv6-riscv 进一步证明平台不只服务自制 guest，它走真实 virtio-blk board path，并能稳定进入 shell。Linux-facing 路线则把 flat image、DTB、rootfs 和 serial console 接入进来。

这里也要保持边界诚实：Linux-facing 表示我们已经进入真实 Linux bring-up 方向，但不说完整发行版兼容已经完成。

## 第 7 页：浏览器 Lab

时间：5:00 - 5:45

系统级项目最大的问题之一是状态太多，如果没有观察面，调试和展示都会很困难。

我们的 Browser Lab 就是把底层证据投射出来。页面里有 Load、Run、Pause、Reset、Terminate 这些会话控制；中间是 terminal；旁边可以组织 pipeline、register、CSR、device counters 和 AI profile。

所以前端不是装饰，它把 marker、snapshot 和 counters 变成一个可以操作的实验台。老师不需要先读源码，也能看到系统现在运行到哪一步。

## 第 8 页：创新点总览

时间：5:45 - 6:35

我把项目创新点概括成四个词：可运行、可观察、可验证、可扩展。

可运行，是指它已经能运行自制内核、interactive monitor、xv6 和 Linux-facing probe。

可观察，是指 terminal、pipeline、寄存器、CSR 和设备计数器都能通过 debug snapshot 与前端看到。

可验证，是指我们不是只靠一次演示成功，而是有 asm、unit、host smoke、guest smoke、frontend 和 Spike differential 的多层门禁。

可扩展，则体现在 AI accelerator、Vector / ML、JIT / DBT、cache / memory-system 等方向都已经有实验入口和边界。

## 第 9 页：扩展能力

时间：6:35 - 7:25

这一页展开三个方向。

AI Accelerator 的重点是设备化：我们不是把 AI workload 写成普通函数，而是做成 MMIO 设备路径，包括 graph package、doorbell、submission queue、scratchpad、DMA 和 profile summary。

Vector / ML 的重点是 workload 化：项目不仅验证单条向量指令，还通过固定 conv 到 relu 的 CNN demo，用 `V3OK` 锁住一组算子闭环。

JIT / DBT 的重点是工程化推进：先做 hot-path、IR dry-run、lowering、host emitter 和 opt-in harness，而不是一开始就替换默认 backend。

这页也要说明边界：我们不声称完整商用 NPU，不声称默认 JIT backend 已完成，也不声称完整 Linux 发行版兼容已经完成。

## 第 10 页：验证体系

时间：7:25 - 8:25

系统模拟器不能只靠“演示时能跑”，所以我们把验证拆成四层。

第一层是 asm 和 unit 测试，锁住最小指令语义、loader、MMU、device 和 helper。

第二层是 host smoke，比如 debug CLI、pipeline、JIT、AI profile，验证宿主侧组合路径。

第三层是 guest smoke，比如 `kernel_alpha`、`interactive_os`、xv6 和 Linux-facing probe，验证 CPU、内存、设备和 guest runtime 的跨层链路。

第四层是 Spike differential。它的价值是引入外部参考实现，避免模拟器只和自己的预期互相验证。

底部这几个命令，就是我们常用的门禁：`make test`、`make test-pipeline` 和前端 `node --test`。

## 第 11 页：三人分工与 Git 协作

时间：8:25 - 9:30

这一页讲团队协作。

梁家琦主要负责总体架构、ISA / CSR / 内存 / 平台设备、guest runtime、前端调试链路、文档整理和最终联调。

杨皓宇主要负责 pipeline 和微结构，包括 branch predictor、rename、ROB、LSQ、flush / rollback 和 pipeline 可观察状态。

余健超主要负责测试与验证，包括 asm 指令级测试、unit / host / guest smoke、pipeline 回归、frontend 测试和 Spike differential。

Git 协作上，我们把 main 作为稳定主线，功能按 feature 或 wave 小步推进。每次合入前先跑对应测试，再同步文档口径。这样可以避免系统级项目在持续修改后丢掉可运行基线。

当前快照有 246 次提交，也说明项目不是最后临时拼出来的，而是在持续 checkpoint 中逐步收口的。

## 第 12 页：总结

时间：9:30 - 10:00

最后总结三点。

第一，我们完成了一套已经可运行的 RISC-V 系统模拟器原型。

第二，它不只解释指令，还能支撑特权级、虚拟内存、外设、guest OS、真实 workload 和浏览器调试。

第三，课程结题不是项目停止。后续它还可以继续沿 Linux 发行版平台、AI accelerator、JIT / DBT、cache 和多核方向演进。

所以用一句话总结，myCPU 已经从课程里的指令集模拟器，成长为一个可以继续做系统结构实验的平台。我的汇报结束，谢谢老师。
