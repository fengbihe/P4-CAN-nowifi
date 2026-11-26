# README_MAC — 在 macOS + VS Code 下编译与烧录 ESP32-P4

面向 macOS（Apple Silicon）用户，用 **VS Code 点两下**完成「编译 → USB 烧录」。
原项目功能、源码逻辑、板级选项均未改动。

---

## 1. 环境现状（本机已就绪）

| 组件 | 版本 | 位置 |
| --- | --- | --- |
| arduino-cli | 1.6.0 (Homebrew) | `/opt/homebrew/bin/arduino-cli` |
| Arduino-ESP32 核心 | **3.3.12** | `~/Library/Arduino15/packages/esp32/hardware/esp32/3.3.12` |
| esptool | 5.3.1 | `~/Library/Arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool` |
| ESP32-P4 预编译库 | esp32p4-libs / esp32p4_es-libs 3.3.12 | `~/Library/Arduino15/packages/esp32/tools/` |

> 说明：本机原本**没有安装任何 Arduino 平台**，本次已安装 `esp32:esp32@3.3.12`（原 README 指定的版本）。
> 不需要 ESP-IDF，也不需要 PlatformIO —— 本项目是 Arduino 框架工程。

---

## 1.5 更喜欢"点按钮"？用已装好的 Arduino CLI 扩展

你 VS Code 里已经装了 `vscode-arduino-cli-wrapper`。打开 `P4-CAN/` 之后：

- **状态栏**出现 `Compile` / `Upload` / `Monitor` 和 FQBN / 串口 / 波特率，点一下就能改；
- **资源管理器**里多出 `Arduino CLI` 视图，能点编译、上传、串口、Board Details；
- 板级参数已经写在 `firmware/P4-CAN-Universal-v3.35.3/sketch.yaml` 的 `default_fqbn` 里，
  不用每次手输 FQBN；命令行 `arduino-cli compile <sketch>` 也能直接编。

前提：命令面板执行一次 `Arduino CLI: Check CLI Version`，确认找到 `/opt/homebrew/bin/arduino-cli`。
它和下面的 `build.sh`、VS Code tasks 调的是同一个 arduino-cli，产物写不同目录，互不影响。

细节（包括为什么不建议用 `sketch.yaml` 的 `profiles`）见 `../.vscode/README.md`。

---

## 2. 三步上手

### 2.1 打开工程

VS Code → `文件 → 打开文件夹` → 选择 `P4-CAN/` 这个目录（**不是** `firmware/`）。
打开后 IntelliSense 和 Build Task 都以 `P4-CAN/` 为根。

### 2.2 编译

`Cmd+Shift+B`（或 `Terminal → Run Build Task`）→ 选 **`P4-CAN: Build (postv3)`**。

等价的命令行：

```bash
cd P4-CAN
./scripts/build.sh postv3
```

成功后会打印固件大小与 SHA256，产物在 `build/firmware/postv3/`。

### 2.3 烧录

用 USB-C 数据线接板子的 USB 口 → `Terminal → Run Task` → **`P4-CAN: Flash (postv3)`**。
脚本会自动找 `/dev/cu.usbmodem*`，列出烧录地址，**等你输入 `y` 确认后**才开始写 Flash。

```bash
./scripts/flash.sh --list        # 先看有没有识别到板子
./scripts/flash.sh postv3        # 分段烧录（推荐，快）
./scripts/flash.sh --merged      # 烧 32MB 整片镜像（慢，新板首次可用）
```

> 我**没有**替你执行真实烧录，也不会自动擦除芯片。

---

## 3. 芯片版本：postv3 还是 prev3？

原README要求「确认你的 ESP32-P4 芯片版本」：

| 芯片丝印/版本 | 选哪个 | 编译选项 | 产物 |
| --- | --- | --- | --- |
| **v3.00 或更新**（2025 年后批次多为这个） | `postv3` | `ChipVariant=postv3`（CPU 400 MHz） | `*.OTA.REV3.X.bin` |
| v3.00 之前（工程样品） | `prev3` | `ChipVariant=prev3`（CPU 360 MHz） | `*.OTA.REV1.X.bin` |

版本判断方式：烧录用 esptool 读芯片信息（下面命令只读不写，安全）：

```bash
~/Library/Arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool \
  --chip esp32p4 -p /dev/cu.usbmodem1101 chip-id
```

`chip-id` 输出里会带 revision，`v3.x` 用 postv3。或者直接用 Arduino IDE 的板级菜单看。
**两个变体本次都已编译通过**，选错不会损坏板子，只是可能无法启动（表现为不断重启），换另一个重烧即可。

