# 仓库安全清理报告 · 2026-11-03

## 范围与基线

- 仓库：`12313Q2312/51-MCU-Machine-Operation-Simulation`
- 基线：`main` / `f265ed110cb7e115c03c8479834ea0f01627124a`（2026-06-09）。
- 分支：`chore/repository-cleanup-20261103`；通过独立 Git worktree 实施。
- 审查时只有 main，未查询到 Issue 或 PR；4 次历史 Actions 运行均失败。最新运行 `27190156984` 的 GCC、Clang、MSVC 任务均在 Configure CMake 失败，Build/Test 被跳过。历史原始日志已过期（API 410），不能宣称读取过其错误详情。
- 此提交只清理追踪范围、新增有效 `.gitignore` 和本报告；不修改运行逻辑、源代码、测试、固件或工程配置。

## 清理清单

| 类别 | 原受追踪文件数 | 依据 |
|---|---:|---|
| `.vs/` | 17 | IntelliSense 数据库、索引、预编译头、窗口布局与用户状态 |
| `build/` | 33 | CMake/Ninja 缓存、编译器探测代码、目标文件、可执行文件和复制数据 |
| `TrajectorySystem/x64/` | 19 | MSBuild 对象、调试数据库、日志和增量链接产物 |
| `x64/` | 2 | 编译出的 exe 与 pdb |
| `TrajectorySystem.vcxproj.user` | 1 | 文件只有空的 PropertyGroup，无工程设置 |
| `1.gitignore` | 1 | 零字节文件，替换为真正生效的 `.gitignore` |
| **合计** | **73** | **保留全部 37 个其他原有文件** |

`build/CMakeFiles/4.2.3/CompilerIdCXX/CMakeCXXCompilerId.cpp` 是 CMake 生成的探测源码；`build/trajectory.txt` 与保留的 `matlab/trajectory.txt` 完全一致。没有仅凭扩展名删除用户源码。

72 个缓存/产物文件通过 `git rm --cached` 取消追踪；检查确认其仍存在于本次 worktree。零字节 `1.gitignore` 被替换。检查保留的 37 个文件 Git blob 哈希均与基线一致，且均不被新忽略规则匹配。新规则匹配所有 72 个取消追踪的缓存/产物。最终受追踪文件为 39 个（37 个原有资产 + `.gitignore` + 本报告）。

逻辑文件体积约 517 MB；这是从后续快照取消追踪的内容总量，不是 Git 历史压缩量。既有提交和大文件历史保留，没有强制推送、历史重写或垃圾回收。

## 保留的资产

- `src/`：12 个 PC 控制、AI 路径、运动、串口和协议源文件。
- `firmware/`：8 个固件、HAL、队列和 LED/协议文件。
- `matlab/`：2 个 MATLAB 脚本及 `trajectory.txt` 数据。
- `tests/`：测试构建文件、运动/协议测试、MCU 模拟器，共 4 个。
- `.github/workflows/cmake-multi-platform.yml`、根 `CMakeLists.txt`、VS solution/project/filter、AGENTS/CLAUDE/README/使用说明全部原样保留。`TrajectorySystem.gitignore` 曾随第一次清理保留，后续提交已把它的规则并入根 `.gitignore` 并删除该文件，见文末。

## 实际验证

环境：Linux，GNU C++ 13.3.0，CMake/CTest 4.4.4。CMake 仅安装到本次工作环境，未增加项目依赖。没有调用 DeepSeek API、串口或真实硬件。

| 验证 | 结果 |
|---|---|
| 保留 37 个文件的 Git blob 哈希 | 全部与基线一致 |
| 72 个取消追踪文件本地存在且已被忽略 | 通过 |
| 新 ignore 不误伤 37 个保留文件 | 通过 |
| 从原 clone 的受追踪 `build/` 执行 CMake 配置 | 失败：缓存源目录绑定旧 G 盘路径，编译器绑定旧 H 盘路径 |
| 从暂存区用 `git checkout-index` 导出干净快照后 Debug 配置 | 成功 |
| 干净快照完整构建 | 失败：`src/main.cpp:22` 无条件包含 `windows.h` |
| 干净快照构建 `test_motion`、`test_protocol` | 成功 |
| Debug CTest（断言启用） | 2/2 通过 |
| 直接 GNU C++ 编译并执行相同两项测试（清理前） | 2/2 通过 |

