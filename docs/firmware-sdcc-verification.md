# 8051 固件目标编译验证（v0.2）

## 目的与边界

本验证的 v0.2 阶段只回答：`firmware/` 中现有 STC89C52 / 8051 源码，能否经 SDCC 编译、链接，生成结构有效且非空的 Intel HEX 固件文件。

它**不**证明开发板烧录、引脚接线、时序、RAM/Flash 资源满足实际板型、UART 通信、LED 显示或急停行为正确。没有真实烧录、设备交互或网络 API 调用。

v0.2 初始版本没有修改固件源码。经负责人进一步批准，v0.2.1 仅将 \`firmware/queue.h\` 的缓存上限从 32 帧减为 8 帧，并增加主机端队列行为测试。未更改协议、管脚、时序、固件构建参数、主机串口逻辑或默认分支。

## 自动化验证

工作流：[`8051 firmware SDCC verification`](../.github/workflows/firmware-sdcc.yml)，对主线推送、所有 PR 和手动触发执行；只读仓库权限。

1. Ubuntu 24.04 上从发行版仓库安装 `sdcc` 和 `make`，并打印实际 SDCC 版本；安装路径的版本并未锁定，若需要可复现工具链镜像，应另行评审固定版本。
2. 运行 Python 标准库测试，检查 Intel HEX 校验器能够识别合法记录及损坏记录。
3. 执行 `make -C firmware PLATFORM=8051`，编译和链接已有四个 C 文件，使用现有 `packihx` 生成 `firmware/build/firmware.hex`。
4. 检查 IHX/HEX 文件非空，校验 HEX 行长度、校验和、记录类型、EOF 和存在实际数据。
5. 保存编译日志（Actions job logs），上传 `firmware/build/` 产物及 `build-evidence.txt`。该文件区分 PR head SHA 与实际 checkout SHA，记录 SDCC 版本、HEX/IHX SHA-256。若 SDCC 生成 `.mem` 或 `.map`，也随产物上传以供后续人工评估资源占用。

CI 失败视为**需要排查的目标编译证据**，不是为了显示绿色而改变固件行为。

## 本地复现

在干净的 Linux 克隆或独立 Git worktree 中，安装 SDCC、GNU Make 和 Python 3 后执行：

```sh
sdcc --version
make -C firmware PLATFORM=8051
python3 -m unittest discover -s tests -p 'test_intel_hex_validator.py' -v
python3 scripts/validate_intel_hex.py firmware/build/firmware.hex
sha256sum firmware/build/firmware.ihx firmware/build/firmware.hex
```

已有本地 `firmware/build/` 文件时，先保护可能有价值的未追踪产物；不要在不清楚内容时运行 `make clean`。在全新 checkout 运行可排除旧目标文件掩盖编译问题。

## 验收与后续

- [ ] GitHub Actions 针对本 PR 实际 head SHA 成功完成 SDCC 编译与 HEX 校验。
- [ ] Artifact 中有非空 HEX、IHX、SDCC 版本、commit SHA、SHA-256，必要时包含资源报告。
- [ ] 人工确认资源报告与所选开发板规格匹配；此项不由脚本自动宣称完成。
- [ ] 真实开发板编译参数、烧录、串口 ACK 和物理 LED 联调另立工单，由负责人审批后实施。

状态以对应运行的实际 job 日志与 head SHA 为准。本文件中的验收勾选框不代表已执行。


## v0.2.1：8051 内部 RAM 占用优化与回归（2026-11-16）

### 触发证据

首次目标编译对应 [Actions #43781317339](https://github.com/12313Q2312/51-MCU-Machine-Operation-Simulation/actions/runs/43781317339)、提交 `ef38a60a76dbd25a1a60f641e76df3e2ecbe7ecc`。Ubuntu 24.04 上 SDCC 4.2.0 成功编译四个 C 文件，在链接阶段报告：
`?ASlink-Error-Could not get 239 consecutive bytes in internal RAM for area DSEG.`
未生成最终 HEX；PC 三平台构建在 [Actions #43781317246](https://github.com/12313Q2312/51-MCU-Machine-Operation-Simulation/actions/runs/43781317246) 上通过。

`firmware/main.c` 静态持有 `FrameQueue`；原先配置为 32 个 `FrameData`，结构体的实际大小由目标编译器决定，按字段共约 7 字节，原缓存约 224 字节。STC89C52 是 RAM 紧张的 8051 目标。减为 8 帧可降低缓存静态占用约 168 字节（字段大小不变时）；并不据此推定其他数据区、栈或 RAM 余量足够。

PC 端新 MotionControl 执行器按每帧 ACK 后才发送下一帧，因此对当前 PC 发送模式，一次最多缓存 8 帧仍留有缓冲余量；但外部发送器若持续突发超过 8 帧，队列可能溢出。当前固件队列满时点亮错误灯，不对未入队的帧发送 ACK。缓冲容量改变属于兼容性影响，不能宣称所有旧发送器均兼容。

### 本轮改动与测试

- `firmware/queue.h`：容量 32 → 8，不修改队列算法、帧结构及任何串口/LED 逻辑。
- `tests/test_firmware_queue.c`：直接链接现有 `firmware/queue.c`，验证容量边界、满载拒收且不覆盖旧帧、FIFO、环形回绕、重复进出队，使用显式失败返回值，不依赖 Release 断言。
- `tests/CMakeLists.txt`：将 `test_firmware_queue` 纳入现有 Windows/MSVC 和 Linux/GCC/Clang CTest。
- 继续由 SDCC CI 判断是否真的能编译、链接、生成有效 Intel HEX；通过后的完整 SHA、工具链与内存报告必须用实际运行证据填写，不能仅由此文档推定。

**待人工验收：** 8051 片上 RAM 用量、真实板型、电气接口、烧录、串口 ACK 与 LED 实物行为。尤其不能用缩小缓冲来掩盖连续输入时的丢帧风险。

**恢复：** 撤销本次 `queue.h` 容量修改及配套测试提交即可恢复 32 帧；不修改或强推 `main`。全部改动继续留在 Draft PR #7，由负责人审批合并。


### 内存映射复核与第二步优化

8 帧版本对应 [Actions #43789664328](https://github.com/12313Q2312/51-MCU-Machine-Operation-Simulation/actions/runs/43789664328)、提交 `ba767a176780baa4d1adf7da8ae07f7a88675507`：仍在链接阶段失败，缺少 26 和 24 字节的 DSEG 连续空间。失败工作流已上传 `.map` 与 `.mem` 中间产物；在 `main.asm` 中 `_frame_queue` 的静态占用为 **59 字节**（含 8 个帧元素及索引），因此只减小容量仍不足以释放所有直接寻址 RAM。

**修复策略：** 在 `MCU_8051`（当前 SDCC/STC89C52 目标）下，将唯一的全局 `frame_queue` 声明为 `__idata`，使用 8052 兼容芯片上高 128 字节、仅可间接寻址的内部 RAM；其余 Keil/STM32 分支继续沿用原声明。队列仍为 8 帧，算法、字节格式及 UART/LED 引脚不变。此改动假定目标确有完整的 256 字节内部 RAM；**不适用于仅有 128 字节 RAM 的老式 8051**。

后续验收必须检查新 SDCC 链接日志、`firmware.map` 的 IDATA/栈布局以及非空 HEX 校验，同时保留 PC 端队列和三平台回归。编译通过也不能替代 ISR/主循环组合的实际栈深度测量，更不能证明真实硬件上板正确。


## v0.2.1 成功验证记录（仅目标构建）

目标源码提交 `21975ed2aea42b9028e411b8eaa59a9fd98de42a`，其 [SDCC CI #43790070145](https://github.com/12313Q2312/51-MCU-Machine-Operation-Simulation/actions/runs/43790070145) 的固件构建、HEX 校验、自检、Artifact 上传全部成功。[原有 PC 三平台 CI #43790070109](https://github.com/12313Q2312/51-MCU-Machine-Operation-Simulation/actions/runs/43790070109) 的 Windows/MSVC、Linux/GCC、Linux/Clang 三项全部成功，新增队列边界测试随 CTest 执行。以上运行对应的 PR head SHA 为 `21975ed2aea42b9028e411b8eaa59a9fd98de42a`；PR 后续文档提交仍需单独核对新 SHA。

通过运行 Artifact 读取的 `firmware.map`/`firmware.mem`：

| 内容 | 实际值 | 工程含义 |
|---|---|---|
| SDCC | 4.2.0, Ubuntu 24.04 | 工具链版本已经记录，发行版包未锁定 |
| DSEG | 127 B / 128 B | 直接寻址 RAM 极为紧张，后续新增全局变量容易再次溢出 |
| ISEG | 59 B, `_frame_queue` | 8 帧队列确实从 DSEG 进入间接寻址内部 RAM |
| 栈可用区间 | 起点 `0xA4`，余量 92 B | **静态链接器布局**，不代表 ISR/嵌套调用时实际安全余量 |
| ROM 使用 | `0x0992` 字节（2451 B） | 仅为生成的镜像大小统计 |
| Intel HEX SHA-256 | `e9de05040bcf686e91cdfb489a963b97262f2616d38561064a3aabe53cc29ca9` | Artifact 所附 HEX 的可核验摘要 |
| IHX SHA-256 | `5aa1f262f69bc455dfea7b9b92b764df9e1cd75d2267a52b00f8b0069f875e07` | 原始目标输出摘要 |

**残余风险：** 这里使用的目标为有高 128 字节间接寻址内部 RAM 的 STC89C52（8052 兼容）。不能直接用该固件替换到只有 128 字节内部 RAM 的 8051。DSEG 已占 127 B，仍需后续优化以留出增长空间。没有真实硬件，无法证明最高栈深度、2ms LED ISR、串口输入密集情形和硬件急停时序。硬件验证要独立获批；不能把此 CI 的成功称为上板验收成功。

**恢复说明：** Draft PR #7 尚未合并。若撤回本轮内存布局变更，可在分支上 revert `21975ed2...`（把队列从 IDATA 移回 DSEG，固件目标编译会重新失败）；若彻底撤回本轮，可关闭 PR #7，不影响 `main`。

## P2 收尾时的再次链接（2026-11-18，北京时间）

上一节的 2451 字节和两条 SHA-256 只属于提交 `21975ed2` 的那次 CI，不是当前源码。协议解析补上未知运动类型拒绝和校验失败重同步之后，本机 SDCC 4.2.0 重新链接：DSEG 仍是 127 字节、空洞仍是 1 字节，ISEG 仍是 59 字节，栈起点仍是 `0xA4`、空区间仍是 92 字节；CODE 变为 2622 字节。调用链粗估和“未在硬件上执行”的测法写在 [硬件验收清单](HARDWARE_ACCEPTANCE.md)。不要把这次链接成功写成上板通过。