---

## 4. 板级编译选项（与原作者完全一致）

```
esp32:esp32:esp32p4:ChipVariant=postv3,FlashSize=32M,PartitionScheme=app3M_fat9M_16MB,
PSRAM=enabled,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio
```

| 选项 | 值 | 依据 |
| --- | --- | --- |
| `FlashSize` | 32M | 板上 U1 = W25Q256JVEIQ = 32 MB |
| `PSRAM` | enabled | U15 = ESP32-P4NRW32，片内 32 MB PSRAM |
| `PartitionScheme` | app3M_fat9M_16MB | 原作者 `Compile-P4-Variants.ps1` 使用的方案；应用上限 3 MB，实际占用 1.55 MB |
| `USBMode` / `CDCOnBoot` | hwcdc / cdc | 固件里有硬性 `#error` 检查，必须硬件 CDC + 上电使能 CDC；对应你板子 USB 接 GPIO24/25 |
| `FlashMode` | qio | 原作者配置（core 内部映射为镜像 dio + boot qio） |

---

## 5. 烧录地址与合并固件

分段烧录（`flash.sh` 默认）—— 与 Arduino-ESP32 core 生成的 `flash_args` 完全一致：

| 地址 | 文件 | 内容 |
| --- | --- | --- |
| `0x2000` | `*.ino.bootloader.bin` | 二级 bootloader |
| `0x8000` | `*.ino.partitions.bin` | 分区表 |
| `0xe000` | `boot_app0.bin` | OTA 选择位（重置为启动 app0） |
| `0x10000` | `*.OTA.REVx.X.bin` | 应用固件（app0，上限 3 MB） |

整片镜像（`--merged`）：`*.FULLFLASH.REVx.X.merged.bin` 直接写到 **`0x0`**，
内部包含上面 4 段并用 `0xFF` 填充到完整 32 MB。生成参数由 core 自动完成：
`--pad-to-size 32MB --flash-mode keep --flash-freq keep --flash-size keep`。

分区表实际内容（`app3M_fat9M_16MB`）：

```
nvs      0x009000  0x005000
otadata  0x00e000  0x002000
app0     0x010000  0x300000
app1     0x310000  0x300000
ffat     0x610000  0x9E0000
coredump 0xFF0000  0x010000
```

---

## 6. 烧录失败排查（USB 未识别 / 进不了下载模式）

macOS 上 ESP32-P4 走**原生 USB Serial/JTAG**，正常应出现 `/dev/cu.usbmodem*`。

1. **先确认端口**：`./scripts/flash.sh --list`
   - 只有 `cu.Bluetooth-Incoming-Port` / `cu.debug-console` → 系统根本没识别到板子。
2. **换线**：必须是**能传数据**的 USB-C 线（很多线只有电源）。
3. **确认供电**：你的板子是 12 V 自供电设计，USB 可能只做数据。
   烧录时请**同时给 12 V**（或先确认 VBUS 是否能给 5 V 轨供电），否则芯片可能带电不足。
   → 见 `docs/README.md 第 2 节` §6.2。
4. **手动进下载模式**：按住 **BOOT**（GPIO35，板上有 `boot+` 测试点），
   点一下 **RESET**（EN），松开 BOOT，再执行 `flash.sh`。
5. **权限/占用**：串口监视窗口（screen/Arduino 串口监视器）没关会占用端口，
   `Ctrl+]` 退出后再烧。
6. **看 esptool 报什么错**：
   - `Failed to connect` → 没进下载模式，重复第 4 步；
   - `No serial data received` → 线或供电问题；
   - `The chip is busy` / 掉线重启 → 供电不足（P4 峰值电流较大，USB 口带不动）。
7. **兜底**：用整片镜像 `./scripts/flash.sh --merged --yes`，先把芯片写成已知状态。

### 6.1 特殊故障：能连上芯片，但 Flash 读不出来

**症状**（实测于本板，2026-11-24）：

- `esptool chip-id` 正常：`ESP32-P4 (revision v3.2)`，MAC 能读出来 → 芯片本身活着；
- 但 `esptool flash-id` 返回：

  ```
  Manufacturer: 7f
  Device: ffff
  Detected flash size: Unknown
  ```

- 写 flash 时小文件报 `100%` 后掉流，大文件（1 MB 应用）在数据阶段就断，
  读回 `read-flash` 直接 `No more data to read`。

