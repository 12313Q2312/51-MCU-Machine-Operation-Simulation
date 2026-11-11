#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
flash_mcu.py — 51 MCU 固件烧录辅助工具

用法:
  python scripts/flash_mcu.py --list-ports
  python scripts/flash_mcu.py firmware/build/firmware.hex --port COM3 --tool stcgal
  python scripts/flash_mcu.py firmware/build/firmware.hex --port COM3
  python scripts/flash_mcu.py --keil-guide
"""

import argparse
import os
import subprocess
import sys

# 强制 UTF-8 输出 (Windows GBK 兼容)
if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

try:
    import serial
    import serial.tools.list_ports
    HAS_PYSERIAL = True
except ImportError:
    HAS_PYSERIAL = False


def list_serial_ports():
    if not HAS_PYSERIAL:
        print("[ERROR] 需要 pyserial: pip install pyserial")
        return []
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("[INFO] 未检测到串口设备")
        return []
    print("\n可用串口:")
    print("-" * 60)
    for port in ports:
        print(f"  {port.device}  -  {port.description}")
    print("-" * 60)
    return [p.device for p in ports]


def flash_stcgal(hex_file, port, baud=115200):
    try:
        import stcgal
    except ImportError:
        print("[ERROR] stcgal 未安装. 运行: pip install stcgal")
        return False
    print(f"\n[烧录] 目标: {port}, 文件: {hex_file}")
    print("[烧录] 请确保 MCU 已断电, 按 Enter 开始烧录...")
    input()
    cmd = [sys.executable, "-m", "stcgal", "-P", port, "-b", str(baud), "-p", "none", hex_file]
    print(f"[烧录] 运行: {' '.join(cmd)}")
    print("[烧录] 现在请给 MCU 上电 (冷启动)...")
    result = subprocess.run(cmd, capture_output=False)
    if result.returncode == 0:
        print("\n[烧录] 完成! MCU 已编程.")
        return True
    else:
        print(f"\n[烧录] 失败 (exit code {result.returncode})")
        return False


def flash_manual_stc_isp(hex_file, port):
    abs_path = os.path.abspath(hex_file)
    print(f"""
========================================
  STC-ISP 手动烧录说明
========================================

1. 打开 STC-ISP 软件 (stc-isp-15xx-v6.xx.exe)
2. 选择 MCU 型号 (如 STC89C52RC)
3. 点击 "打开程序文件" -> 选择:
   {abs_path}
4. 串口号选择: {port}
5. 点击 "下载/编程"
6. 给 MCU 断电再上电 (冷启动)
7. 等待烧录完成

========================================
""")


def show_keil_build_guide():
    print("""
========================================
  Keil uVision 4 编译与烧录指引
========================================

前提: 安装 Keil uVision 4 (C51)

----------------------------------------
  A. 创建工程
----------------------------------------

1. 打开 Keil uVision 4
2. Project -> New uVision Project...
   - 选择目录: firmware/keil_project/
   - 芯片型号: Atmel -> AT89C52
     (或 STC -> STC89C52RC, 需先安装 STC 器件库)

3. 添加源文件:
   - 右键 Source Group 1 -> Add Files to Group
   - 添加 firmware/ 下的 4 个文件:
       main.c
       protocol.c
       led_matrix.c
       queue.c

----------------------------------------
  B. 工程配置
----------------------------------------

右键 Target 1 -> Options for Target:

  [Target] 标签:
    Xtal (MHz): 11.0592

  [Output] 标签:
    勾选 "Create HEX File"

  [C51] 标签:
    Preprocessor Symbols -> Define: MCU_KEIL

  [Debug] 标签:
    不需要调试, 可以取消勾选

----------------------------------------
  C. 编译
----------------------------------------

Project -> Build Target (F7)
-> 0 Error(s), 0 Warning(s)
-> 生成 .hex 文件在工程目录下

----------------------------------------
  D. 烧录到单片机
----------------------------------------

接线:
  USB-TTL TX  -> MCU P3.0 (RX)
  USB-TTL RX  -> MCU P3.1 (TX)
  USB-TTL GND -> MCU GND

烧录 (任选一种):

  方式1: STC-ISP (推荐)
    1. 打开 STC-ISP.exe
    2. 选择 MCU 型号
    3. 加载编译好的 .hex 文件
    4. 点击 "下载/编程"
    5. MCU 断电再上电

  方式2: stcgal (命令行)
    pip install stcgal
    python scripts/flash_mcu.py <你的.hex文件> --port COM3 --tool stcgal

========================================
""")


def main():
    parser = argparse.ArgumentParser(
        description="51 MCU 固件烧录工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python scripts/flash_mcu.py --keil-guide      Keil 编译指引
  python scripts/flash_mcu.py --list-ports       列出串口
  python scripts/flash_mcu.py fw.hex --port COM3 --tool stcgal
        """
    )
    parser.add_argument("hex_file", nargs="?", help="固件 .hex 文件路径")
    parser.add_argument("--port", default="COM3", help="串口号")
    parser.add_argument("--tool", choices=["stcgal", "stcisp"], default="stcisp")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--list-ports", action="store_true", help="列出可用串口")
    parser.add_argument("--keil-guide", action="store_true", help="Keil uVision 4 编译烧录指引")

    args = parser.parse_args()

    if args.keil_guide:
        show_keil_build_guide()
        return

    if args.list_ports:
        list_serial_ports()
        return

    if not args.hex_file:
        parser.print_help()
        print("\n[HINT] 查看 Keil 编译指引: python scripts/flash_mcu.py --keil-guide")
        sys.exit(1)

    if not os.path.exists(args.hex_file):
        print(f"[ERROR] 文件不存在: {args.hex_file}")
        print("\n编译固件:")
        print("  SDCC: cd firmware && make PLATFORM=8051")
        print("  Keil: python scripts/flash_mcu.py --keil-guide")
        sys.exit(1)

    list_serial_ports()

    if args.tool == "stcgal":
        flash_stcgal(args.hex_file, args.port, args.baud)
    else:
        flash_manual_stc_isp(args.hex_file, args.port)


if __name__ == "__main__":
    main()
