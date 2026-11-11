# 跨平台构建修复与验证 · 2026-11-03

## 问题与变更

以清理提交 `a62b303` 为基线；分支 `fix/portable-build-20261103`。仅修复可复现的编译/验证问题，无架构、接口、协议或依赖变更。

1. `src/main.cpp`：用 `_WIN32` 保护 Windows 头文件及 `system("pause")`。Windows 保留原有行为；Linux 可使用既有 AI 降级与离线编码路径。没有实现 Linux 串口驱动。
2. `tests/CMakeLists.txt`：测试目标在 MSVC 使用 `/UNDEBUG`，其他编译器使用 `-UNDEBUG`，避免 Release 的 `-DNDEBUG` 将现有 assert 检查全部移除。产品目标的 Release 优化不变。
3. 本验证报告。

取消追踪旧 build 缓存消除了绑定旧 G/H 盘路径的配置失败；之后干净 Linux 全量构建又暴露无条件 windows.h，此处修复后全量构建成功。远程历史日志已过期，不能把本地复现错误当作已读取的旧远程日志。

## 实测结果

环境：Linux / GNU C++ 13.3.0 / CMake 与 CTest 4.4.4。CMake 是工作环境工具，没有加入仓库依赖。未修改 CI 平台矩阵。

| 项目 | Debug | Release |
|---|---|---|
| CMake 干净配置 | 成功 | 成功 |
| 全量编译（主程序及两测试） | 成功 | 成功 |
| CTest test_motion | 通过 | 通过 |
| CTest test_protocol | 通过 | 通过 |
| 主程序离线 smoke | 退出码 0 | 退出码 0 |

离线 smoke 将 DEEPSEEK_API_KEY 与 MCU_SERIAL_PORT 在子进程中设为空，从构建目录运行主程序并提供一个回车。观察到既有本地 A* 降级、58 个轨迹点、58 帧/580 字节以及跳过串口发送。没有外部 API 调用或硬件操作。此结果仅证明现有离线流程可执行，不证明生成轨迹遵循 A* waypoints，也不证明 MATLAB 数据参与控制。

Release 编译命令确认测试使用 `-O3 -DNDEBUG -UNDEBUG`。另在仓库外复制 test_protocol，将第一个 `assert(v == 0)` 改成故意错误的 `assert(v == 1)`，用实际 Release 编译选项编译；进程以 SIGABRT（Python returncode -6）失败。该负对照验证断言并未被优化配置关闭；原有测试源码未被改动。

## 可复现命令

在此分支的干净 checkout 中：

```sh
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug
ctest --test-dir build/Debug --output-on-failure
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release
ctest --test-dir build/Release --output-on-failure
```

MSVC 为多配置生成器，使用 `cmake --build build --config Release` 和 `ctest --test-dir build -C Release --output-on-failure`。

## 尚未验证与发布状态

- 未实际运行 Windows/MSVC、Clang 或新的 GitHub Actions；不能称 CI 已通过。
- 未运行 MATLAB、串口模拟器、固件编译、烧录或硬件验证；Linux 串口仍是既有占位实现。
- main 的 MATLAB 输入只打印；plan.waypoints 未传给后续轨迹生成。需要用户决定“执行 AI 路径”与“MATLAB 轨迹”的关系后另开任务。
- 远程 GitHub App 建分支返回 403；此工作环境的 Git push 没有可用登录凭据。当前只有本地可审查提交，尚无远程分支或 Draft PR。GitHub main 未被修改。
- 回滚时先逆序 `git revert <此修复提交 SHA>`，再按需要 revert 清理提交；通过独立分支/PR 审批，不重写历史。

## 2026-11-10：让既有矩阵真正跑到这次修复

2026-06-09 的运行 `27190156984` 原始日志已过期（API 410）。三个矩阵任务的步骤结论仍在：GCC、Clang、MSVC 都在 Configure CMake 失败，Build 和 Test 被跳过。用当时仍被追踪的 `build/CMakeCache.txt` 复现，CMake 直接拒绝配置：缓存创建目录是 `g:/linshicunchuqu/51-MCU-Machine-Operation-Simulation/build`，源目录是 `G:/linshicunchuqu/51-MCU-Machine-Operation-Simulation`，编译器是 `H:/MSYS2/mingw64/bin/c++.exe`。这与“配置步骤在一秒内失败、三个平台一起失败”一致。

清理分支去掉这份缓存之后，PR #1 的运行 `41648971466` 可以完成配置。Linux GCC 与 Clang 的构建日志仍在，失败点是 `src/main.cpp:22` 无条件 `#include <windows.h>`。同一次运行的 Windows MSVC 配置、构建和测试已经通过。本分支原有的 `_WIN32` 保护和测试目标 `-UNDEBUG` / `/UNDEBUG` 就是这条 Linux 编译失败的修复，编译器矩阵没有删减。

本 PR 的基线是 `chore/repository-cleanup-20261103`，不是 `main`。原来的工作流只在指向 `main` 的 `pull_request` 上运行，所以这份修复从未进入 Actions。工作流现在额外接受：`push` 到 `fix/portable-build-20261103`，以及 `pull_request` 指向 `chore/repository-cleanup-20261103`。

本地按工作流命令复现（GCC 13.3，CMake 3.28，Release）：配置、全量编译、`ctest --build-config Release` 为 2/2 通过。此云端镜像的 Clang 在链接编译器探测程序时找不到 `-lstdc++`，发生在编译本仓库之前。GitHub ubuntu-latest 的 Clang 任务在 2026-11-03 已经通过编译器探测，并编译到 `windows.h`，因此没有为这个镜像差异修改项目。上面这段不代表 GitHub Actions 已经通过；以本提交触发的 Checks 为准。
