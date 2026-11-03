# 项目架构与工程状态

## 目标

将轨迹仿真、PC 决策与编码、串口通信及 MCU LED 模拟连接成可验证的运动系统。目录中存在相关模块，不代表端到端链路已经完成验证。

## 当前实际数据流

1. MATLAB 脚本可生成轨迹与状态向量，仓库保留 matlab/trajectory.txt。
2. main.cpp 读取该文件到 trajectory0，并打印；没有用这些点构造当前 PathRequest。
3. main.cpp 使用写在代码中的起点与终点调用 generatePathWithAI。
4. DSAI 尝试外部规划，失败后生成本地 A* waypoints 与运动类型。
5. main.cpp 只读取 plan.motionType，按原起终点重新调用直线、正弦或圆弧生成器；plan.waypoints 没有传入执行阶段。
6. 生成的轨迹由 encodeFrames 编码。只有设置 MCU_SERIAL_PORT 时才尝试串口操作。
7. firmware 中有接收、协议、队列与 LED 模块；本轮没有验证 PC 与真实固件的互操作。

目前不能把“读取了 MATLAB”“调用了 AI”“编码成功”直接等同于“按 MATLAB/AI 路径执行”。

## 模块边界

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

构建入口、执行命令和验证矩阵见 README.md 及 docs/build-verification-20261103.md。保留 VS 工程与 CMake 两种工程入口，未引入新的关键依赖。

请在新构建目录中配置。历史受追踪缓存包含本机 G/H 盘路径，不能跨电脑直接复用。取消追踪文件不会缩减旧 Git 历史；其他电脑拉取前应将仍需保留的本地构建产物复制到仓库外。

## 待 Tech Lead 确认

- MATLAB 是主轨迹来源、外部规划请求来源，还是独立可视化基准？
- AI/A* waypoints 应完整执行，还是只允许选择预定义轨迹模板？
- 确定职责后，定义起终点、关键路径点、速度/加速度、编码映射与失败处理的验收标准。

这些决定会影响执行接口；本轮整理未代替用户选择或改动链路。

## 变更与审批

使用独立 worktree、可恢复提交、实际测试与 Draft PR；重要架构、关键依赖、兼容性变化及合并由用户审批。日常维护必须依据真实仓库状态，用户没有回复不代表没有新想法。
