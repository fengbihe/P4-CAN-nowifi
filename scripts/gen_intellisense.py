#!/usr/bin/env python3
"""生成 .vscode/c_cpp_properties.json，让 VS Code IntelliSense 能找到 ESP-IDF / FreeRTOS 头文件。

背景：Arduino-ESP32 3.x 的 esp32p4 目标用 GCC 的 "-iprefix <libs>/include/" +
"-iwithprefixbefore <相对路径>" 方式来给 ESP-IDF 组件加头文件搜索路径（见
<libs>/<ver>/flags/includes）。VS Code 的 C/C++ 扩展（cpptools）不支持这种写法，
所以 .ino 文件（它在 compile_commands.json 里没有条目，只能靠 includePath 兜底）
会满屏报「无法打开源文件 "freertos/FreeRTOS.h"」之类的错误。

本脚本把这些相对路径展开成绝对路径，连同 -I、宏定义一起写进 c_cpp_properties.json。
编译本身与 VS Code 无关，此脚本只影响编辑器体验。

用法：python3 scripts/gen_intellisense.py
      build.sh --ide-db 会自动调用（保证随核心版本更新而更新）
"""
import json
import os
import re
import shutil
import sys

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKETCH_DIR = os.path.join(PROJECT_ROOT, "firmware", "P4-CAN-Universal-v3.35.3")
ARDUINO15 = os.path.expanduser("~/Library/Arduino15/packages/esp32")
OUT = os.path.join(PROJECT_ROOT, ".vscode", "c_cpp_properties.json")


def latest_dir(path, pattern=r"^\d"):
    if not os.path.isdir(path):
        return None
    cands = [d for d in os.listdir(path) if os.path.isdir(os.path.join(path, d)) and re.match(pattern, d)]

    def ver(d):
        return [int(x) for x in re.findall(r"\d+", d)]

    return os.path.join(path, sorted(cands, key=ver)[-1]) if cands else None


def resolve_libs_dir():
    """返回 (esp32p4-libs 目录, core 目录, 工具链 bin 目录, 核心版本号)"""
    core_root = os.path.join(ARDUINO15, "hardware", "esp32")
    core_ver = latest_dir(core_root)
    if not core_ver:
        sys.exit("错误: 找不到 Arduino-ESP32 核心，请先运行 scripts/build.sh")
    libs_ver = None
    for d in os.listdir(core_ver):
        pass
    # 核心版本号 = 目录名，如 3.3.12
    version = os.path.basename(core_ver)
    libs = os.path.join(ARDUINO15, "tools", "esp32p4-libs", version)
    if not os.path.isdir(libs):
        libs = latest_dir(os.path.join(ARDUINO15, "tools", "esp32p4-libs"))
    tool = latest_dir(os.path.join(ARDUINO15, "tools", "esp-rv32"))
    return libs, core_ver, tool, version


def read_flag_file(path):
    """读取 arduino 的 response 文件（单行，空格分隔）"""
    if not os.path.isfile(path):
        return []
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read().split()


def expand_iprefix(tokens, iprefix):
    """把 -iwithprefixbefore X 展开成 iprefix + X"""
    dirs = []
    i = 0
    while i < len(tokens):
        tok = tokens[i]
        if tok in ("-iwithprefixbefore", "-iwithprefix") and i + 1 < len(tokens):
            dirs.append(os.path.normpath(os.path.join(iprefix, tokens[i + 1])))
            i += 2
            continue
        if tok.startswith("-iwithprefixbefore") and len(tok) > len("-iwithprefixbefore"):
            dirs.append(os.path.normpath(os.path.join(iprefix, tok[len("-iwithprefixbefore"):])))
        elif tok == "-I" and i + 1 < len(tokens):
            dirs.append(os.path.normpath(tokens[i + 1]))
            i += 1
        elif tok.startswith("-I") and len(tok) > 2:
            dirs.append(os.path.normpath(tok[2:]))
        i += 1
    return dirs


def collect_defines(tokens):
    out = []
    i = 0
    while i < len(tokens):
        tok = tokens[i]
        if tok == "-D" and i + 1 < len(tokens):
            out.append(tokens[i + 1])
            i += 1
        elif tok.startswith("-D") and len(tok) > 2:
            out.append(tok[2:])
        i += 1
    return out


