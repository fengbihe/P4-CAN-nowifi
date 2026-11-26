#!/usr/bin/env bash
# P4-CAN Universal v3.35.2 - macOS 编译脚本 (Arduino-ESP32 + arduino-cli)
#
# 用法:
#   ./scripts/build.sh                 # 编译 postv3 (芯片 v3.00 及更新)
#   ./scripts/build.sh prev3           # 编译 prev3 (芯片 v3.00 之前)
#   ./scripts/build.sh postv3 --clean  # 清理后重新编译
#   ./scripts/build.sh all             # 两种芯片版本都编译
#   ./scripts/build.sh postv3 --ide-db # 额外生成 compile_commands.json (VS Code 智能提示)
#
# 环境变量:
#   CHIP_VARIANT=postv3|prev3  覆盖默认芯片版本
#   ARDUINO_CLI=/path/to/arduino-cli
#   ESP32_CORE=3.3.12          要求的 Arduino-ESP32 版本
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_NAME="P4-CAN-Universal-v3.35.3"
SKETCH_DIR="$PROJECT_ROOT/firmware/$SKETCH_NAME"
BUILD_ROOT="$PROJECT_ROOT/build"
LOG_DIR="$BUILD_ROOT/logs"

ESP32_CORE="${ESP32_CORE:-3.3.12}"
CHIP_VARIANT="${CHIP_VARIANT:-postv3}"

