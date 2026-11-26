#!/usr/bin/env bash
# P4-CAN Universal v3.35.2 - 串口监视
# 用法: ./scripts/monitor.sh [--port /dev/cu.usbmodem1101] [--baud 115200]
#       Ctrl+] 退出 (screen)
set -euo pipefail

BAUD="115200"
PORT=""
prev=""
for arg in "$@"; do
  if [ "$prev" = "--port" ]; then PORT="$arg"; fi
  if [ "$prev" = "--baud" ]; then BAUD="$arg"; fi
  case "$arg" in
    /dev/*) PORT="$arg" ;;
    -h|--help) sed -n '2,5p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
  esac
  prev="$arg"
done

if [ -z "$PORT" ]; then
  cands=()
  for p in /dev/cu.usbmodem*; do [ -e "$p" ] && cands+=("$p"); done
  if [ ${#cands[@]} -eq 0 ]; then
    echo "错误: 未检测到 /dev/cu.usbmodem* 设备，用 --port 指定。" >&2
    echo "可用串口:" >&2; ls /dev/cu.* 2>/dev/null | sed 's/^/  /' >&2
    exit 1
  fi
  PORT="${cands[0]}"
fi

echo "串口: $PORT   波特率: $BAUD   (Ctrl+] 退出)"
exec screen "$PORT" "$BAUD"
