# P4-CAN 低成本无无线方案（No-WiFi / No-BLE / 2MB Flash）

> 基于 [06066060606060/P4can-universal-unlock](https://github.com/06066060606060/P4can-universal-unlock) **v3.35.3**（commit `81a4210`）
> 本文档面向开源发布，完整记录本方案相对上游的**全部改动、动机、实现方式与实测结果**。
> 可直接作为衍生仓库的 README 使用。

---

## 0. 一句话概括

**去掉 Wi-Fi / BLE，用编译期开关代替网页面板，让一块只有 ESP32-P4 + 3 颗 CAN 收发器 + 2MB Flash 的低成本板子，跑起上游的全部车辆功能。**

固件从 1.55MB 降到 **1.10MB**，功能一个不少——因为砍掉的 450KB 全是无线与网页那条链。

---

## 1. 为什么要做这个方案

上游方案假定硬件是「ESP32-P4 + ESP32-C6/C5 无线从机 + 32MB Flash」的成品板，所有功能通过
`192.168.4.1` 网页面板配置。这带来三个成本：

| 上游依赖 | 成本 |
| --- | --- |
| ESP32-C6/C5 模块 | 额外器件 + SDIO 布线 + 从机固件维护 |
| 32MB Flash | 比 2MB 贵 |
| 网页配置 | 每次改设置要连 Wi-Fi、开浏览器 |

如果你的板子**本来就没有无线模块**（例如自制板只画了 P4 + 收发器），上游固件会卡在
「车型未选择」的配车模式，**CAN 永远不会初始化**——因为选车型的唯一入口是网页。

本方案解决两件事：
1. **把配置从"运行期网页写入"改成"编译期代码开关"**，没有网页也能配车；
2. **砍掉常供电的无线器件，把整机电流降到约一半**——见 §1.5 的计算。

### 适用场景

- 自制 ESP32-P4 板，没有 C6/C5
- 想要 STM32 那种「改 `#define` → 编译 → 烧录」的工作流
- 不需要 OTA、不需要手机 App

### 不适用场景

- 需要网页面板实时调参（本方案 Wi-Fi 已编译期移除）
- 需要 OTA 双槽（2MB 的 `minimal` 分区只有单 app 槽）

---

## 1.5 ★ 为什么去掉 Wi-Fi/BLE：功耗，而不只是"板子没模块"

**这是本方案的核心动机。** 板子从车上取 12V 常电时**无法真正休眠**——固件里的
"vehicle CAN sleep" 只是逻辑状态，MCU 始终 400MHz 全速运行，收发器 S 脚硬件接地也无法待机。
既然待机电流降不下去，**唯一的办法就是砍掉耗电的器件**。

上游方案里最耗电的不是 P4，而是**那颗额外常供电的 ESP32-C6/C5**。
只要 Wi-Fi AP 开着，它的射频就长期处于接收态。

### 数据手册依据

| 器件 | 工况 | 电流 | 出处 |
| --- | --- | --- | --- |
| ESP32-C6 | Wi-Fi RX (HT20) | **78 mA** @3.3V | C6 数据手册 v1.6 Table 5-7 |
| ESP32-C6 | Wi-Fi TX 峰值 | 252–354 mA | 同上 |
| ESP32-C6 | Modem-sleep, CPU 运行 160MHz | 38 mA | Table 5-10 |
| ESP32-P4 | 400MHz 双核 WAITI，外设时钟全开 | **56 mA** | P4 数据手册 v0.7 Table 5-7 |
| ESP32-P4 | 400MHz 双核 while(1)，外设全开 | 112 mA | 同上 |
| ESP32-P4 | 400MHz 双核 32 位数据访问 | 150 mA | 同上 |
| TCAN1044A | 隐性（总线空闲）typ | **4.5 mA** @5V | TI 数据手册 Supply current |
| TCAN1044A | 显性（发送中）typ | 45 mA | 同上 |

> P4 数据手册注明：访问 flash/PSRAM 时电流会更高。
> 收发器只在**自己发送**时才进入显性；持续接收不增加自身供电电流，所以常驻按隐性 4.5 mA 计。

### 计算模型

假设（**非实测，需自行验证**）：

- P4 负载分配：静止时本方案 60 mA / 上游 75 mA（多出 hosted 驱动 + WiFi 栈 + WebServer）；
  行车时 130 mA / 145 mA。落在数据手册 56–150 mA 区间内。
- C6 在 softAP 常开、无客户端时按 **85 mA** 计（RX 78 mA + 从机 CPU 开销）。
- 电源效率：12V→5V 取 0.85，5V→3.3V 取 0.88。
- 三路收发器均按隐性 4.5 mA @5V。

### 结果

| 场景 | 本方案 @12V | 上游 @12V | **降幅** |
| --- | --- | --- | --- |
| **车醒着·静止**（寄生电流场景） | **28.7 mA** | 65.4 mA | **≈ 56%** |
| 行车中（约 8200 帧/秒） | 54.4 mA | 91.2 mA | ≈ 40% |

停放耗电（按车醒着·静止）：

| | 每天 | 每周 |
| --- | --- | --- |
| 本方案 | **0.69 Ah** | **4.8 Ah** |
| 上游 | 1.57 Ah | 11.0 Ah |

**停放一周相差约 6 Ah。** 对 12V 小电瓶来说这是实打实的差别。

### 三点必须说清楚

1. **这是计算估算，不是实测。** 射频部分有数据手册支撑，P4 的负载分配是估算。
   实际值请在 12V 输入串电流表测量，分别记录"车醒着静止"和"行车中"两个状态。
2. **省电的大头是删掉 C6 这颗芯片**，不是固件优化。P4 侧节省的 10–20 mA 只是次要项。
3. **两个方案都不会真正休眠。** 想做到零寄生电流，仍需硬件断电——
   例如从手套箱 USB 取 5V（随车休眠断电，注意先拆 L2 避免反灌 U6，见 §9）。
   本方案的意义是：**在必须常供电的前提下，把代价降到约一半。**

---

## 2. 硬件方案

### 2.1 实测验证过的器件

| 位号 | 型号 | 说明 |
| --- | --- | --- |
| U15 | **ESP32-P4NRW32** | QFN-104，片内 32MB PSRAM，**无片内 Flash** |
| U1 | **W25Q16（2MB）** | WSON-8。上游 BOM 指定 W25Q256（32MB），本方案实测 2MB 足够 |
| U3/U7/U8 | **TCAN1044AVDRQ1** ×3 | 5V CAN FD 收发器，VIO=3.3V |
| U6 | LMR38020 | 12V → 5V |
| U4 | TPS62902 | 5V → 3.3V |
| U5 | TLV62569 | 5V → 1.2V（P4 内部 DCDC） |

> **注意**：上游 README 推荐 SN65HVD230，本板用的是 TCAN1044A。两者 TXD/RXD 引脚兼容，
> 但 TCAN1044A 是 5V 器件带独立 VIO，**S 脚（静默控制）经 0Ω 接地 = 固定正常模式**，
> 固件没有任何收发器使能/待机 GPIO。

### 2.2 三路 CAN 引脚

| 物理总线 | TWAI | TX | RX | P4 焊盘 |
| --- | --- | --- | --- | --- |
| CAN1 BODY | 0 | GPIO20 | GPIO21 | 22 / 23 |
| CAN2 CHASSIS | 1 | GPIO22 | GPIO23 | 24 / 25 |
| CAN3 PARTY | 2 | GPIO26 | GPIO27 | 55 / 56 |

焊盘→GPIO 对照来自 ESP32-P4 数据手册 Table 2-1。

**一个上游的坑**：`pin_config.h` 里 `CAN_CHASSIS_TX/RX` 写的是 **50/49**，与多数板子不符。
但固件在 CAN 初始化前会调用 `p4CanGpioLoadAtBoot()`，用 NVS 或
`p4CanGpioDefaultsPure()`（`{20,22,26}/{21,23,27}`）**无条件覆盖**，所以实际生效的是 22/23。
本方案的 `CFG_CAN_*` 开关排在覆盖之后，不会被冲掉。

### 2.3 USB

USB-C 的 D+/D− 经 22Ω 接 **GPIO24/GPIO25**（P4 的 USB FS PHY），即
**USB Serial/JTAG**，用于烧录与日志。高速 USB 引脚（pad 49/50）未使用。

⚠️ **USB VBUS 未接入任何电源轨**（只挂 47nF + 1MΩ），所以**板子必须外部供电**，
只插 USB 不会上电。这是自制板常见的"插上没反应"原因。

---

## 3. 快速开始

### 3.1 环境

```bash
brew install arduino-cli
arduino-cli core install esp32:esp32@3.3.12
```

### 3.2 编译

```bash
./scripts/build.sh postv3      # 芯片 v3.00 及更新
./scripts/build.sh prev3       # 芯片 v3.00 之前
```

默认已按 **2MB / minimal** 配置。等价 FQBN：

```
esp32:esp32:esp32p4:ChipVariant=postv3,FlashSize=2M,PartitionScheme=minimal,
PSRAM=enabled,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio
```

> `USBMode=hwcdc` + `CDCOnBoot=cdc` 是**硬性要求**，主 `.ino` 里有 `#error` 检查。

### 3.3 烧录

```bash
./scripts/flash.sh postv3 --no-stub
```

**`--no-stub` 在本板是必需的**：用默认 stub flasher 会报
`Write failed, the written flash region is empty`，改走 ROM loader 才写得进去。
代价是慢一些（1.1MB 约 45 秒）。

日常更新只烧应用（保留 NVS）：

```bash
esptool.py --chip esp32p4 -p /dev/cu.usbmodem101 -b 460800 \
  --no-stub --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 2MB \
  0x10000 P4-CAN-Universal-v3.35.3.OTA.REV3.X.bin
```

### 3.4 烧录地址

| 地址 | 内容 |
| --- | --- |
| `0x2000` | bootloader |
| `0x8000` | 分区表 |
| `0xe000` | boot_app0（OTA 选择位） |
| `0x10000` | 应用 |
| `0x0` | 整片镜像（**会擦掉 NVS**） |

---

## 4. 配置区：关键字对照表

全部开关在 **`user_config.h`**，改完重新编译即可，不需要网页。

**取值约定**：`-1` = 不干预（沿用 Flash/NVS 值）、`0` = 强制关、`1` = 强制开。
**编译期优先于网页**：每次开机在 NVS 加载完成后覆盖一次。

### 4.1 车型与硬件

| 关键字 | 当前值 | 作用 |
| --- | --- | --- |
| `P4CAN_USER_CONFIG` | `1` | 总开关。设 `0` 完全回到上游行为 |
| `CFG_BOOT_PROFILE` | **`5`** | 车型。`1`=Model Y L，`2`=Y Juniper，`3`=Y Legacy，`4`=3 Highland，`5`=3 Legacy。**唯一会写 NVS 的开关** |
| `CFG_TURN_SIGNAL` | `1` | `1`=有拨杆，`2`=无拨杆。**选 Highland(4) 时必填**，否则编译报错 |
| `CFG_CAN_BODY_TX/RX` | `-1` | 默认 20/21 |
| `CFG_CAN_CHASSIS_TX/RX` | `-1` | 默认 22/23 |
| `CFG_CAN_PARTY_TX/RX` | `-1` | 默认 26/27 |
| `CFG_CAN_LISTEN_ONLY` | **`0`** | `1`=只收不发（台架首测建议），`0`=正常收发 |

### 4.2 任务开关（本方案的核心）

| 关键字 | 当前值 | 作用 |
| --- | --- | --- |
| `CFG_ENABLE_WIFI_WEB` | **`0`** | 跳过 esp-hosted 初始化、不建 web 任务。**省下约 450KB** |
| `CFG_ENABLE_BLE_TASK` | **`0`** | 不建 S3XY BLE 任务 |
| `CFG_ENABLE_USB_LOGGER` | `1` | USB CAN 日志（文本行协议） |

### 4.3 车辆功能

| 关键字 | 当前值 | 作用 | 底层机制 |
| --- | --- | --- | --- |
| **`CFG_CHINA_RESTORE`** | **`1`** | **FSD 档总闸** | CAN2 `0x331` 的 `data[0]` 低 6 位 → `0x1B`(SELF_DRIVING) |
| `CFG_AUTO_LANE_CHANGE` | **`1`** | 自动变道（打灯变道） | `0x293` `data[3]` 低 2 位 → 1，重算校验和 |
| `CFG_AUTO_BLINKER` | **`1`** | 自动转向灯 | NOA 变道时自动打灯 |
| `CFG_TLSSC` | **`1`** | 红绿灯/停车标志控制 | `0x3FD` mux0 bit38/39 → 1 |
| `CFG_SUMMON_HEARTBEAT` | **`1`** | 召唤心跳覆盖（±85m） | `0x3F8` `data[0]` bit2-3 |
| `CFG_SUMMON_HEARTBEAT_VALUE` | `-1` | 心跳值 0..3，`-1`=默认 2 | 上游未文档化各值含义 |
| `CFG_LAB_MENU` | **`1`** | LAB 总闸 | **召唤解锁的前置条件** |
| `CFG_R79_UNLOCK` | `-1` | EU/R79 解锁 | `0x3FD` mux1 bit47。**上游默认已开** |
| `CFG_NAG` / `CFG_NAG_MODE` | `-1` | 方向盘扭力提示抑制 | |
| `CFG_TLSSC_HIGHWAY_GATE` | `-1` | 实验性 `0x238` 高速门控 | |
| `CFG_ISA_SUPPRESS` | `-1` | ISA 提示音抑制 | |
| `CFG_FSD_VISUAL` | `-1` | FSD 可视化（**会干扰 AP 与泊车**） | |
| `CFG_CONTINUOUS_AP` / `_METHOD` | `-1` | 连续 AP。手势：1=拨杆双击 2=滚轮单击 3=滚轮双击 | |
| `CFG_SMARTSHIFT` | `-1` | 自动换挡 | |
| `CFG_PREHEAT` | `-1` | 电池预热 | |
| `CFG_ULC_NO_CONFIRM` | `-1` | 免确认变道（上游注明 EU 无效） | |
| `CFG_AUTO_LANE_CHANGE_BUS` | `-1` | 0x293 发到哪路：1/2/3，默认 3=两条都发 | |

### 4.4 ★ 最重要的一条：`CFG_CHINA_RESTORE`

**这是自动泊车和召唤的总闸，不是三个独立功能。**

```c
// service_settings.h
out.data[0] = (out.data[0] & 0xC0u) | 0x1Bu;   // DAS_autopilotConfig → SELF_DRIVING
```

`0x331` 是车辆判断自身自动驾驶能力档位的帧。改成 `0x1B` 后，车认为自己具备 FSD 档能力，
**自动泊车、召唤、TLSSC 这些才会出现**。上游默认关闭，网页里在 Service 区。

> 本方案开发过程中曾误判「项目没有泊车功能」——因为搜 `autopark` 搜不到。
> 实际它不叫这个名字，而是通过抬高能力档位间接放开。**这是本方案最关键的一条发现。**

---

## 5. 当前启用状态与实测结果

### 5.1 已实测确认有效（实车）

| 功能 | 结果 |
| --- | --- |
| 打灯变道（自动变道 + 自动转向灯） | ✅ 有效 |
| 自动泊车 | ✅ 有效（开 `CFG_CHINA_RESTORE` 后） |
| 召唤 | ✅ 有效（同上） |
| 三路 CAN 通信 | ✅ 行车实测约 8,200 帧/秒 |

### 5.2 已启用但未逐项实车验证

TLSSC、R79/EU 解锁（上游默认开）、召唤心跳覆盖的 ±85m 效果。

### 5.3 编译数据

```
应用固件  1,098,866 B / 1,310,720 B (83.8%)   余量 207 KB
静态 RAM    155,560 B /   327,680 B (47%)
```

**功能开关不改变固件大小**——所有功能代码都已链接进去，开关只是运行期标志。
唯一的增长来源是上游新增模块（3.35.3 的 Lighting 约 3KB）。

---

## 6. 相对上游的全部代码改动

> 以下为**完整清单**，可用 `diff -r` 对上游 `81a4210` 复核。
> 除新增文件外，**没有删除或改写任何上游原有逻辑**，所有插入点都在 `#if` 内或为追加调用。

| 文件 | 类型 | 改动量 |
| --- | --- | --- |
| `user_config.h` | **新增** | 配置区，纯 `#define` |
| `user_config_apply.h` | **新增** | 生效逻辑，5 个 `static inline` 函数 |
| `eap_anti_restart.h` | **新增（原样移植）** | 来自上游未合并分支 `tlssc-fix`，逐字节一致 |
| `sketch.yaml` | **新增** | arduino-cli 工程文件，固定 `default_fqbn` |
| `P4-CAN-Universal-v3.35.3.ino` | 修改 | **+58 行**，10 处钩子 |
| `can_usb_logger_task.h` | 修改 | **+22 行**，新增 `LOGGER DIAG` 命令 |
| `can_runtime.h` | 修改 | **+3 行**，防重启修复的观察/回放调用 |
| `service_settings.h` | 修改 | **+2 行**，引入 `eap_anti_restart.h` |

### 6.1 主程序的 10 处钩子

| 位置 | 插入内容 | 为什么必须在这里 |
| --- | --- | --- |
| 文件头 | `#include "user_config.h"` | 配置区入口 |
| include 末尾 | `#include "user_config_apply.h"` | 必须最后，依赖前面所有全局量 |
| `p4NetworkPrepare()` 处 | `#if P4UCFG_WEB_ENABLED` | 关闭时完全跳过 esp-hosted |
| NVS 车型探测**之前** | `p4UserCfgApplyProfile()` | 启动流程要先读到车型，晚了就进配车模式 |
| `p4CanGpioLoadAtBoot()` **之后** | `p4UserCfgApplyCan()` | 该函数会覆盖引脚，必须排在它后面 |
| `preheatMenuLoad()` 之后 | `p4UserCfgApplyFeatures()` + `ApplyTaskGates()` | 所有 `xxxLoad()` 已完成，且早于 `if(labMenuEnabled)` 分支 |
| 3 处 web 启动点 | `#if P4UCFG_WEB_ENABLED` | 不建 web 任务 |
| `BOOT OK` 之后 | `#if P4UCFG_USBLOG_ENABLED` | 不启动 USB 日志任务 |
| `loop()` | `p4UserCfgMaintain()` | 每秒自愈（见 §7.2） |
| `setup()` | `eapNrLoad()` | 防重启缓存加载（见 §7.3） |

### 6.2 编译期安全护栏

`user_config.h` 末尾有 5 条 `#error` / `#warning`，防止配出"烧进去悄悄不生效"的组合：

- Highland 未指定转向变体 → `#error`（上游 `vehicleProfileSave()` 会校验失败）
- 召唤开启但 LAB 未开 → `#error`（`summonHeartbeatOverrideApply()` 内部强制校验）
- 关网页且车型为 0 → `#warning`（设备将无任何可用接口）
- 关网页且关 USB 日志 → `#warning`（无输出通道）
- 取值越界 → `#error`

---

## 7. 三个修复：问题、原理、实现

### 7.1 无无线时无法进入 CAN 工作状态

**问题**：上游启动流程中，若 NVS 无有效车型 → 进入 `PROFILE SETUP MODE`，
只启动 Wi-Fi/Web，**不初始化 CAN**。而选车型的唯一入口是网页。没有无线模块 = 死锁。

**实现**：`p4UserCfgApplyProfile()` 在 NVS 探测**之前**，用上游自带的
`vehicleProfileSave(id, topology, turn)` 把编译期车型写入 NVS。
语义与网页选车型完全一致；**已一致时不重复写**，避免 Flash 磨损。

```c
if (readOk && haveId == wantId && haveTurn == resolvedTurn) return true;  // 不写
return vehicleProfileSave(wantId, wantTopology, wantTurn);
```

### 7.2 召唤覆盖在 CAN 恢复后被静默清零

**问题**：上游把召唤心跳覆盖设计为 RAM-only：

```c
// vehicle_logic.h
// Deliberately RAM-only: every boot and CAN full-recovery starts disabled.
```

`can_runtime.h` 的恢复流程里调用 `summonHeartbeatOverrideResetUnderTxBarrier()` 清零。
配置区只在开机灌一次 → **行车中一旦发生 CAN 恢复，召唤就永久失效**。

**实现**：在主 `loop()`（原本空闲，每 1000ms 一次）里做周期性自愈：

```c
static inline void p4UserCfgMaintain() {
  if (!labMenuEnabled) return;
  bool enabled; /* 读 summonHeartbeatOverrideEnabled */
  if (!enabled) summonHeartbeatOverrideApply(true, v);   // 重新灌入
}
```

**为什么放 `loop()` 而不是恢复流程里**：`summonHeartbeatOverrideApply()` 自己会取
`canTxBarrierMutex`，在已持锁的恢复流程中调用会**死锁**。

### 7.3 每次上电都要手动重启中控屏

**问题**：`chinaRestoreEcho()` 是**被动**的——只在车发出 `0x331` 时才改写一帧回发。
而 **MCU 只在自己启动那一刻锁存一次 DAS 配置**。若那一刻总线上没有改过的 `0x331`，
它就锁存"禁用"状态，于是每次上电都要手动重启屏幕。

**来源**：上游未合并分支 `tlssc-fix`（PR #15，`baicyber/fix/eap-anti-restart`），
本方案**原样移植** `eap_anti_restart.h`，未作修改。

**实现**：
1. 观察到 stock `0x331` → 打补丁 → 缓存进 NVS（`features/eapFrame`，每次上电最多写一次）
2. 开机 `eapNrLoad()` 读回缓存
3. CAN B 服务循环里 `eapNrTick()`，以 **10Hz 回放 60 秒**（上限 600 帧）
4. 车自己发出新鲜 `0x331`（2 秒内）→ **立刻让位**，回到被动模式

**安全性**（移植前逐行审查过）：
- 非阻塞取 TX 屏障（`xSemaphoreTake(..., 0)`），拿不到即计"让位"返回，**不死锁**
- 走标准 `p4CanSendFrame()`，沿用全部现有闸门（administrative hold / USB passive /
  三路就绪 / epoch 匹配 / Summon 准入）
- 只在 `chinaRestoreEnabled` 打开时工作，只发 CHASSIS

**已知行为**：缓存要跑车一轮才学到。**烧完第一次上电仍可能需要重启屏**，第二次起才免。
整片烧（`0x0`）会擦掉 NVS 从而丢缓存，所以日常更新建议只烧应用（`0x10000`）。

---

## 8. 诊断：`LOGGER DIAG`

本方案没有网页，所有运行期状态通过 USB 文本协议读取。新增命令 `LOGGER DIAG`
（**纯追加，不影响上游原有协议**）：

```
@DIAG,summon,<启用>,<值>,<已应用>,<被拦>,<发送失败>,
      tx,<CAN1接受>,<CAN1拒绝>,<CAN2接受>,<CAN2拒绝>,<CAN3接受>,<CAN3拒绝>,
      restore,<启用>,<成功>,<失败>,
      stock,<原厂心跳值>,<有效>,
      eap,<缓存有效>,<回放成功>,<回放失败>,<让位>,<本轮帧数>
```

判读要点：

| 现象 | 含义 |
| --- | --- |
| `已应用=0` 且 `被拦=0` | 从没收到过 stock `0x3F8`（车可能休眠） |
| `已应用=0` 但 `被拦>0` | 收到了但闸门没开 |
| `CAN2 接受>0` | 固件确实在往总线发帧 |
| `restore 成功>0` | FSD 档覆盖帧已发出 |
| `eap 回放成功>0` | 开机回放在工作，不需要重启屏 |

配套的采集/诊断脚本见 `scripts/capture_can.py`。

---

## 9. 已知限制与坑

| 项 | 说明 |
| --- | --- |
| **无 OTA** | `minimal` 分区只有单 app 槽；且无 Wi-Fi |
| **无文件系统** | 固件不使用 FFat/SPIFFS，看板是 PROGMEM 内嵌 |
| **无低功耗休眠** | 代码里的 "vehicle CAN sleep" 只是**逻辑状态**（静默 5 秒后不再当故障），MCU 始终全速运行；收发器 S 脚硬件接地，无法待机 |
| **stub flasher 写不进** | 本板必须 `--no-stub` |
| **USB-JTAG 间歇连不上** | 约一半概率 `Failed to connect`，重试即可 |
| **`pin_config.h` CHASSIS 50/49** | 与实际接线不符，靠运行期覆盖纠正 |
| **2MB 余量 207KB** | 够用，但每次升级留意占用百分比 |

### 供电建议（寄生电流）

板子接车辆常电会持续耗电。推荐**从手套箱 USB 取 5V**（随车休眠断电）。
但注意：`+5V` 轨挂着 U6 的输出电感 L2，直接并入会**反灌 U6**。
稳妥做法是**拆掉 L2**（或 U6 本身）再注入 5V，下游 3.3V/1.2V/收发器供电完全不受影响。

---

## 10. 复现与验证

```bash
# 1. 编译（默认 2MB）
./scripts/build.sh postv3
# 期望：Sketch uses 1098866 bytes (83%)

# 2. 确认改动量（对上游 81a4210）
diff -r <upstream> firmware/P4-CAN-Universal-v3.35.3
# 期望：仅 4 个新增文件 + 4 个文件少量追加

# 3. 烧录后看启动日志
# 期望：
#   USER_CONFIG: Wi-Fi/Web disabled, esp-hosted not initialized
#   USER_CONFIG: boot profile forced to 5
#   CAN1 BODY TWAI0 TX20 RX21 NORMAL: ESP_OK
#   CAN2 CHASSIS TWAI1 TX22 RX23 NORMAL: ESP_OK
#   CAN3 PARTY TWAI2 TX26 RX27 NORMAL: ESP_OK
#   @HELLO,2,v3.35.3
```

**可重复编译**：`build.sh` 导出 `SOURCE_DATE_EPOCH`（取源码最新 mtime），
固定固件内嵌的 `__DATE__`/`__TIME__`，保证**同目录、同源码 → SHA256 不变**（已实测两次全量重编一致）。

⚠️ **两个边界**：
- 换目录编译时，固件内嵌的 `__FILE__` 绝对路径长度改变，SHA 随之不同
  （实测同一份源码在两处编译相差 40 字节）。复现校验请在相同路径下进行。
- 正因为固件内嵌编译者本机路径（`/Users/<用户名>/...`），
  **建议只发布源码、不发布预编译 `.bin`**，避免泄露目录结构。GPL 本来也要求提供源码。

---

## 11. 安全声明

本固件会向车辆 CAN 总线**主动注入帧**，可能影响转向、变道、召唤等实际动作。

- 首次实车请在**空旷、低车速、无其他车辆**的场地，手扶方向盘随时接管
- 台架首测建议 `CFG_CAN_LISTEN_ONLY 1`（只收不发）
- **编译通过 ≠ 功能正确**，所有车辆行为需自行验证
- 部分功能依赖车辆自身具备相应能力与订阅，固件不能凭空创造功能

---

## 12. 许可与致谢

- 上游项目：**GNU GPL v3 or later**，本衍生方案沿用同一许可
- `eap_anti_restart.h` 来自上游 PR #15（`baicyber/fix/eap-anti-restart`），原样移植，版权归原作者
- 感谢上游作者及社区贡献者

> 上游仓库根目录未提供 LICENSE 文件，仅子模块内有 `automatic_shift/LICENSE`
> 与 `third_party/tinycrypt/LICENSE-ESP-IDF`。开源发布时建议补齐完整 LICENSE 文本。
