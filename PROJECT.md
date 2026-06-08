# PROJECT.md — 51-MCU-Machine-Operation-Simulation

## 项目简介

**51-MCU-Machine-Operation-Simulation** 是一个完整的机器运动模拟管线：MATLAB 轨迹仿真 → C++ AI 路径规划 → 串口协议下发 → MCU LED 点阵执行。

## 架构设计

### 四层架构

| 层级 | 环境 | 职责 |
|------|------|------|
| 仿真层 | MATLAB | 轨迹可视化、状态向量离散化、数据导出 |
| 控制层 | C++20 (PC) | 状态向量读取、DeepSeek AI 决策、轨迹插值、协议编码、串口发送 |
| 通信层 | 串口 (9600-8N1) | 10 字节二进制协议帧 + XOR 校验 |
| 执行层 | MCU (51/STM32) | 帧解码、8×8 LED 点阵逐行扫描、急停保护 |

### 运动模式

| 模式 | 枚举值 | 触发条件（距离 d） | 生成函数 |
|------|--------|-------------------|----------|
| 直线 | `line` | d < 3 | `generateLine()` |
| 正弦 | `Sin` | 3 ≤ d < 6 | `generateSin()` |
| 圆弧 | `circle` | d ≥ 6 | `generateCircle()` |

### AI 决策策略

```
generatePathWithAI()
  ├─ 首选: DeepSeek API (需 DEEPSEEK_API_KEY)
  │   └─ 失败 → 降级
  └─ 降级: 本地 A* (8 方向, 100×100 栅格) + fakeAI 运动模式选择
```

## 协议帧格式

每帧 10 字节：
```
┌──────┬─────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ 0xAA │ type    │ x_hi     │ x_lo     │ y_hi     │ y_lo     │ reserved │
│ 帧头 │ 运动模式 │ 坐标高8位 │ 坐标低8位 │ 坐标高8位 │ 坐标低8位 │   0x00   │
├──────┼─────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ 1B   │ 1B      │ 2B (uint16, 0-65535)   │ 2B (uint16, 0-65535)   │ 1B       │
└──────┴─────────┴────────────────────────┴────────────────────────┴──────────┘
继续:
┌──────────┬──────────┬──────────┐
│ vx       │ vy       │ checksum │
│ 速度×量化│ 速度×量化│ XOR      │
│ int8     │ int8     │ 1-9 字节 │
├──────────┼──────────┼──────────┤
│ 1B       │ 1B       │ 1B       │
└──────────┴──────────┴──────────┘
```

ACK 帧（MCU → PC）：`0xCC` + `seq(1B)`

## 文件清单

### src/ — PC 控制层
| 文件 | 说明 |
|------|------|
| `main.cpp` | 主入口，串联全流程 |
| `Point.h` | 状态向量结构体 |
| `motion.h/cpp` | 三种运动轨迹生成 + MotionType 枚举 |
| `AI.h/cpp` | 简易 AI 决策 (fakeAI) |
| `DSAI.h/cpp` | DeepSeek API + A* + JSON 解析器 |
| `protocol.h/cpp` | 二进制帧编码 |
| `serial.h/cpp` | Windows 串口驱动 |

### firmware/ — MCU 固件
| 文件 | 说明 |
|------|------|
| `hal_map.h` | 硬件抽象层 (51 ↔ STM32) |
| `main.c` | 主循环 + 中断初始化 |
| `protocol.h/c` | 帧解码 + 逐字节状态机 |
| `led_matrix.h/c` | 8×8 LED 点阵动态扫描 |
| `queue.h/c` | 环形缓冲 (帧队列) |

### matlab/ — 仿真
| 文件 | 说明 |
|------|------|
| `day1_trajectory.m` | 轨迹可视化 |
| `day2_state_generation.m` | 状态向量离散化 + quiver |
| `trajectory.txt` | 预计算状态向量数据 |

### tests/ — 测试
| 文件 | 说明 |
|------|------|
| `test_motion.cpp` | 运动生成单元测试 |
| `test_protocol.cpp` | 协议编码/解码测试 |
| `mcu_simulator.py` | PC 端 MCU 模拟器 |

## 关键依赖

- **编译器**：MSVC v145+ / GCC 10+ / Clang 12+，C++20
- **MATLAB**：R2019b+（仅仿真层）
- **MCU 编译器**：SDCC (51) 或 arm-none-eabi-gcc (STM32)
- **MCU 模拟器**：Python 3 + pyserial
- **外部库**：无（PC 端纯标准库 + WinHTTP；MCU 端纯 C + HAL）

## 典型工作流

### 离线模式（无硬件）
```bash
cmake -B build && cmake --build build
./build/TrajectorySystem   # 输出轨迹点到终端
```

### 模拟模式（PC ↔ 虚拟 MCU）
```powershell
# 终端 1: MCU 模拟器
python tests/mcu_simulator.py --port COM4

# 终端 2: 主程序
$env:MCU_SERIAL_PORT = "COM3"
./build/Release/TrajectorySystem.exe
```

### 硬件模式（PC ↔ 真实 MCU）
```powershell
$env:MCU_SERIAL_PORT = "COM3"
$env:DEEPSEEK_API_KEY = "sk-..."  # 可选
./build/Release/TrajectorySystem.exe
```

## 可移植性设计

| 移植目标 | 改动范围 | 说明 |
|----------|----------|------|
| MCU: 51 → STM32 | `firmware/hal_map.h` | 修改编译宏和引脚映射 |
| PC: Windows → Linux | `src/serial.cpp` | 替换 WinAPI → termios |
| PC: DeepSeek → 其他 LLM | `src/DSAI.cpp` | 修改 API endpoint 和 JSON 解析 |
| 协议: 有线 → 无线 | `src/serial.cpp` + `firmware/` | 替换传输层，帧格式不变 |

## 当前进度

- [x] MATLAB 轨迹仿真与可视化
- [x] MATLAB 状态向量离散化
- [x] C++ 状态向量读取
- [x] C++ 运动轨迹生成（直线/正弦/圆弧）
- [x] 二进制协议帧编码/解码
- [x] MCU 固件（可移植 C，支持 51/STM32）
- [x] DeepSeek API + A* 降级
- [x] Windows 串口通信
- [x] MCU 模拟器 (Python)
- [x] CMake 跨平台构建 + CI
- [ ] 真实 51 MCU 硬件联调
- [ ] STM32 平台移植与嵌入式验证

## 许可证

待定。
