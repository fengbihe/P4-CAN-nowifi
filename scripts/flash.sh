#!/usr/bin/env bash
# P4-CAN Universal v3.35.2 - macOS USB 烧录脚本 (ESP32-P4 原生 USB Serial/JTAG)
#
# 用法:
#   ./scripts/flash.sh --list                 # 只列出可用串口
#   ./scripts/flash.sh                        # 自动选口，分段烧录 (最快)
#   ./scripts/flash.sh --merged               # 烧写 32MB 整片镜像 (最稳，耗时数分钟)
#   ./scripts/flash.sh --port /dev/cu.usbmodem1101
#   ./scripts/flash.sh --variant prev3        # 芯片 v3.00 之前
#   ./scripts/flash.sh --erase --yes          # 整片擦除 (会清空 NVS/配置，慎用)
#   ./scripts/flash.sh --yes                  # 跳过确认
#
# 烧录地址(分段模式):
#   0x2000  bootloader.bin
#   0x8000  partitions.bin
#   0xe000  boot_app0.bin        (重置 OTA 选择位到 app0)
#   0x10000 应用固件 (OTA 槽 app0, 大小上限 3MB)
# 整片镜像模式: FULLFLASH.*.merged.bin 写入 0x0
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_NAME="P4-CAN-Universal-v3.35.3"
BUILD_ROOT="$PROJECT_ROOT/build"

CHIP="esp32p4"
BAUD="921600"
VARIANT="postv3"
PORT=""
MODE="segments"
FLASH_SIZE="2MB"   # 本板装的是 2MB Flash；换 32MB 后用 --flash-size 32MB
NO_STUB=0
DO_ERASE=0
ASSUME_YES=0
LIST_ONLY=0

