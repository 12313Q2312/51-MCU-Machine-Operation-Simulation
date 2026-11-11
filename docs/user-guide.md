# MotionControl 操作与演示

## 第一次运行

按 README 构建后运行 --help 查看参数。Windows Visual Studio 生成器输出 build/Release/MotionControl.exe，Linux 输出 build/MotionControl。下方 Windows 命令在仓库根目录执行。

```powershell
.\build\Release\MotionControl.exe --demo --simulate --output artifacts\demo-01
```

打开输出目录中的 preview.svg：蓝线先到 (2,4)，再绕 (4,4) 顺时针一圈，黄色光点按格点显示。预览为了观察循环播放，实际计划只执行一次。CSV 保留连续坐标和时间戳；run.json 应显示 source=fixture、mode=firmware-decoder-simulation、121 帧 ACK。

模拟使用真实固件协议解码函数及虚拟时钟，所以很快结束；它不测量真实串口速度、Windows 调度精度或物理显示。输出目录须尚不存在，重复演示请更换 --output。

## 编写计划或输入中文

复制 docs/natural-language-plan.example.json，按契约修改。坐标 0–7 格，动作时长为 50ms 的正整数倍；最多 16 动作、重复 10 次、总时长 60 秒。整圆前当前位置须在圆周上，重复计划须回到起点。

```powershell
.\build\Release\MotionControl.exe --plan docs\natural-language-plan.example.json --output artifacts\my-plan
```

真实 AI 需要负责人在本机设置 DEEPSEEK_API_KEY，本轮开发没有调用 API。密钥不要写入源码或公开截图。

```powershell
.\build\Release\MotionControl.exe --text "从 (0,0) 到 (7,0)，一秒，再回到 (0,0)，一秒，重复三次" --output artifacts\ai-01
```

默认模型 deepseek-flash，可加 --model NAME。程序显示 AI 说明、默认值，以及本地核对的动作列表。每次输入完整指令；“再快一点”没有历史上下文时应请求补充。格式检查不能保证模型理解正确，请核对动作和预览。source=ai 表示在线来源，不表示经过人工或硬件验收。

--provider-response FILE 可读取提供方响应的离线样例，标记为 provider-fixture，不证明在线理解效果。所有输入模式默认只预览，设置 MCU_SERIAL_PORT 也不会触发新入口发送。

## 开发板联调

真实硬件本轮尚未接入，不应仅凭 PC 测试宣称上板成功。

1. 核对 MCU、晶振、点阵驱动、电平和接线。HAL 是平台示例，不能假设适配任意成品模块；STM32 还需要其工程与 HAL 配置。
2. 51 示例假定 STC89C52、11.0592MHz、12T、SDCC。先完成目标编译和烧录，确认扫描、复位与急停输入状态；在真实板型上记录工具链与烧录步骤。
3. 串口为 9600-8N1；USB-TTL 与 MCU 的 RX/TX 交叉连接并共地，核对电平。端口名从本机设备管理器确认。
4. 每次执行前复位开发板，确保空队列。先用短计划检查四角，再测整圆、重复、断连和停止；观察结果与 CSV/预览比较。
5. 预览确认后输入 EXECUTE。失败后先查明原因并复位，不自动续传或重发。

```powershell
.\build\Release\MotionControl.exe --plan docs\natural-language-plan.example.json --execute --port COM3 --output artifacts\board-01
```

ACK 为 0xCC 加循环计数：首帧建立基线，随后检查连续性。它不是请求 ID，不能识别全部迟到回复。ACK 默认超时 1 秒；延迟超过 50ms 计划节拍则中止，不追赶积压点。该执行方式不提供硬实时保证。

Ctrl+C 停止后续发送，点阵可能保持最后一点；它不是远程急停。当前不接电机，不提供真实位置、速度或加速度反馈保证。

## 记录成果与排查

保留三组独立证据：固定计划离线演示、真实中文 AI 解析结果、真实开发板视频与日志。每组标明提交号、方式和验证范围。可展示产品目标、计划契约、轨迹生成、异常停止与 PC/固件联合验证；科研代码和实验在其他仓库维护。

退出码：0 成功/预览；2 参数、解析、API 或文件错误；3 需要补充指令；4 确认前取消；5 执行异常/中途取消。run.json 记录完整发送帧数、ACK 数与部分写入字节数，它们不替代真实显示反馈。

| 现象 | 处理 |
|---|---|
| 输出目录已存在 | 更换 --output，已有记录不会覆盖 |
| 缺少密钥 | 先用 --demo，或由负责人在本机配置密钥 |
| 越界/圆周错误 | 修改请求或计划；程序不自动裁剪 |
| ACK 超时/错序 | 检查接线、端口、固件与复位状态，不盲目重试 |
| Linux 在线或串口不可用 | 本版该平台支持离线预览和协议模拟 |
| 只有静态预览 | 下载 SVG 后用支持 SVG 动画的浏览器打开 |
