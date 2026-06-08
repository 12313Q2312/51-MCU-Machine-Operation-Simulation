#!/usr/bin/env python3
"""
mcu_simulator.py — MCU 端模拟器 (用于协议调试, 无需真实硬件)

模拟 51 MCU 行为:
  1. 打开虚拟串口对 (com0com 或 socat)
  2. 接收 PC 发来的二进制帧
  3. 解码并验证校验和
  4. 发送 ACK (0xCC + seq)
  5. 在 ASCII 终端上渲染 8x8 LED 点阵

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

def decode_frame(frame: bytes):
    """解码 10 字节帧, 失败返回 None"""
    if len(frame) != FRAME_SIZE or frame[0] != FRAME_HEADER:
        return None
    # XOR 校验
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

def render_led(qx, qy):
    """在终端渲染 8x8 LED 点阵"""
    # 量化 0-65535 → 0-7
    col = min(7, (qx * 8) // 65536)
    row = 7 - min(7, (qy * 8) // 65536)  # Y 轴翻转

    print("\033[2J\033[H")  # 清屏
    print("┌────────┐")
    for r in range(8):
        line = "│"
        for c in range(8):
            if r == row and c == col:
                line += "●"
            else:
                line += "·"
        line += "│"
        print(line)
    print("└────────┘")

def main():
    parser = argparse.ArgumentParser(description="MCU Simulator")
    parser.add_argument("--port", default="COM4", help="串口号")
    parser.add_argument("--baud", type=int, default=9600, help="波特率")
    args = parser.parse_args()

    print(f"[MCU Sim] Opening {args.port} @ {args.baud}")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.5)
    except serial.SerialException as e:
        print(f"[ERROR] {e}")
        sys.exit(1)

    buf = bytearray()
    frame_seq = 0

    print(f"[MCU Sim] Listening on {args.port}... Ctrl+C to exit")
    print("[MCU Sim] Waiting for frames from PC...")

    try:
        while True:
            if ser.in_waiting:
                byte = ser.read(1)
                buf.append(byte[0])

                # 搜索帧头
                while len(buf) > 0 and buf[0] != FRAME_HEADER:
                    buf.pop(0)

                # 完整帧?
                if len(buf) >= FRAME_SIZE:
                    data = decode_frame(bytes(buf[:FRAME_SIZE]))
                    if data:
                        frame_seq += 1
                        motion_names = {0: "LINE", 1: "CIRCLE", 2: "SIN"}
                        mn = motion_names.get(data['motion'], "UNKNOWN")
                        print(f"\n[Frame {frame_seq}] {mn} "
                              f"pos=({data['x']},{data['y']}) "
                              f"vel=({data['vx']},{data['vy']})")
                        render_led(data['x'], data['y'])

                        # 发送 ACK
                        ack = bytes([0xCC, frame_seq & 0xFF])
                        ser.write(ack)
                        print(f"[ACK] seq={frame_seq}")
                    else:
                        print(f"[WARN] Invalid frame, dropping")

                    # 消费已处理的字节
                    buf = buf[FRAME_SIZE:]
            else:
                time.sleep(0.01)
    except KeyboardInterrupt:
        print("\n[MCU Sim] Shutting down...")
    finally:
        ser.close()
        print("[MCU Sim] Closed.")

if __name__ == "__main__":
    main()