# ---------- 可重复编译：固定内嵌的编译时间戳 ----------
# ESP-IDF 会把 __DATE__ / __TIME__ 写进固件的 esp_app_desc_t，
# 所以不处理的话，同一份源码每次 --clean 重编出来的 SHA256 都不一样。
# 这里默认用「sketch 源码里最新的修改时间」作为 SOURCE_DATE_EPOCH：
#   源码不变 → 时间戳不变 → 产物 SHA256 不变
#   改了任何源码 → 时间戳更新 → SHA256 变化（符合预期，也正是我们想要的信号）
# 想强制指定：SOURCE_DATE_EPOCH=1735689600 ./scripts/build.sh
if [ -z "${SOURCE_DATE_EPOCH:-}" ]; then
  SOURCE_DATE_EPOCH="$(find "$SKETCH_DIR" -type f \
      \( -name '*.ino' -o -name '*.h' -o -name '*.c' -o -name '*.cpp' -o -name '*.yaml' -o -name 'build_opt.h' \) \
      -not -path "*/build/*" -exec stat -f %m {} \; 2>/dev/null | sort -n | tail -1)"
  [ -n "$SOURCE_DATE_EPOCH" ] || SOURCE_DATE_EPOCH=1735689600
fi
export SOURCE_DATE_EPOCH

# 原作者 Compile-P4-Variants.ps1 / README 使用的板级选项（逐项对齐，不要随意改动）
# ★ 本板实际装的是 W25Q16（2MB），且不上无线模块，所以默认用 2MB 方案。
#   固件约 1.1MB，minimal 的 app0=1.25MB 足够，余量约 211KB。
#   若将来换成 BOM 指定的 W25Q256（32MB），用环境变量切回上游标准配置：
#     FLASH_SIZE=32M PARTITION_SCHEME=app3M_fat9M_16MB ./scripts/build.sh postv3
FLASH_SIZE="${FLASH_SIZE:-2M}"
PARTITION_SCHEME="${PARTITION_SCHEME:-minimal}"
BOARD_OPTIONS_BASE="FlashSize=$FLASH_SIZE,PartitionScheme=$PARTITION_SCHEME,PSRAM=enabled,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio"

CLEAN=0
GEN_IDE_DB=0
TARGETS=()

# ---------- 参数解析 ----------
for arg in "$@"; do
  case "$arg" in
    postv3|prev3) TARGETS+=("$arg") ;;
    all)          TARGETS+=(postv3 prev3) ;;
    --clean)      CLEAN=1 ;;
    --ide-db)     GEN_IDE_DB=1 ;;
    -h|--help)    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "未知参数: $arg (用 --help 查看用法)" >&2; exit 2 ;;
  esac
done
[ ${#TARGETS[@]} -eq 0 ] && TARGETS=("$CHIP_VARIANT")

# ---------- 工具查找 ----------
find_arduino_cli() {
  if [ -n "${ARDUINO_CLI:-}" ] && [ -x "${ARDUINO_CLI}" ]; then echo "$ARDUINO_CLI"; return; fi
  for candidate in "$(command -v arduino-cli 2>/dev/null || true)" \
                   /opt/homebrew/bin/arduino-cli /usr/local/bin/arduino-cli "$HOME/.local/bin/arduino-cli"; do
    if [ -n "$candidate" ] && [ -x "$candidate" ]; then echo "$candidate"; return; fi
  done
  return 1
}

ARDUINO_CLI_BIN="$(find_arduino_cli)" || {
  echo "错误: 未找到 arduino-cli。安装方法: brew install arduino-cli" >&2; exit 1; }

# ---------- 前置检查 ----------
preflight() {
  echo "== 环境检查 =="
  echo "arduino-cli : $ARDUINO_CLI_BIN ($("$ARDUINO_CLI_BIN" version | head -1))"

  if [ ! -f "$SKETCH_DIR/$SKETCH_NAME.ino" ]; then
    echo "错误: 找不到主程序 $SKETCH_DIR/$SKETCH_NAME.ino" >&2; exit 1
  fi

  local installed
  installed="$("$ARDUINO_CLI_BIN" core list 2>/dev/null | awk '/^esp32:esp32/ {print $2}' | head -1)"
  if [ "$installed" != "$ESP32_CORE" ]; then
    if [ -z "$installed" ]; then
      echo "错误: 未安装 Arduino-ESP32 核心。执行:" >&2
      echo "  $ARDUINO_CLI_BIN core install esp32:esp32@$ESP32_CORE" >&2
    else
      echo "警告: 当前核心为 $installed，本项目按 $ESP32_CORE 验证。" >&2
      echo "  如需一致: $ARDUINO_CLI_BIN core install esp32:esp32@$ESP32_CORE" >&2
    fi
    [ -z "$installed" ] && exit 1
  else
    echo "ESP32 核心  : esp32:esp32@$installed (符合要求)"
  fi
  mkdir -p "$BUILD_ROOT" "$LOG_DIR"
}

# ---------- 单个变体编译 ----------
build_variant() {
  local variant="$1"
  local fqbn="esp32:esp32:esp32p4:ChipVariant=$variant,$BOARD_OPTIONS_BASE"
  local build_path="$BUILD_ROOT/build-$variant"
  local out_dir="$BUILD_ROOT/firmware/$variant"
  local stamp log
  stamp="$(date +%Y%m%d-%H%M%S)"
  log="$LOG_DIR/compile-$variant-$stamp.log"

  local revision
  case "$variant" in
    postv3) revision="REV3.X" ;;
    prev3)  revision="REV1.X" ;;
  esac

  echo
  echo "== 编译 $variant ($revision) =="
  echo "FQBN      : $fqbn"
  echo "Flash     : $FLASH_SIZE / 分区方案 $PARTITION_SCHEME"
  echo "构建目录  : $build_path"
  echo "输出目录  : $out_dir"
  echo "日志      : $log"
  echo "时间戳    : SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH ($(date -r "$SOURCE_DATE_EPOCH" '+%Y-%m-%d %H:%M:%S' 2>/dev/null || echo '?'))"

  if [ "$CLEAN" -eq 1 ]; then
    echo "清理构建目录..."
    rm -rf "$build_path"
  fi
  mkdir -p "$build_path" "$out_dir"

  # --warnings default: 与原项目保持一致；源码中大量 -Wvolatile 提示属正常现象
  if ! "$ARDUINO_CLI_BIN" compile \
        --fqbn "$fqbn" \
        --build-path "$build_path" \
        --output-dir "$out_dir" \
        --warnings default \
        "$SKETCH_DIR" 2>&1 | tee "$log"; then
    echo
    echo "编译失败: $variant，日志: $log" >&2
    grep -n "error:" "$log" | head -20 >&2 || true
    return 1
  fi

  # 整理发布文件名（与原 PowerShell 脚本的命名规则一致）
  local app="$out_dir/$SKETCH_NAME.ino.bin"
  local merged="$out_dir/$SKETCH_NAME.ino.merged.bin"
  [ -f "$app" ]    || { echo "缺少产物: $app" >&2; return 1; }
  [ -f "$merged" ] || { echo "缺少产物: $merged" >&2; return 1; }

  mv -f "$app"    "$out_dir/$SKETCH_NAME.OTA.$revision.bin"
  mv -f "$merged" "$out_dir/$SKETCH_NAME.FULLFLASH.$revision.merged.bin"
  cp -f "$build_path/flash_args" "$out_dir/flash_args.txt" 2>/dev/null || true

  local app_final="$out_dir/$SKETCH_NAME.OTA.$revision.bin"
  local merged_final="$out_dir/$SKETCH_NAME.FULLFLASH.$revision.merged.bin"

  # arduino-cli 1.6.0 除 --build-path/--output-dir 之外，还会把最终产物另存一份到
  # <sketch>/build/<fqbn>/（纯副本，不含对象文件，不参与增量构建，约 65MB）。清理掉，
  # 保持源码树干净；增量编译只依赖 --build-path，删除无影响。
  if [ -d "$SKETCH_DIR/build" ]; then
    rm -rf "$SKETCH_DIR/build"
  fi

  echo
  echo "== $variant 编译成功 =="
  printf "  应用固件  : %s (%s 字节)\n" "$app_final" "$(stat -f%z "$app_final")"
  printf "  整片固件  : %s (%s 字节)\n" "$merged_final" "$(stat -f%z "$merged_final")"
  printf "  SHA256    : %s\n" "$(shasum -a 256 "$app_final" | awk '{print $1}')"

  # flash_args.txt 里的文件名是编译期的 .ino.xxx.bin，改写成实际产物名，供 flash.sh 参考
  cat > "$out_dir/flash_layout.txt" <<LAYOUT
# 分段烧录地址（由 Arduino-ESP32 $ESP32_CORE 的 esp32p4 目标生成，勿改）
0x2000  $SKETCH_NAME.ino.bootloader.bin   (=$out_dir/$SKETCH_NAME.ino.bootloader.bin)
0x8000  $SKETCH_NAME.ino.partitions.bin   (=$out_dir/$SKETCH_NAME.ino.partitions.bin)
0xe000  boot_app0.bin                     (=$build_path/boot_app0.bin)
0x10000 $SKETCH_NAME.OTA.$revision.bin    (应用固件)
# 整片镜像: FULLFLASH.$revision.merged.bin 直接烧到 0x0 (32MB, 覆盖 bootloader+分区+应用)
LAYOUT

  if [ "$GEN_IDE_DB" -eq 1 ]; then
    generate_ide_db "$variant" "$fqbn"
  fi
}

# ---------- 生成 compile_commands.json 供 VS Code / clangd 使用 ----------
generate_ide_db() {
  local variant="$1" fqbn="$2"
  local db_dir="$BUILD_ROOT/ide-db-$variant"
  echo
  echo "== 生成 IDE 配置 ($variant) =="
  rm -rf "$db_dir"; mkdir -p "$db_dir"
  if "$ARDUINO_CLI_BIN" compile --fqbn "$fqbn" --build-path "$db_dir" \
        --only-compilation-database "$SKETCH_DIR" >/dev/null 2>&1 && [ -f "$db_dir/compile_commands.json" ]; then
    cp -f "$db_dir/compile_commands.json" "$BUILD_ROOT/compile_commands.json"
    echo "已写入 $BUILD_ROOT/compile_commands.json"
  else
    echo "跳过 compile_commands.json: 该 arduino-cli 版本不支持 --only-compilation-database" >&2
  fi
  # ESP-IDF 用 -iprefix/-iwithprefixbefore 给头文件路径，cpptools 不认，
  # 所以必须展开成绝对路径写进 c_cpp_properties.json，否则 .ino 会满屏"无法打开源文件"
  if command -v python3 >/dev/null 2>&1; then
    python3 "$PROJECT_ROOT/scripts/gen_intellisense.py" || \
      echo "警告: IntelliSense 配置生成失败，不影响编译" >&2
  fi
}

# ---------- 主流程 ----------
preflight
FAILED=()
for variant in "${TARGETS[@]}"; do
  build_variant "$variant" || FAILED+=("$variant")
done

echo
if [ ${#FAILED[@]} -gt 0 ]; then
  echo "编译失败: ${FAILED[*]}" >&2
  exit 1
fi
echo "全部完成。产物目录: $BUILD_ROOT/firmware"
