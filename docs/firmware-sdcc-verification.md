# 8051 固件目标编译验证（v0.2）

## 目的与边界

本验证只回答：`firmware/` 中现有 STC89C52 / 8051 源码，能否经 SDCC 编译、链接，生成结构有效且非空的 Intel HEX 固件文件。

它**不**证明开发板烧录、引脚接线、时序、RAM/Flash 资源满足实际板型、UART 通信、LED 显示或急停行为正确。没有真实烧录、设备交互或网络 API 调用。

此阶段**不修改**固件源码、协议、管脚定义、编译参数、主机程序或默认分支。

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
