# 项目架构与工程状态

## 目标

用户已确定下一阶段方向：输入中文描述，由 AI 解析为运动计划，再交给单片机执行。第一版拟用现有 8×8 LED 点阵演示光点运动，未来机器人扩展本次不做。

负责人已同意 v0.1 方案。新入口 MotionControl 已实现计划、预览、AI 适配器及串口接口；真实 AI 和上板验收仍待完成。科研实验与论文代码在其他仓库管理，本仓库保持独立工程项目定位。

具体接口与验收见[自然语言控制 v0.1 方案](docs/natural-language-control-v0.1.md)，运行方法和证据范围见 README。模块存在不代表完整硬件链路已经验证。

## 新入口 MotionControl

src/control/main.cpp 接收文字（Windows 在线）、固定样例或计划文件。ai.cpp 构造请求并解析响应；plan.cpp 验证受限计划并生成带时间戳的点；preview.cpp 导出动画和 CSV；executor.cpp 固定工作区编码并按节拍等待 ACK；device.cpp 提供 Windows 串口及固件解码模拟。

固件 C 源码参与 PC 互操作与 LED 测试。LED 驱动改为最近格点舍入，避免整数格坐标经量化后偏移；主机测试覆盖 64 个格点。51 UART 初始化将 TI 置为初始可发送状态，读取宏换为函数；目标编译与上板仍需确认。

新 AI/串口入口不共享旧 DSAI/serial 的降级或超时路径。新请求保留 TLS 校验，设置响应上限与截止时间。--demo 与 --provider-response 明确标记为离线来源。

## 保留的旧 TrajectorySystem 数据流

1. MATLAB 脚本可生成轨迹与状态向量，仓库保留 matlab/trajectory.txt。
2. main.cpp 读取该文件到 trajectory0，并打印；没有用这些点构造当前 PathRequest。
3. main.cpp 使用写在代码中的起点与终点调用 generatePathWithAI。
4. DSAI 尝试外部规划，失败后生成本地 A* waypoints 与运动类型。
5. main.cpp 只读取 plan.motionType，按原起终点重新调用直线、正弦或圆弧生成器；plan.waypoints 没有传入执行阶段。
6. 生成的轨迹由 encodeFrames 编码。只有设置 MCU_SERIAL_PORT 时才尝试串口操作。
7. firmware 中有接收、协议、队列与 LED 模块；本轮没有验证 PC 与真实固件的互操作。

目前不能把“读取了 MATLAB”“调用了 AI”“编码成功”直接等同于“按 MATLAB/AI 路径执行”。

## 旧模块边界

| 目录/文件 | 职责 | 当前限制 |
|---|---|---|
| matlab/ | 轨迹可视化与状态向量 | 本轮未运行 MATLAB；数据尚未成为主程序决策输入 |
| src/DSAI.* | 外部规划、解析与本地 A* 降级 | 外部 API 未调用；waypoints 未被最终轨迹消费 |
| src/motion.* | 三种几何轨迹生成 | 现有测试覆盖有限，不代表物理约束或避障保证 |
| src/protocol.* | PC 二进制帧编码 | 当前测试验证编码侧；未验证真实 MCU 往返 |
| src/serial.* | PC 串口接口 | Windows 实现存在；Linux 为占位实现 |
| firmware/ | 固件协议、HAL、队列、LED | 未编译、烧录或连接硬件验证 |
| tests/mcu_simulator.py | 串口协议调试工具 | 本轮未运行；不能用单元测试替代串口验证 |

## 构建与数据

新入口、测试与依赖使用 CMake 构建，见 README.md 和 docs/control-verification-20261103.md。nlohmann/json 已固定版本保存在 third_party；VS 原工程文件只覆盖旧示例，不将其作为新入口的构建方法。历史构建记录见 docs/build-verification-20261103.md。

请在新构建目录中配置。历史受追踪缓存包含本机 G/H 盘路径，不能跨电脑直接复用。取消追踪文件不会缩减旧 Git 历史；其他电脑拉取前应将仍需保留的本地构建产物复制到仓库外。

## 获批设计与后续验收

- 自然语言作为用户输入，MATLAB 作为独立分析与对照工具。
- AI 输出受限动作计划，由本地校验器和确定性轨迹编译器生成点序列；新链路不以 fakeAI 或 A* 自动代替失败的指令理解。
- 支持直线、整圆、等待和有界重复；使用固定 LED 格坐标、50ms 节拍、默认预览与明确执行。
- JSON 库、计划契约和新入口已按获批方案实现；真实 AI 与硬件分别验收。

构建、测试与未验证项见 docs/control-verification-20261103.md。当前授权不包含自动合并。

## 变更与审批

使用独立 worktree、可恢复提交、实际测试与 Draft PR；重要架构、关键依赖、兼容性变化及合并由用户审批。日常维护必须依据真实仓库状态，用户没有回复不代表没有新想法。
