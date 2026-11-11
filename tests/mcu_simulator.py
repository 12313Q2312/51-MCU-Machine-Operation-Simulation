#!/usr/bin/env python3
"""
mcu_simulator.py — MCU 端模拟器 (用于协议调试, 无需真实硬件)

模拟 51 MCU 行为:
  1. 打开虚拟串口对 (com0com 或 socat)
  2. 接收 PC 发来的二进制帧
  3. 解码并验证校验和
  4. 发送 ACK (0xCC + seq)
  5. 在 ASCII 终端上渲染 8x8 LED 点阵

坐标映射: 与 firmware/led_matrix.c 完全一致
  - 量化范围: 0-65535 (uint16)
  - 映射到 8x8: 四舍五入到最近格点，row 翻转

用法:
  python mcu_simulator.py [--port COM4] [--baud 9600]

依赖: pyserial (pip install pyserial)
"""
import argparse
import struct
import sys
import time

try:
    import serial
except ImportError:
    print("[ERROR] 需要 pyserial: pip install pyserial")
    sys.exit(1)

FRAME_HEADER = 0xAA
FRAME_SIZE = 10
MATRIX_ROWS = 8
MATRIX_COLS = 8
X_MIN, X_MAX = 0, 65535
Y_MIN, Y_MAX = 0, 65535


def decode_frame(frame: bytes):
    """解码 10 字节帧, 失败返回 None
    与 firmware/protocol.c 保持一致:
      [0]=0xAA [1]=motion [2-3]=x [4-5]=y [6]=vx [7]=vy [8]=reserved [9]=checksum
    """
    if len(frame) != FRAME_SIZE or frame[0] != FRAME_HEADER:
        return None
    # XOR 校验 (与 MCU protocol_decode 一致)
    cs = 0
    for b in frame[:FRAME_SIZE - 1]:
        cs ^= b
    if cs != frame[FRAME_SIZE - 1]:
        return None
    motion_type = frame[1]
    x = (frame[2] << 8) | frame[3]
    y = (frame[4] << 8) | frame[5]
    vx = struct.unpack('b', frame[6:7])[0]
    vy = struct.unpack('b', frame[7:8])[0]
    return {'motion': motion_type, 'x': x, 'y': y, 'vx': vx, 'vy': vy}


def map_to_grid(qx, qy):
    """将量化坐标 (0-65535) 映射到 8x8 网格
    与 firmware/led_matrix.c::led_plot_point() 完全一致:
      col = round((qx - x_min) * (COLS-1) / (x_max - x_min))
      row = (ROWS-1) - round((qy - y_min) * (ROWS-1) / (y_max - y_min))
    """
    if X_MAX > X_MIN:
        col = ((qx - X_MIN) * (MATRIX_COLS - 1) + (X_MAX - X_MIN) // 2) // (X_MAX - X_MIN)
    else:
        col = 0
    if col >= MATRIX_COLS:
        col = MATRIX_COLS - 1

    if Y_MAX > Y_MIN:
        row = ((qy - Y_MIN) * (MATRIX_ROWS - 1) + (Y_MAX - Y_MIN) // 2) // (Y_MAX - Y_MIN)
    else:
        row = 0
    if row >= MATRIX_ROWS:
        row = MATRIX_ROWS - 1

    # Y 轴翻转 (屏幕坐标系 Y 向下)
    row = (MATRIX_ROWS - 1) - row

    return col, row


def render_led(qx, qy):
    """在终端渲染 8x8 LED 点阵, 显示当前帧位置"""
    col, row = map_to_grid(qx, qy)

    print("\033[2J\033[H")  # 清屏
    print("  MCU Simulator — 8x8 LED Matrix")
    print("  ┌────────┐")
    for r in range(MATRIX_ROWS):
        line = "  │"
        for c in range(MATRIX_COLS):
            if r == row and c == col:
                line += "\033[1;33m●\033[0m"  # 黄色高亮当前位置
            else:
                line += "·"
        line += "│"
        # 右侧标注坐标轴
        if r == 0:
            line += f"  Y: {qy:5d}/65535"
        elif r == MATRIX_ROWS - 1:
            line += f"  X: {qx:5d}/65535"
        print(line)
    print("  └────────┘")
    print(f"  Grid: col={col} row={row}")


def main():
    parser = argparse.ArgumentParser(description="MCU Simulator — 51 MCU 固件模拟")
    parser.add_argument("--port", default="COM4", help="串口号 (默认 COM4)")
    parser.add_argument("--baud", type=int, default=9600, help="波特率 (默认 9600)")
    args = parser.parse_args()

    print(f"[MCU Sim] Opening {args.port} @ {args.baud} baud")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.5)
    except serial.SerialException as e:
        print(f"[ERROR] 无法打开串口: {e}")
        print("[HINT] 可用端口:")
        from serial.tools import list_ports
        for p in list_ports.comports():
            print(f"       {p.device} - {p.description}")
        sys.exit(1)

    buf = bytearray()
    frame_seq = 0
    dropped = 0

    print(f"[MCU Sim] Listening on {args.port}... Ctrl+C to exit")
    print(f"[MCU Sim] Protocol: {FRAME_SIZE}B frames, header=0x{FRAME_HEADER:02X}, XOR checksum")
    print("[MCU Sim] Waiting for frames from PC...")
    print()

    try:
        while True:
            if ser.in_waiting:
                byte = ser.read(1)
                buf.append(byte[0])

                # 搜索帧头 (丢弃帧头前的杂散字节)
                skipped = 0
                while len(buf) > 0 and buf[0] != FRAME_HEADER:
                    buf.pop(0)
                    skipped += 1
                if skipped > 0:
                    dropped += skipped
                    print(f"[WARN] Skipped {skipped} bytes (sync lost), total dropped={dropped}")

                # 完整帧?
                if len(buf) >= FRAME_SIZE:
                    frame_bytes = bytes(buf[:FRAME_SIZE])
                    data = decode_frame(frame_bytes)
                    if data:
                        frame_seq += 1
                        motion_names = {0: "LINE", 1: "CIRCLE", 2: "SIN"}
                        mn = motion_names.get(data['motion'], f"0x{data['motion']:02X}")
                        print(f"\n[Frame {frame_seq}] {mn} "
                              f"pos=({data['x']},{data['y']}) "
                              f"vel=({data['vx']:+d},{data['vy']:+d})")
                        render_led(data['x'], data['y'])

                        # 发送 ACK (与 MCU firmware 一致: 0xCC + seq)
                        ack = bytes([0xCC, frame_seq & 0xFF])
                        ser.write(ack)
                        print(f"[ACK] seq={frame_seq & 0xFF}")
                    else:
                        print(f"[WARN] Invalid frame (checksum fail or bad header), dropping")
                        # 显示原始hex
                        hex_str = ' '.join(f'{b:02X}' for b in frame_bytes)
                        print(f"       Raw: {hex_str}")

                    # 消费已处理的字节
                    buf = buf[FRAME_SIZE:]
            else:
                time.sleep(0.01)
    except KeyboardInterrupt:
        print("\n[MCU Sim] Shutting down...")
    finally:
        ser.close()
        print(f"[MCU Sim] Closed. Total frames: {frame_seq}, dropped bytes: {dropped}")


if __name__ == "__main__":
    main()
