# MotionControl v0.1 实现与验证记录

日期：2026-11-03。负责人确认按自然语言控制方向制作独立工程成果，科研代码另放仓库。本轮在独立 feat/natural-language-motion-v1 分支实施，不合并、不改写主分支。

## 实现

- 新 MotionControl C++20 入口：中文文字请求、提供方响应解析、严格动作计划、确定性轨迹、CSV/SVG 预览、Windows 串口与协议模拟执行。
- 仅支持直线、整圆、等待、有界重复。完整验证后才生成执行帧，默认预览；--execute、--port 及输入 EXECUTE 才打开端口。
- AI 适配器使用 WinHTTP 异步完成通知、60 秒请求截止时间、正常 TLS 验证、禁止重定向、64KiB 响应上限。默认模型依据当日官方文档设为 deepseek-flash；--model 可指定。
- 新模式采用固定格坐标与零速度字段，保留旧示例及旧自动量化入口。MCU 10 字节帧布局未改变。
- 固件 LED 映射改为最近格点舍入，解决整数格经 16 位量化再向下取整导致偏移的问题。51 TI 初始化避免首次 ACK 死等；UART 读取不再使用 GNU 语句表达式。硬件相关修正仍需目标工具链及上板验证。
- nlohmann/json 3.12.0 单头文件和 MIT 许可证已附带。文件与上游提交 55f93686c01528224f448c19128836e7df245f72 的 Git blob 完全一致，SHA-256 记录在 manifest.json，CLI 测试也核验摘要。

## 本地验证

环境：Linux，GCC 13.3.0，CMake 4.4.4，Python 3.12。CMake 与 SVG 渲染工具只安装在本次执行环境，CairoSVG 不是产品运行依赖。

```sh
cmake -S . -B build/control-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/control-debug -j 4
ctest --test-dir build/control-debug --output-on-failure
cmake -S . -B build/control-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/control-release -j 4
ctest --test-dir build/control-release --output-on-failure
```

实际命令使用安装位置 /root/.local/bin/cmake 和同目录 ctest。Debug 与 Release 均全量构建成功，各 **4/4 CTest 通过**。新核心测试使用显式检查并返回失败码，不依赖 Release 会删除的 assert；当前有 **1011 项检查**，不是 1011 个独立测试用例。

| 检查 | 结果与范围 |
|---|---|
| 原 test_motion / test_protocol | 保留并通过；断言保持启用 |
| 新 test_motion_control | 计划/解析、整圆四分点、重复、等待、错误输入、提供方包解析、真实 C 解码、LED 格点、执行异常 |
| 全部 64 个 LED 格点 | 编码、真实固件解码、真实 LED 驱动及行扫描捕获联合检查通过 |
| 假设备执行 | 正确节拍、ACK 计数回绕、超时、持续噪声、错序、部分写入、落后中止、取消和设备异常检查通过 |
| 新 test_control_cli | 输出文件、SVG XML、中文路径、离线来源、损坏计划、澄清、缺密钥、参数错误和执行前取消通过 |
| 固定计划演示 | 6000ms、121 点、121 帧、1210 字节，模拟 ACK=121；没有真实网络或串口 |
| SVG 预览 | XML 解析通过，渲染为 PNG 并人工检查布局；动画关键帧用固件相同的格点映射 |

示例运行：

```sh
build/control-debug/MotionControl --demo --simulate --output artifacts/first-demo
```

其 preview.svg 保存为 docs/demo-preview.svg；此文件明确是固定计划预览。源计划与 CLI 都在仓库，产物可重新生成。

## 远程与尚未验证项

工作流现对所有目标分支的 PR 运行 Windows/MSVC、Linux/GCC、Linux/Clang，成功时上传程序、计划和离线演示。远程结果需检查实施 PR 的具体 head SHA，不能把本地结果当成远程通过。

本轮没有使用真实 DeepSeek 密钥或调用 API；在线模型理解能力、网络超时及 Windows 网络回调的实际运行仍需单独验证。没有连接串口、烧录开发板、验证物理 LED、电机或机器人。模拟 ACK 不代表物理位置反馈。

执行环境无 SDCC，常规安装因权限限制失败，因此未完成 51 目标编译。真实板型、接线、HAL 与烧录步骤需要设备联调时记录；STM32 示例也不属于本轮验收范围。

## 恢复与合并

源码、依赖与文档均在 Git 提交中，可撤销实施提交恢复到设计分支。生成数据位于忽略目录 artifacts，不覆盖已有运行目录。所有工作保留为 Draft PR，由负责人决定合并。主分支与其他仓库没有改动。