复现清理后状态：在此提交的新 clone 中执行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_motion test_protocol
ctest --test-dir build --output-on-failure
cmake --build build
```

最后一条在 Linux 的已知结果是 windows.h 编译失败；本清理不伪称完整构建或远程 CI 已通过。Windows、Clang、MATLAB、真实串口、MCU 固件编译/烧录及硬件链路本次未验证。

## 回滚与本机文件保护

- 本提交有父提交，完整保留旧文件及历史。审核后若需撤销，用 `git revert <此清理提交的完整 SHA>` 创建逆向提交，经 PR 审批合入；不使用 reset/force-push。
- 单个原有文件可由 `git restore --source=f265ed110cb7e115c03c8479834ea0f01627124a -- <具体路径>` 恢复。
- **`git rm --cached` 只保证执行它的 worktree 保留文件。其他电脑拉取/切换到清理提交时，Git 仍可能删除其原受追踪产物。** 在 Windows 本机拉取前，将仍需使用的构建目录或用户设置复制到仓库外；不要对未追踪文件运行 git clean。源码未变化，产物可重新构建。
- 当前操作位于云端独立 clone/worktree，没有访问用户的 Windows 磁盘或未追踪文件。

## 后续工作（独立变更）

1. 修复已复现的 Windows 头文件阻断，并验证干净完整构建。不要以减少 CI 平台覆盖掩盖失败。
2. 当前 Release 配置会定义 NDEBUG，而现有测试依赖 assert；应保证 CI 真正执行断言。
3. `main.cpp` 打印 AI 路径结果，但随后只按起终点与 motionType 生成轨迹，未使用 `plan.waypoints`；MATLAB 输入也只打印。需要先定义链路语义与验收标准，再修改执行路径。
4. PROJECT.md 缺失、Linux 串口为占位实现；README 应区分实现、验证与规划。
5. 每日 19:00 任务已存在且启用，保持用户明确答“没有”才进入低风险维护的规则；不重复创建提醒，也不宣称 Windows 无人值守执行器已配置。

本 PR 保持 Draft，合并由用户决定。

## 完整取消追踪/替换路径

- `.vs/TrajectorySystem.slnx/FileContentIndex/7440d36a-2c38-4267-9a83-9acc8b5f4457.vsidx`
- `.vs/TrajectorySystem.slnx/FileContentIndex/83928b5f-2121-478e-bb0e-beb56c7dda8e.vsidx`
- `.vs/TrajectorySystem.slnx/FileContentIndex/cf37ff6f-4319-4f42-8f1e-23f6d37d3e64.vsidx`
- `.vs/TrajectorySystem.slnx/FileContentIndex/e3da432f-5c85-4a83-b949-1bb6f88be7a1.vsidx`
- `.vs/TrajectorySystem.slnx/FileContentIndex/eaec1811-7b79-4a29-b2c1-5456fa27dc45.vsidx`
- `.vs/TrajectorySystem.slnx/v18/.suo`
- `.vs/TrajectorySystem.slnx/v18/Browse.VC.db`
- `.vs/TrajectorySystem.slnx/v18/DocumentLayout.backup.json`
- `.vs/TrajectorySystem.slnx/v18/DocumentLayout.json`
- `.vs/TrajectorySystem.slnx/v18/Solution.VC.db`
- `.vs/TrajectorySystem.slnx/v18/ipch/AutoPCH/2619654f61113533/MOTION.ipch`
- `.vs/TrajectorySystem.slnx/v18/ipch/AutoPCH/313673eb63953d4e/MOTION.ipch`
- `.vs/TrajectorySystem.slnx/v18/ipch/AutoPCH/67b6b951ca0bd9cf/AI.ipch`
- `.vs/TrajectorySystem.slnx/v18/ipch/AutoPCH/9109efaa458b10c0/POINT.ipch`
- `.vs/TrajectorySystem.slnx/v18/ipch/AutoPCH/aa08e559be594c6a/AI.ipch`
- `.vs/TrajectorySystem.slnx/v18/ipch/AutoPCH/d573dafd08bb0a08/TRAJECTORYSYSTEM.ipch`
- `.vs/TrajectorySystem.slnx/v18/ipch/AutoPCH/fd6eab9a7b4d69a9/MAIN.ipch`
- `1.gitignore`
- `TrajectorySystem.vcxproj.user`
- `TrajectorySystem/x64/Debug/AI.obj`
- `TrajectorySystem/x64/Debug/TrajectorySystem.Build.CppClean.log`
- `TrajectorySystem/x64/Debug/TrajectorySystem.exe.recipe`
- `TrajectorySystem/x64/Debug/TrajectorySystem.ilk`
- `TrajectorySystem/x64/Debug/TrajectorySystem.log`
- `TrajectorySystem/x64/Debug/TrajectorySystem.tlog/CL.command.1.tlog`
- `TrajectorySystem/x64/Debug/TrajectorySystem.tlog/CL.read.1.tlog`
- `TrajectorySystem/x64/Debug/TrajectorySystem.tlog/CL.write.1.tlog`
- `TrajectorySystem/x64/Debug/TrajectorySystem.tlog/Cl.items.tlog`
- `TrajectorySystem/x64/Debug/TrajectorySystem.tlog/TrajectorySystem.lastbuildstate`
- `TrajectorySystem/x64/Debug/TrajectorySystem.tlog/link.command.1.tlog`
- `TrajectorySystem/x64/Debug/TrajectorySystem.tlog/link.read.1.tlog`
- `TrajectorySystem/x64/Debug/TrajectorySystem.tlog/link.secondary.1.tlog`
- `TrajectorySystem/x64/Debug/TrajectorySystem.tlog/link.write.1.tlog`
- `TrajectorySystem/x64/Debug/TrajectorySystem.vcxproj.FileListAbsolute.txt`
- `TrajectorySystem/x64/Debug/main.obj`
- `TrajectorySystem/x64/Debug/motion.obj`
- `TrajectorySystem/x64/Debug/vc145.idb`
- `TrajectorySystem/x64/Debug/vc145.pdb`
- `build/.ninja_deps`
- `build/.ninja_log`
- `build/CMakeCache.txt`
- `build/CMakeFiles/4.2.3/CMakeCXXCompiler.cmake`
- `build/CMakeFiles/4.2.3/CMakeDetermineCompilerABI_CXX.bin`
- `build/CMakeFiles/4.2.3/CMakeRCCompiler.cmake`
- `build/CMakeFiles/4.2.3/CMakeSystem.cmake`
- `build/CMakeFiles/4.2.3/CompilerIdCXX/CMakeCXXCompilerId.cpp`
- `build/CMakeFiles/4.2.3/CompilerIdCXX/a.exe`
- `build/CMakeFiles/CMakeConfigureLog.yaml`
- `build/CMakeFiles/InstallScripts.json`
- `build/CMakeFiles/TargetDirectories.txt`
- `build/CMakeFiles/TrajectorySystem.dir/src/AI.cpp.obj`
- `build/CMakeFiles/TrajectorySystem.dir/src/DSAI.cpp.obj`
- `build/CMakeFiles/TrajectorySystem.dir/src/main.cpp.obj`
- `build/CMakeFiles/TrajectorySystem.dir/src/motion.cpp.obj`
- `build/CMakeFiles/TrajectorySystem.dir/src/protocol.cpp.obj`
- `build/CMakeFiles/TrajectorySystem.dir/src/serial.cpp.obj`
- `build/CMakeFiles/cmake.check_cache`
- `build/CMakeFiles/rules.ninja`
- `build/CTestTestfile.cmake`
- `build/TrajectorySystem.exe`
- `build/build.ninja`
- `build/cmake_install.cmake`
- `build/tests/CMakeFiles/test_motion.dir/__/src/motion.cpp.obj`
- `build/tests/CMakeFiles/test_motion.dir/test_motion.cpp.obj`
- `build/tests/CMakeFiles/test_protocol.dir/__/src/protocol.cpp.obj`
- `build/tests/CMakeFiles/test_protocol.dir/test_protocol.cpp.obj`
- `build/tests/CTestTestfile.cmake`
- `build/tests/cmake_install.cmake`
- `build/tests/test_motion.exe`
- `build/tests/test_protocol.exe`
- `build/trajectory.txt`
- `x64/Debug/TrajectorySystem.exe`
- `x64/Debug/TrajectorySystem.pdb`

## 后续：合并误命名的忽略文件（2026-11-10）

第一次清理已经删除空的 `1.gitignore`，并取消追踪 `.vs/`（17）、`build/`（33）、根 `x64/`（2）和 `TrajectorySystem/x64/`（19）。这些目录里没有需要保留的源码或实验数据：`DocumentLayout.json` 是 IDE 窗口布局；`CMakeCXXCompilerId.cpp` 是 CMake 探测代码；`build/trajectory.txt` 与 `matlab/trajectory.txt` 内容相同。

`TrajectorySystem.gitignore` 不会被 Git 读取。其中的规则已并入根 `.gitignore`。`x64/` 已覆盖 `TrajectorySystem/x64/`；这次补上原先缺失的 `*.log` 与 `*.recipe`。该误命名文件从此提交删除。最终不再追踪任何名为 `*gitignore*` 且不是根 `.gitignore` 的文件。`.vs/`、`build/`、`x64/` 仍不在索引中。

撤销这一后续提交时，在独立分支上 `git revert <本提交 SHA>`，不要改写历史。后面叠在本分支上的 PR 需要用 merge（不要 rebase）把本提交接进去，否则那些分支仍会保留 `TrajectorySystem.gitignore`。
