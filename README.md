# 51-MCU Machine Operation Simulation

结合 MATLAB 轨迹数据、C++ 运动生成、协议编码与 MCU 固件的运动模拟项目。当前可验证的入口是 PC 离线轨迹生成与编码；真实串口和 LED 硬件链路仍需单独验收。

## 工程入口

| 入口 | 内容 |
|---|---|
| [PROJECT.md](PROJECT.md) | 架构、实际数据流与待确认的接口职责 |
| [src/](src/) | 路径规划、运动、协议与串口代码 |
| [firmware/](firmware/) | MCU 协议、队列、HAL 与 LED 实现 |
| [matlab/](matlab/) | 轨迹脚本与状态向量数据 |
| [tests/](tests/) | 运动、编码测试与 MCU 模拟器 |
| [清理报告](docs/repository-cleanup-20261103.md) | 文件清理范围和回滚 |
| [构建验证](docs/build-verification-20261103.md) | 实际执行结果与未验证范围 |

## 构建与测试

需要 CMake 3.16+ 和支持 C++20 的编译器：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Linux 单配置构建：进入 build 后运行 ./TrajectorySystem。Windows Visual Studio 多配置构建：使用 cmake --build build --config Release 和 ctest --test-dir build -C Release --output-on-failure，进入 build 后运行 .\Release\TrajectorySystem.exe。

离线运行时不设置 DEEPSEEK_API_KEY 与 MCU_SERIAL_PORT。程序使用既有本地规划降级，生成并编码轨迹，跳过串口发送；末尾需要回车。MATLAB 数据由 CMake 复制到构建根目录。

## 实现与验证状态

| 项目 | 当前证据 |
|---|---|
| 直线、正弦、圆弧生成及 PC 帧编码 | 源码存在；Linux GCC Debug/Release 各 2 个单元测试通过 |
| PC 离线主程序 | 两种配置均执行成功，示例生成 58 帧 / 580 字节 |
| AI/A* waypoints 进入最终轨迹 | 尚未接入；目前仅 motionType 参与选择轨迹生成器 |
| MATLAB 状态向量进入控制决策 | 尚未接入；目前读取并打印 |
| Windows / Clang / 新远程 CI | 本轮未实际运行 |
| Linux 串口 | 既有占位实现 |
| MATLAB 执行、固件烧录、真实 MCU 与 LED | 本轮未验证 |

构建通过不代表完整控制链路正确。下一项工程决策是明确 MATLAB 输入与 AI 路径的职责，再设计链路验收。所有更新通过独立分支和人工审核，不自动合并。