**含义**：`0x7F / 0xFFFF` 不是任何真实 Flash 的 JEDEC ID（W25Q256JV 应该是 `EF 40 19`），
这是 MISO 悬空/无响应的典型读数。**用 stub 和 ROM loader（`--no-stub`）读出来一样**，
所以不是 esptool 或传输的问题，是板上外部 SPI Flash 没被芯片识别。

**本板确认的原因：U1 尚未贴片。** 没装 Flash 时现象与此完全一致 —— 芯片仍能枚举出
USB-Serial/JTAG 并能读 MAC（ROM bootloader 跑在片内 ROM，不依赖 Flash），
但任何 Flash 读写都会失败。焊上 U1 后重新执行 `flash-id`，出现
`Manufacturer: ef  Device: 4019  Detected flash size: 32MB` 即恢复正常。

> 顺带：这次写入中途反复报 `Serial data stream stopped` / `No more data to read`，
> 大概率也是 Flash 无应答导致的（esptool 写完等不到芯片回应），而不是 USB 链路不稳。
> Flash 装好后再观察；若仍反复掉流，再按 §6 的线材/供电方向排查。

**片子是外部那颗 U1 = W25Q256JVEIQ**（ESP32-P4NRW32 只有片内 PSRAM，没有片内 Flash）。按网表逐点查：

| 检查点 | 期望 |
| --- | --- |
| U1 是否已贴片、方向是否正确（WSON-8 的 1 脚定位点） | 已贴、方向对 |
| U1 散热焊盘 EP 是否可靠接地 | 通 |
| U1 VCC | 3.3V（同 `VDD_FLASHIO`，P4 pad 30） |
| `P4_FLASH_CS`（U1.1 ↔ P4 pad 27） | 空闲时为高；`R3` 10 kΩ 上拉到 3.3 V，**R3 缺失会让 CS 悬空、Flash 永不应答** |
| `P4_FLASH_CK/D/Q`（U1.6/5/2 ↔ pad 32/33/28） | 通，无虚焊 |
| `P4_FLASH_WP/HOLD`（U1.3/7 ↔ pad 29/31） | 通，且不应被拉低（拉低会锁死写保护/暂停） |

先查 `R3` 和 EP 接地，这两个最容易漏。

### 6.2 本板实况（2026-11-24 实测记录）

这块自制板在调试过程中确认了两件与文档默认值不同的事，记录下来避免重复踩坑。

**(1) 板上装的是 2 MB Flash，不是 BOM 里的 32 MB**

`esptool flash-id` 读出 `Manufacturer: ef / Device: 4015` = **W25Q16（2 MB）**，
而 BOM 指定的是 W25Q256JVEIQ（32 MB，ID 应为 `EF 4019`）。
原 32 MB 配置的分区表（`app3M_fat9M_16MB`）远超 2 MB，必然写不进去。

**已决定长期用 2MB**（不上无线模块，固件约 1.1MB，1.25MB 分区够用，余量约 211KB）。
所以 `build.sh` / `flash.sh` 的**默认值已改成 2MB / minimal**，日常直接：

```bash
./scripts/build.sh postv3
./scripts/flash.sh postv3 --no-stub
```

若将来换成 32MB 片子，用环境变量切回上游标准配置（届时必须整片烧 `0x0`，因为分区表变了）：

```bash
FLASH_SIZE=32M PARTITION_SCHEME=app3M_fat9M_16MB ./scripts/build.sh postv3
./scripts/flash.sh postv3 --no-stub --flash-size 32MB --merged
```

`minimal` 布局下 app0 = 0x140000（1.25 MB），当前固件 1,093,664 B 占 **83%**，还有约 211 KB 余量。
换成正确的 32 MB 片子后，不带环境变量重新编译即可回到原配置。

**(2) 这块板的 stub flasher 写不进去，必须用 ROM loader**

用默认（stub）烧录时，每段都报：

```
Writing at 0x000076d0 ... 100.0%
A fatal error occurred: Write failed, the written flash region is empty.
```

即写完后回读校验发现区域是空的。改成 `--no-stub`（ROM loader）后四段全部写入成功。
`scripts/flash.sh` 已加 `--no-stub` 和 `--flash-size` 两个选项：

```bash
./scripts/flash.sh postv3 --no-stub --flash-size 2MB
```

代价是慢一些（1.09 MB 应用约 45 秒，约 195 kbit/s），但稳定。
另外这块板的 USB-JTAG 连接**间歇性**连不上（大约一半概率报
`Failed to connect / No serial data received`），重试一两次即可，不是故障。

