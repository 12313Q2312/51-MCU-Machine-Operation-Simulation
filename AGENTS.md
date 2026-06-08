# AGENTS.md — 51-MCU-Machine-Operation-Simulation

> 详细项目文档见 PROJECT.md，开始任务前请先阅读。

## 项目概述
本项目通过 MATLAB 仿真生成轨迹（直线/正弦/圆），将轨迹离散化为状态向量（位置 + 速度），再用 C++ 进行 AI 运动决策与轨迹处理，通过串口协议下发到 MCU（51/STM32）驱动 LED 点阵模拟机器运动。

## 技术栈
- **PC 端**：C++20（MSVC / GCC / Clang），CMake 构建
- **仿真层**：MATLAB (.m) — 轨迹可视化与状态向量生成
- **AI 层**：DeepSeek API（WinHTTP）+ 本地 A* 降级
- **通信**：10 字节二进制协议帧 + XOR 校验，串口（9600-8N1）
- **MCU**：C（可移植 HAL，支持 51/SDCC 和 STM32/HAL）

## 项目结构
```
├── src/                       # C++ PC 控制层
│   ├── main.cpp               # 入口：读取 → AI → 轨迹 → 编码 → 串口
│   ├── Point.h                # 状态结构体：x, y, vx, vy
│   ├── motion.h / motion.cpp  # 运动生成：Line / Sin / Circle
│   ├── AI.h / AI.cpp          # fakeAI 降级决策
│   ├── DSAI.h / DSAI.cpp      # DeepSeek API + A* 降级
│   ├── protocol.h / protocol.cpp  # 二进制协议帧编码
│   └── serial.h / serial.cpp  # 串口驱动 (Windows API)
│
├── firmware/                  # MCU 固件 (可移植 C)
│   ├── hal_map.h              # 硬件抽象层 (51 ↔ STM32 切换)
│   ├── main.c                 # 主循环：接收 → 解码 → LED
│   ├── protocol.h / protocol.c   # 帧解码 + 状态机
│   ├── led_matrix.h / led_matrix.c # 8x8 LED 驱动
│   └── queue.h / queue.c      # 环形缓冲
│
├── matlab/                    # MATLAB 仿真
│   ├── day1_trajectory.m      # 轨迹可视化
│   ├── day2_state_generation.m # 状态向量离散化
│   └── trajectory.txt         # 预计算状态向量
│
├── tests/                     # 单元测试
│   ├── test_motion.cpp
│   ├── test_protocol.cpp
│   └── mcu_simulator.py       # MCU 模拟器 (协议调试)
│
├── CMakeLists.txt
├── TrajectorySystem.vcxproj   # VS2022 工程
└── .github/workflows/         # CMake 多平台 CI
```

## 编码规范

### C++ (src/)
- C++20，`using namespace std;` 允许
- 结构体 `struct`，枚举 `enum`，函数小驼峰
- 调试输出 `std::cout << "[DEBUG ...]"` 前缀
- 缩进：tab

### C (firmware/)
- C99，无动态内存分配
- `uint8_t`/`uint16_t` 精确宽度类型
- 硬件操作通过 `HAL_*` 宏抽象，禁止直接写寄存器
- 中断回调不执行耗时操作，仅入队

### MATLAB (matlab/)
- `clc; clear; close all;` 头部
- `%%` 分节

## 构建与运行

### PC 端
```bash
cmake -B build && cmake --build build --config Release
./build/Release/TrajectorySystem.exe
```

### 测试
```bash
ctest --test-dir build
```

### MCU 模拟器
```bash
pip install pyserial
python tests/mcu_simulator.py --port COM4
```

### 完整链路
```powershell
# 1. MATLAB 生成数据
#    运行 day2_state_generation.m → 生成 matlab/trajectory.txt

# 2. 启动 MCU 模拟器
start python tests/mcu_simulator.py --port COM4

# 3. 运行主程序 (设置 API Key 可选)
$env:DEEPSEEK_API_KEY = "sk-..."
$env:MCU_SERIAL_PORT = "COM3"
.\build\Release\TrajectorySystem.exe
```

## 数据流
```
MATLAB (.m) → trajectory.txt
                  ↓
src/main.cpp  ← DSAI (DeepSeek / A*)
  ├─ 轨迹生成 (Line/Sin/Circle)
  ├─ protocol 编码 (10B 帧)
  └─ serial 发送 → COM3
                  ↓
firmware/main.c (MCU)
  ├─ UART RX → protocol 解码
  ├─ queue 缓冲
  └─ LED 点阵显示
```

## 可移植性
- **PC → MCU 协议**：10 字节帧格式固定，两端共享 `FRAME_HEADER=0xAA, FRAME_SIZE=10`
- **MCU 平台切换**：修改 `firmware/hal_map.h` 中的编译宏 (`-DMCU_8051` / `-DMCU_STM32`)
- **串口 (PC 端)**：`serial.h` 使用 Windows API，移植 Linux 时替换为 termios
- **AI 层**：`DSAI.cpp` 的非 WinHTTP 路径返回 `nullopt`，自动降级 A*

## 注意事项
- `trajectory.txt` 需在运行目录，CMake 自动复制；VS 需手动设置
- `generateCircle` 有 100k 次迭代安全阀
- `day2_state_generation.m` 用 `diff()` 计算速度，状态向量比原始点少 1
- DeepSeek API 需 `DEEPSEEK_API_KEY` 环境变量，未设置则自动降级
