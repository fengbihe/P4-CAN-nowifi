# scripts/ — 编译、烧录、串口工具

三个脚本都已 `chmod +x`，可脱离 VS Code 直接跑。全部是 bash，无额外依赖（用系统自带的 `screen`）。

| 脚本 | 作用 |
| --- | --- |
| `build.sh` | 调用 arduino-cli 编译固件，整理产物并打印大小/SHA256 |
| `flash.sh` | 探测串口 → 列出烧录地址 → 二次确认 → esptool 烧写 |
| `monitor.sh` | 115200 串口监视（`Ctrl+]` 退出） |
| `gen_intellisense.py` | 生成 `.vscode/c_cpp_properties.json`，修 VS Code 找不到 ESP-IDF 头文件的问题（由 `build.sh --ide-db` 自动调用） |

---

## build.sh

```bash
./scripts/build.sh                    # 默认 postv3
./scripts/build.sh prev3              # 芯片 v3.00 之前
./scripts/build.sh all                # 两个变体都编
./scripts/build.sh postv3 --clean     # 删构建缓存重编
./scripts/build.sh postv3 --ide-db    # 额外生成 build/compile_commands.json
```

- 环境变量：`CHIP_VARIANT`、`ARDUINO_CLI`（自定义 arduino-cli 路径）、`ESP32_CORE`（默认要求 3.3.12）
- 会先做前置检查：arduino-cli 是否存在、核心版本是否匹配、主 `.ino` 是否存在
- 产物写到 `build/firmware/<变体>/`，日志写到 `build/logs/`
- 成功后把 `*.ino.bin` 重命名为 `*.OTA.REVx.X.bin`、`*.merged.bin` 重命名为 `*.FULLFLASH.REVx.X.merged.bin`
  （与原 PowerShell 脚本的命名规则一致），并生成 `flash_layout.txt` 记录地址
- 编译成功后自动删掉 arduino-cli 在 `<sketch>/build/` 里重复写的一份产物（约 65 MB，纯副本），
  保持源码树干净；不影响 `build/build-<变体>/` 的增量编译。详见 `../docs/BUILD_REPORT.md` §4.4

## flash.sh

```bash
./scripts/flash.sh --list                  # 只列串口，不烧
./scripts/flash.sh                         # 自动选口，分段烧录
./scripts/flash.sh --merged                # 烧 32 MB 整片镜像到 0x0
./scripts/flash.sh --port /dev/cu.usbmodem1101
./scripts/flash.sh prev3                   # 烧 prev3 变体
./scripts/flash.sh --erase --yes           # 整片擦除（清空 NVS/配置！）
./scripts/flash.sh --yes                   # 跳过二次确认
```

- 端口优先级：`/dev/cu.usbmodem*`（原生 USB Serial/JTAG）→ `usbserial` → `wchusbserial` → `SLAB_USBtoUART`
- 找到多个端口时**不会瞎猜**，会列出候选并要求用 `--port` 指定
- 烧录地址固定在脚本里，与 core 生成的 `flash_args` 一致：
  `0x2000` bootloader / `0x8000` 分区表 / `0xe000` boot_app0 / `0x10000` 应用
- **默认需要输入 `y` 确认**；擦除必须显式加 `--erase`
- esptool 自动从 `~/Library/Arduino15/packages/esp32/tools/esptool_py/<最新版本>/esptool` 找

## monitor.sh

```bash
./scripts/monitor.sh                       # 自动选 /dev/cu.usbmodem*
./scripts/monitor.sh --port /dev/cu.usbmodem1101 --baud 115200
```

⚠️ 固件在 `BOOT OK` 之后会把 USB CDC 交给 **CAN USB logger**（二进制帧协议），
所以监视窗口里大部分时间是"乱码"，这是正常的。文本启动日志只在复位后很短一段。

---