**(3) 烧录成功的验证结果**

```
[P4-CAN] USER_CONFIG: Wi-Fi/Web disabled, esp-hosted not initialized
[P4-CAN] USER_CONFIG: boot profile forced to 5
[P4-CAN] USER_CONFIG: summon heartbeat override enabled=1 value=2
[P4-CAN] CAN1 BODY TWAI0 TX20 RX21 NORMAL: ESP_OK
[P4-CAN] CAN2 CHASSIS TWAI1 TX22 RX23 NORMAL: ESP_OK
[P4-CAN] CAN3 PARTY TWAI2 TX26 RX27 NORMAL: ESP_OK
@HELLO,2,v3.35.2
```

三路 CAN 控制器全部初始化成功，车型/召唤/EAP 开关全部生效，
最后一行 `@HELLO` 是 USB CAN logger 的协议头。
**这仍然只是"固件在跑"，不等于车载功能已验证。**

---

## 7. 上电后看日志

```bash
./scripts/monitor.sh          # 115200，Ctrl+] 退出
```

⚠️ **重要**：这个固件会在 `BOOT OK` 之后启动 **USB CAN logger**，
串口上跑的是**二进制 CAN 帧协议**，不是纯文本。
所以：

- 只能看到**复位瞬间**的文本启动日志（`=== P4-CAN Unified BOOT ===` 之类需要打开诊断宏）；
- 之后就变成二进制数据流，用普通串口终端看会是乱码 —— 这是**正常现象**，不是坏了。

另外你板子上 `U0TXD/U0RXD`（GPIO69/70）在最新网表里是悬空的，**没有 UART0 备用调试口**，
USB 是唯一的日志通道。

---

## 8. VS Code 满屏红波浪线（IntelliSense 报找不到头文件）

**症状**：能编译，但编辑器里 `.ino` 开头一堆错误：

```
无法打开 源 文件 "freertos/FreeRTOS.h" (dependency of "Arduino.h")
无法打开 源 文件 "driver/twai.h"
未定义标识符 "BaseType_t" / "esp_err_t" / "uint32_t" 不是类型名 / 应输入声明
```

**这不是代码错误**，是 VS Code C/C++ 扩展（cpptools）解析不了 Arduino-ESP32 的头文件搜索方式：
核心用 GCC 的 `-iprefix` + `-iwithprefixbefore` 挂 ESP-IDF 组件路径
（`esp32p4-libs/3.3.12/flags/includes`），cpptools 不支持这两个选项。

**解决**（已配好，一般不需要你动手）：

```bash
./scripts/build.sh postv3 --ide-db      # 编译并刷新 IntelliSense 配置
# 或者单独刷新
python3 scripts/gen_intellisense.py
```

它会把 383 个头文件目录展开成绝对路径 + 41 个宏写入 `.vscode/c_cpp_properties.json`，
并自检那 14 个关键头文件能否解析（成功会打印 `✅ 关键头文件全部可解析（14/14）`）。
之后按 `Cmd+Shift+P → C/C++: Reset IntelliSense Database`，或 `Developer: Reload Window`。

> 注意 `ESP_PLATFORM` 这个宏必须有：NimBLE 的 `syscfg.h` 靠它才去包含 `esp_nimble_cfg.h`，
> 缺了会报 `MYNEWT_VAL_BLE_GATT_CSFC_SIZE 未定义`。

---

## 9. 其它常用命令

```bash
./scripts/build.sh all                  # postv3 + prev3 都编
./scripts/build.sh postv3 --clean       # 清缓存重编（约 5 分钟）
./scripts/build.sh postv3 --ide-db      # 生成 build/compile_commands.json
./scripts/flash.sh --erase              # 整片擦除（会清空 NVS/车型配置，慎用）
./scripts/flash.sh --merged --yes       # 整片镜像 + 跳过确认
```

板级硬件资料（未随发布提供） 里放着核对引脚用的网表、BOM 和芯片手册，改板前请以
`板级网表`（最新）为准。

---

## 10. 已知阻塞项（硬件，非编译）

你自制的 PCB（按最新网表）**没有 ESP32-C6/C5 无线从机**，而 ESP32-P4 自身没有 Wi-Fi。
原固件必须连上 `192.168.4.1` 的网页面板选车型才会启动 CAN，因此当前硬件走不到 CAN 工作状态。

详见 `docs/README.md 第 2 节` §6.1 的三种可选方向。
**编译和烧录不受影响**，这一条只影响把板子真正接到车上之后的功能。
