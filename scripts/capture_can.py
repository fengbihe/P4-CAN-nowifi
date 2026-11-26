#!/usr/bin/env python3
"""P4-CAN USB 日志采集与诊断（只用 Python 标准库，不需要 pyserial / arduino-cli）

用法：
    python3 capture_can.py            # 采集 10 秒
    python3 capture_can.py 30         # 采集 30 秒
    python3 capture_can.py 10 /dev/cu.usbmodem2401

它会：
  1. 自动找 /dev/cu.usbmodem* 端口
  2. 发 LOGGER START ACTIVE 开始采集（不改变 CAN 收发模式）
  3. 采集指定秒数，发 LOGGER STOP
  4. 打印每路总线的收帧数、出现过的 CAN ID、@STAT 计数
  5. 把完整原始日志存到 ~/p4can_capture.txt

判读：
  - 某路总线帧数为 0  → 那一路没接到 / 接反 / 没电
  - 三路都是 0        → 板子根本没在总线上（供电或接线）
  - 有帧但功能没反应  → 通信正常，是功能触发条件或订阅的问题
"""
import collections
import glob
import os
import select
import sys
import time
import tty

DUR = float(sys.argv[1]) if len(sys.argv) > 1 else 10.0
PORT = sys.argv[2] if len(sys.argv) > 2 else None

if not PORT:
    cands = sorted(glob.glob('/dev/cu.usbmodem*'))
    if not cands:
        print("错误：找不到 /dev/cu.usbmodem* 端口。")
        print("      先确认板子已上电、USB 数据线已插好。")
        sys.exit(1)
    PORT = cands[0]

print("端口: %s   采集时长: %.0f 秒" % (PORT, DUR))
try:
    fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
except OSError as e:
    print("打开端口失败: %s" % e)
    print("提示：串口监视器/Arduino IDE 占用着端口时先关掉。")
    sys.exit(1)

tty.setraw(fd)
buf = b''


def pump(seconds):
    global buf
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.2)
        if r:
            try:
                chunk = os.read(fd, 1 << 16)
                if chunk:
                    buf += chunk
            except BlockingIOError:
                pass


os.write(fd, b'LOGGER START ACTIVE\n')
pump(DUR)
os.write(fd, b'LOGGER STOP\n')
pump(1.0)
os.close(fd)

out = os.path.expanduser('~/p4can_capture.txt')
with open(out, 'wb') as fh:
    fh.write(buf)

lines = buf.decode('ascii', 'replace').splitlines()
proto = [l for l in lines if l.startswith('@') and not l.startswith('@CAN,')]
can = [l for l in lines if l.startswith('@CAN,')]

print("\n===== 协议应答 =====")
for l in proto[:12]:
    print(" ", l[:120])
if not proto:
    print("  （没有任何协议应答 —— 板子可能没运行固件，或端口不对）")

print("\n===== 收帧统计 =====")
per_bus = collections.Counter()
ids = collections.defaultdict(collections.Counter)
for l in can:
    f = l.split(',')
    if len(f) >= 8:
        bus = f[4]
        per_bus[bus] += 1
        ids[bus][f[6]] += 1

names = {'1': 'CAN1 BODY', '2': 'CAN2 CHASSIS', '3': 'CAN3 PARTY'}
for b in ('1', '2', '3'):
    n = per_bus.get(b, 0)
    print("  %-14s %6d 帧" % (names[b], n))
    if n:
        top = ', '.join('0x%s(%d)' % (k, v) for k, v in ids[b].most_common(8))
        print("      常见ID: %s" % top)

print("\n===== 结论 =====")
total = sum(per_bus.values())
if total == 0:
    print("  三路都是 0 帧：板子没有收到任何 CAN 报文。")
    print("  优先查：12V 供电是否到位 → H1 端子接线/H-L 是否接反 → 取的是不是这三路总线。")
else:
    print("  共收到 %d 帧，通信是通的。" % total)
    missing = [names[b] for b in ('1', '2', '3') if not per_bus.get(b)]
    if missing:
        print("  但这几路是 0 帧，需要单独查：%s" % ', '.join(missing))
    print("  若功能仍无反应，方向是触发条件（AP/NOA 是否激活）或车辆订阅，而不是接线。")

print("\n完整日志已存: %s  (%d 字节)" % (out, len(buf)))
print("把这个文件发回来即可。")