while [ $# -gt 0 ]; do
  case "$1" in
    --list)       LIST_ONLY=1 ;;
    --merged)     MODE="merged" ;;
    --no-stub)    NO_STUB=1 ;;
    --erase)      DO_ERASE=1 ;;
    --yes|-y)     ASSUME_YES=1 ;;
    --port)       PORT="$2"; shift ;;
    --baud)       BAUD="$2"; shift ;;
    --flash-size) FLASH_SIZE="$2"; shift ;;
    postv3|prev3) VARIANT="$1" ;;
    -h|--help)    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    /dev/*)       PORT="$1" ;;
    *) echo "未知参数: $1 (用 --help 查看用法)" >&2; exit 2 ;;
  esac
  shift
done

case "$VARIANT" in
  postv3) REVISION="REV3.X" ;;
  prev3)  REVISION="REV1.X" ;;
esac

# ---------- 查找 esptool ----------
find_esptool() {
  if [ -n "${ESPTOOL:-}" ] && [ -x "${ESPTOOL}" ]; then echo "$ESPTOOL"; return; fi
  local esptool_dir="$HOME/Library/Arduino15/packages/esp32/tools/esptool_py"
  if [ -d "$esptool_dir" ]; then
    local found
    found="$(ls -1 "$esptool_dir" 2>/dev/null | sort -V | tail -1)"
    if [ -n "$found" ] && [ -x "$esptool_dir/$found/esptool" ]; then
      echo "$esptool_dir/$found/esptool"; return
    fi
  fi
  if command -v esptool >/dev/null 2>&1; then command -v esptool; return; fi
  if python3 -c "import esptool" >/dev/null 2>&1; then echo "python3 -m esptool"; return; fi
  return 1
}
list_ports() {
  echo "== 当前 /dev/cu.* 串口 =="
  local any=0
  for p in /dev/cu.*; do
    [ -e "$p" ] || continue
    any=1
    case "$p" in
      /dev/cu.usbmodem*)    echo "  $p   <- ESP32-P4 原生 USB Serial/JTAG（优先）" ;;
      /dev/cu.usbserial*)   echo "  $p   <- USB 转串口芯片" ;;
      /dev/cu.wchusbserial*) echo "  $p   <- CH340/CH343" ;;
      /dev/cu.SLAB_USBtoUART*) echo "  $p   <- CP210x" ;;
      *)                    echo "  $p" ;;
    esac
  done
  [ "$any" -eq 1 ] || echo "  (无)"
  echo
  echo "提示: ESP32-P4 的 USB Serial/JTAG 在 macOS 上显示为 /dev/cu.usbmodem*。"
  echo "      若看不到，请确认 USB-C 数据线已接到 ESP32-P4 的 USB 引脚 (GPIO24/GPIO25)。"
}

detect_port() {
  local cands=()
  local p
  for p in /dev/cu.usbmodem*; do [ -e "$p" ] && cands+=("$p"); done
  if [ ${#cands[@]} -eq 0 ]; then
    for p in /dev/cu.usbserial* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART*; do
      [ -e "$p" ] && cands+=("$p")
    done
  fi
  if [ ${#cands[@]} -eq 1 ]; then
    echo "${cands[0]}"; return 0
  fi
  if [ ${#cands[@]} -eq 0 ]; then
    echo "错误: 未检测到 ESP32 串口设备。" >&2
    list_ports >&2
    echo >&2
    echo "排查顺序：" >&2
    echo "  1) 数据线必须是能传数据的 USB-C 线（非充电线）" >&2
    echo "  2) 板子是否已上电，3.3V/5V 电源是否正常" >&2
    echo "  3) 按住 BOOT (GPIO35) 再点 RESET (EN)，或按住 BOOT 上电进入下载模式" >&2
    echo "  4) 用 --port 手动指定端口" >&2
    return 1
  fi
  echo "错误: 检测到多个串口，请用 --port 指定：" >&2
  printf '  %s\n' "${cands[@]}" >&2
  return 1
}

# ---------- 前置检查 ----------
ART_DIR="$BUILD_ROOT/firmware/$VARIANT"
APP_BIN="$ART_DIR/$SKETCH_NAME.OTA.$REVISION.bin"
MERGED_BIN="$ART_DIR/$SKETCH_NAME.FULLFLASH.$REVISION.merged.bin"
BOOT_BIN="$ART_DIR/$SKETCH_NAME.ino.bootloader.bin"
PART_BIN="$ART_DIR/$SKETCH_NAME.ino.partitions.bin"
BOOT_APP0="$BUILD_ROOT/build-$VARIANT/boot_app0.bin"

[ "$LIST_ONLY" -eq 1 ] && { list_ports; exit 0; }

ESPTOOL_BIN="$(find_esptool)" || {
  echo "错误: 未找到 esptool。它随 Arduino-ESP32 核心安装，请先运行 ./scripts/build.sh" >&2; exit 1; }
ESPTOOL_CMD=($ESPTOOL_BIN)

if [ -z "$PORT" ]; then
  PORT="$(detect_port)" || exit 1
fi
[ -e "$PORT" ] || { echo "错误: 端口不存在: $PORT" >&2; exit 1; }

if [ "$MODE" = "merged" ]; then
  [ -f "$MERGED_BIN" ] || { echo "错误: 未找到整片镜像 $MERGED_BIN，请先编译: ./scripts/build.sh $VARIANT" >&2; exit 1; }
else
  for f in "$APP_BIN" "$BOOT_BIN" "$PART_BIN" "$BOOT_APP0"; do
    [ -f "$f" ] || { echo "错误: 缺少烧录文件 $f，请先编译: ./scripts/build.sh $VARIANT" >&2; exit 1; }
  done
fi

echo "== 烧录计划 =="
echo "芯片     : $CHIP ($VARIANT / $REVISION)"
echo "端口     : $PORT"
echo "esptool  : $ESPTOOL_BIN"
echo "Flash    : $FLASH_SIZE   (ROM loader: $([ "$NO_STUB" -eq 1 ] && echo 是 || echo 否))"
if [ "$MODE" = "merged" ]; then
  echo "模式     : 整片镜像 0x0 <- $(basename "$MERGED_BIN") ($(stat -f%z "$MERGED_BIN") 字节)"
else
  echo "模式     : 分段烧录"
  echo "  0x2000  <- $(basename "$BOOT_BIN")"
  echo "  0x8000  <- $(basename "$PART_BIN")"
  echo "  0xe000  <- boot_app0.bin"
  echo "  0x10000 <- $(basename "$APP_BIN") ($(stat -f%z "$APP_BIN") 字节)"
fi
[ "$DO_ERASE" -eq 1 ] && echo "附加动作 : 烧录前整片擦除 (会清空 NVS 与所有配置!)"
echo

if [ "$ASSUME_YES" -ne 1 ]; then
  printf "确认开始烧录? [y/N] "
  read -r ans
  case "$ans" in y|Y|yes|YES) ;; *) echo "已取消。"; exit 0 ;; esac
fi

# ---------- 执行 ----------
run_esptool() { "${ESPTOOL_CMD[@]}" "$@"; }

# 这块板实测：stub flasher 写入后会报 "written flash region is empty"（校验读回为空），
# 换成 ROM loader（--no-stub）可以正常写入。默认仍用 stub，可用 --no-stub 切换。
STUB_ARGS=()
[ "$NO_STUB" -eq 1 ] && STUB_ARGS=(--no-stub)
# 换 ROM loader 时速率意义不大，用 460800 即可
[ "$NO_STUB" -eq 1 ] && BAUD="460800"

echo
echo "== 连接芯片 =="
run_esptool --chip "$CHIP" --port "$PORT" --baud "$BAUD" \
  --before default-reset --after hard-reset chip-id

if [ "$DO_ERASE" -eq 1 ]; then
  echo
  echo "== 整片擦除 =="
  run_esptool --chip "$CHIP" --port "$PORT" --baud "$BAUD" \
    --before default-reset --after no-reset erase-flash
fi

echo
echo "== 写入 Flash =="
if [ "$MODE" = "merged" ]; then
  run_esptool --chip "$CHIP" --port "$PORT" --baud "$BAUD" \
    --before default-reset --after hard-reset \
    "${STUB_ARGS[@]}" write-flash --flash-mode dio --flash-freq 80m --flash-size "$FLASH_SIZE" \
      0x0 "$MERGED_BIN"
else
  run_esptool --chip "$CHIP" --port "$PORT" --baud "$BAUD" \
    --before default-reset --after hard-reset \
    "${STUB_ARGS[@]}" write-flash --flash-mode dio --flash-freq 80m --flash-size "$FLASH_SIZE" \
      0x2000 "$BOOT_BIN" \
      0x8000 "$PART_BIN" \
      0xe000 "$BOOT_APP0" \
      0x10000 "$APP_BIN"
fi

echo
echo "== 烧录完成 =="
echo "芯片已复位并开始运行。查看串口输出: ./scripts/monitor.sh --port $PORT"
echo "注意: 烧录成功不等于车载功能验证成功；请先在台架上确认串口日志与网页面板。"