def main():
    if not os.path.isdir(SKETCH_DIR):
        sys.exit("错误: 找不到 sketch 目录: %s" % SKETCH_DIR)

    libs, core, tool, version = resolve_libs_dir()
    iprefix = os.path.join(libs, "include") + os.sep

    tokens = []
    tokens += read_flag_file(os.path.join(libs, "flags", "includes"))
    tokens += read_flag_file(os.path.join(libs, "flags", "defines"))

    include_dirs = [SKETCH_DIR]
    include_dirs += expand_iprefix(tokens, iprefix)

    # compile_commands.json 里的 -I（库源码路径、qio_qspi 等）
    db_path = os.path.join(PROJECT_ROOT, "build", "compile_commands.json")
    if os.path.isfile(db_path):
        try:
            with open(db_path, "r", encoding="utf-8") as fh:
                db = json.load(fh)
            args = db[0].get("arguments") or db[0].get("command", "").split()
            cmd_iprefix = iprefix
            for k, a in enumerate(args):
                if a == "-iprefix" and k + 1 < len(args):
                    cmd_iprefix = args[k + 1]
            include_dirs += [d for d in expand_iprefix(args, cmd_iprefix) if not d.endswith("/flags")]
        except (ValueError, KeyError, IndexError):
            pass

    # 固定补充项
    include_dirs += [
        os.path.join(core, "cores", "esp32"),
        os.path.join(core, "variants", "esp32p4"),
        os.path.join(SKETCH_DIR, "src"),
        os.path.join(SKETCH_DIR, "automatic_shift"),
        os.path.join(SKETCH_DIR, "third_party", "tinycrypt", "include"),
        os.path.join(libs, "qio_qspi", "include"),
        os.path.join(libs, "dio_qspi", "include"),
    ]
    for lib in sorted(os.listdir(os.path.join(core, "libraries"))):
        src = os.path.join(core, "libraries", lib, "src")
        if os.path.isdir(src):
            include_dirs.append(src)

    # 去重 + 只保留真实存在的目录
    seen, final = set(), []
    for d in include_dirs:
        d = os.path.normpath(d)
        if d in seen or not os.path.isdir(d):
            continue
        seen.add(d)
        final.append(d)

    # 关键宏：必须让 cpptools 看到 ARDUINO_USB_MODE=1，否则会显示 .ino 里的 #error 分支。
    # 另外 ESP_PLATFORM / MBEDTLS_CONFIG_FILE 等来自核心的 flags/defines，缺了会导致
    # NimBLE 的 syscfg.h 不去包含 esp_nimble_cfg.h，进而报 MYNEWT_VAL_xxx 未定义。
    defines = [
        "ARDUINO=10607",
        "ARDUINO_ARCH_ESP32",
        "ARDUINO_ESP32P4_DEV",
        "ARDUINO_BOARD=\"ESP32P4_DEV\"",
        "ARDUINO_VARIANT=\"esp32p4\"",
        "ARDUINO_PARTITION_app3M_fat9M_16MB",
        "ARDUINO_HOST_OS=\"macosx\"",
        "CONFIG_IDF_TARGET_ESP32P4",
        "ESP32=ESP32",
        "CORE_DEBUG_LEVEL=0",
        "F_CPU=400000000L",
        "BOARD_HAS_PSRAM",
        "ARDUINO_USB_MODE=1",
        "ARDUINO_USB_CDC_ON_BOOT=1",
        "ARDUINO_USB_MSC_ON_BOOT=0",
        "ARDUINO_USB_DFU_ON_BOOT=0",
    ]
    defines += collect_defines(tokens)
    seen_def, final_def = set(), []
    for d in defines:
        key = d.split("=")[0]
        if key in seen_def:
            continue
        seen_def.add(key)
        final_def.append(d)
    defines = final_def

    gcc = os.path.join(tool, "bin", "riscv32-esp-elf-gcc") if tool else ""

    config = {
        "// 由 scripts/gen_intellisense.py 自动生成，请勿手改": "重新生成: ./scripts/build.sh --ide-db",
        "configurations": [
            {
                "name": "ESP32-P4 (P4-CAN)",
                "includePath": final,
                "defines": defines,
                "compilerPath": gcc,
                "compilerArgs": [
                    "-march=rv32imafc_zicsr_zifencei_xesploop_xespv",
                    "-mabi=ilp32f",
                    "-std=gnu++2b",
                    "-fexceptions",
                ],
                "cStandard": "gnu17",
                "cppStandard": "gnu++2b",
                "intelliSenseMode": "gcc-x64",
                "browse": {
                    "path": final,
                    "limitSymbolsToIncludedHeaders": False,
                },
            }
        ],
        "version": 4,
    }

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    backup = OUT + ".bak"
    if os.path.isfile(OUT) and not os.path.isfile(backup):
        shutil.copy2(OUT, backup)
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(config, fh, ensure_ascii=False, indent=2)
    print("已写入 %s" % OUT)
    print("  核心版本    : %s" % version)
    print("  头文件目录  : %d 个" % len(final))

    # 自检：确认这些曾经报错的头文件现在能被找到
    probe = [
        "freertos/FreeRTOS.h",
        "soc/soc_caps.h",
        "http_parser.h",
        "nvs_flash.h",
        "esp_system.h",
        "esp_heap_caps.h",
        "freertos/semphr.h",
        "esp_timer.h",
        "nvs.h",
        "esp_attr.h",
        "driver/twai.h",
        "host/ble_gap.h",
        "host/ble_store.h",
        "nimble/nimble_port.h",
    ]
    missing = [h for h in probe if not any(os.path.isfile(os.path.join(d, h)) for d in final)]
    if missing:
        print("  ⚠️ 仍无法解析: %s" % ", ".join(missing))
        return 1
    print("  ✅ 关键头文件全部可解析（%d/%d）" % (len(probe) - len(missing), len(probe)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
