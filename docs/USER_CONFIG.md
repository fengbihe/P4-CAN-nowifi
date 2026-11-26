# USER_CONFIG — 编译期用户配置区（不用手机 App 就能配好一切）

像 STM32 工程那样，在代码里改几个 `#define`，重新编译烧录，功能就按你写的方式跑。
**不需要手机 App，不需要连 192.168.4.1 的网页面板。**

开关文件：`firmware/P4-CAN-Universal-v3.35.3/user_config.h`

---

## 1. 怎么用

```bash
# 1. 改开关
open firmware/P4-CAN-Universal-v3.35.3/user_config.h

# 2. 编译（或 VS Code 里 Cmd+Shift+B）
./scripts/build.sh postv3

# 3. 烧录
./scripts/flash.sh postv3
```

VS Code 里改完直接编译即可，改 `user_config.h` 会触发重新编译，不需要 `--clean`。

---

## 2. 取值约定（三态开关）

| 值 | 含义 |
| --- | --- |
| `-1` | **不干预**，沿用设备 Flash（NVS）里保存的值 / 之前网页里设的值 ← 默认 |
| `0` | **强制关闭** |
| `1` | **强制开启** |

**编译期优先于网页设置**：每次开机在 NVS 加载完成后覆盖一次。
想交回网页控制，把对应项改回 `-1`。

---

## 3. 给这块自制 PCB 的推荐配置（复制粘贴即可）

你这块板子的情况：三路 CAN 接法 = 1BODY / 2CHASSIS / 3PARTY，引脚 GPIO20/21、22/23、26/27；
**没有 Wi-Fi 从机（无 C6/C5）**。所以推荐：

当前固件已按「老款 Model 3（Legacy，有转向拨杆）+ EAP + 召唤」配置好：

```c
#define CFG_BOOT_PROFILE                5      // Model 3 Legacy
#define CFG_TURN_SIGNAL                 1      // 有拨杆

#define CFG_CAN_LISTEN_ONLY             0      // EAP/召唤需要发送，不能用仅监听
#define CFG_ENABLE_WIFI_WEB             0      // 板子没有 C6/C5
#define CFG_ENABLE_BLE_TASK             0
#define CFG_ENABLE_USB_LOGGER           1      // 保留 CAN 日志

#define CFG_LAB_MENU                    1      // 召唤解锁的前置条件
#define CFG_AUTO_LANE_CHANGE            1      // EAP：自动变道
#define CFG_AUTO_BLINKER                1      // EAP：自动转向灯
#define CFG_SUMMON_HEARTBEAT            1      // 召唤解锁
#define CFG_SUMMON_HEARTBEAT_VALUE      -1     // 用固件默认 2
```

对应的编译结果：应用固件 **1,093,680 B（34%）**，静态 RAM **155,368 B（47%）**。
（关掉 Wi-Fi/网页省下的那 450 KB，就是 esp-hosted 那条链。）

实测效果（postv3）：把 `CFG_ENABLE_WIFI_WEB=0` 之后固件从 **1,547,908 B（49%）降到 1,093,286 B（34%）**，
因为 esp-hosted / WiFi / WebServer 整条链被链接器丢掉。

---

## 4. 完整开关表

### 4.1 车型（最关键）

| 开关 | 取值 | 说明 |
| --- | --- | --- |
| `CFG_BOOT_PROFILE` | `0` / `1..5` | `0`=不干预（用 NVS）；`1`=Model Y L，`2`=Y Juniper，`3`=Y Legacy，`4`=3 Highland，`5`=3 Legacy |
| `CFG_TURN_SIGNAL` | `-1` / `1` / `2` | `-1`=车型默认；`1`=STALK（有拨杆）；`2`=STALKLESS（无拨杆） |

⚠️ **选 Model 3 Highland（`CFG_BOOT_PROFILE 4`）时必须同时给 `CFG_TURN_SIGNAL`**，填 `1` 或 `2`。
上游固件里 Highland 的默认转向变体是 `TURN_SIGNAL_UNSET`（`vehicle_profile.h` 的 `vehicleProfileSpec`），
网页端选 Highland 时也强制要求指定，否则 `vehicleProfileSave()` 校验失败。
`user_config.h` 里已加编译期 `#error` 拦截这种组合，不会让你烧进一个悄悄不生效的固件。

### Model 3 Highland 与 Model 3 Legacy 的实际区别

两者 **CAN 拓扑完全相同**（都是 `TRIPLE_STANDARD`：BODY/CHASSIS/PARTY 三总线），
所以对你板子的接线、三路 CAN 引脚没有任何影响。差别全在"车上怎么操作"和"特殊位挂哪条报文"：

| 差异点 | Model 3 Highland | Model 3 Legacy |
| --- | --- | --- |
| 转向信号变体 | 没有默认值，**必须显式指定** STALK / STALKLESS | 默认 STALK（有拨杆） |
| 连续 AP 手势 | 滚轮单击 / 双击（`ScrollSingle` / `ScrollDouble`） | 拨杆双击（`StalkDouble`） |
| R79 / EU 解锁的 TSL9 路由 | 走 0x399 传输 | 额外支持网关翻译的 0x39B 路由（`DAS_status` on Body CAN A） |
| CAN 拓扑 / 高级 EAP / EU 解锁 / DMS-NAG / NAG 扭力 | 相同 | 相同 |

依据：`vehicle_profile.h` 的 `vehicleProfileSpec` / `vehicleProfileTurnValid` /
`vehicleProfileR79Hw3Supported`，以及 `continuous_ap_fsm_pure.h` 的 `continuousApGestureRoutePure`。

选错的症状：CAN 帧能正常收到，但转向灯联动、连续 AP 手势、NAG 的部分 TSL9 路径不按预期响应。

`CFG_BOOT_PROFILE` 是**唯一会写 Flash** 的开关——因为启动流程必须先读到它才能决定"是否进配车模式"。
写入用的是工程自带的 `vehicleProfileSave()`，语义与网页选车型完全一致；**已一致时不重复写**，不磨损 Flash。

### 4.2 三路 CAN 引脚

| 开关 | 默认 | 说明 |
| --- | --- | --- |
| `CFG_CAN_BODY_TX` / `_RX` | `-1`（20/21） | CAN 1 BODY |
| `CFG_CAN_CHASSIS_TX` / `_RX` | `-1`（22/23） | CAN 2 CHASSIS |
| `CFG_CAN_PARTY_TX` / `_RX` | `-1`（26/27） | CAN 3 PARTY |

取值 `0..54`，且不能占用 `6/14/15/16/17/18/19/24/25/54`（Wi-Fi SDIO、USB、C6 复位）。
这些默认值已按 `板级网表` 校准，见 `README.md 第 2 节` §2。

### 4.3 收发模式

| 开关 | 说明 |
| --- | --- |
| `CFG_CAN_LISTEN_ONLY` | `-1`=不干预；`0`=正常收发；`1`=仅监听（一个字节都不发） |

⚠️ 注意：只监听模式下三路控制器都不发送，而原固件部分功能要求"三路都就绪"，
所以某些注入类功能在监听模式下不会生效。这是原设计的准入条件，不是 bug。

### 4.4 后台任务

| 开关 | 说明 |
| --- | --- |
| `CFG_ENABLE_WIFI_WEB` | `1`=开启 Wi-Fi + 网页面板（原行为，但 P4 必须外挂 C6/C5 才真能连上）；`0`=完全跳过 esp-hosted，不建 web 任务 |
| `CFG_ENABLE_BLE_TASK` | `1`=开启 S3XY BLE 主机任务；`0`=不建任务，并把 `s3xyBluetoothEnabled` 压成关 |
| `CFG_ENABLE_USB_LOGGER` | `1`=USB CDC 跑二进制 CAN 日志协议（原行为）；`0`=不启动该任务，串口保持文本输出 |

### 4.5 驾驶类功能

| 开关 | 对应网页选项 | 说明 |
| --- | --- | --- |
| `CFG_NAG` | Nag killer | 方向盘扭力提示抑制 |
| `CFG_NAG_MODE` | Nag mode | 不确定就留 `-1` |
| `CFG_TLSSC` | Enable TLSSC | 交通灯/停车标志控制 |
| `CFG_TLSSC_HIGHWAY_GATE` | TLSSC highway gate | 只在高速工况启用 |
| `CFG_ISA_SUPPRESS` | ISA chime suppression | ISA 提示音抑制 |
| `CFG_FSD_VISUAL` | FSD visualisation | ★ 上游 README 提示会影响 AP、引起自动泊车报错 |
| `CFG_R79_UNLOCK` | R79 / EU unlock | `1`=解锁（注入 "1"）；`0`=原厂 STOCK 行为 |
| `CFG_CONTINUOUS_AP` | Continuous AP | 实验性 |
| `CFG_CONTINUOUS_AP_METHOD` | — | `1`=StalkDouble，`2`=ScrollSingle，`3`=ScrollDouble。开了连续 AP 就要给个手势，否则该功能不生效 |
| `CFG_SMARTSHIFT` | SmartShift | 自动换挡 |
| `CFG_PREHEAT` | Battery Preheating | 电池预热 Beta |
| `CFG_LAB_MENU` | LAB | 研究捕获 / 驾驶监控捕获总闸，会占较多 RAM |

### 4.6 EAP 与召唤

| 开关 | 对应网页选项 | 说明 |
| --- | --- | --- |
| `CFG_AUTO_LANE_CHANGE` | Auto Lane Change | NOA 里自动打转向灯完成变道 |
| `CFG_AUTO_LANE_CHANGE_BUS` | — | 发到哪条总线：`1`=CAN1 BODY，`2`=CAN2 CHASSIS，`3`=两条都发（默认） |
| `CFG_AUTO_BLINKER` | Auto Blinker | 自动转向灯 |
| `CFG_ULC_NO_CONFIRM` | No-confirm lane change | 免确认变道（上游注明 EU 区无效） |
| `CFG_SUMMON_HEARTBEAT` | Summon Unlock | 召唤解锁 |
| `CFG_SUMMON_HEARTBEAT_VALUE` | Summon heartbeat value | `0..3`，`-1`=用固件默认 `2` |

两个必须知道的上游约束（都是读源码确认的，不是猜的）：

1. **召唤解锁被硬绑定到 LAB。** `vehicleProfile.h` 无关，是 `vehicle_logic.h` 的
   `summonHeartbeatOverrideApply()` 内部直接校验 `labMenuEnabled`，不满足就 `return false`；
   网页端对应返回 `409 LAB disabled`。所以 `CFG_SUMMON_HEARTBEAT 1` 必须配 `CFG_LAB_MENU 1`，
   否则烧进去是静默不生效。`user_config.h` 里已加 `#error` 拦截这个组合。
   代价：LAB 打开会在启动时初始化研究捕获/驾驶监控捕获模块。
2. **Advanced EAP 要求拓扑为 `TRIPLE_STANDARD`**（Model 3/Y 标准三总线）。
   老款 Model 3、Model 3 Highland、Model Y Legacy/Juniper 都满足；Model Y L 走自己的分线实现。
   若拓扑不满足，`setAutoBlinkerEnabled()` 会直接返回 false。

另外：这几个开关都是**内存覆盖，不写 Flash**（`setAutoBlinkerEnabled(..., false)`、
直接赋值 `uiAutoLaneChangeEnabled` / `ulcNoConfirmEnabled`），避免每次开机磨损 NVS。

---

## 5. 实现方式（为什么不会破坏原逻辑）

新增两个文件，对原代码是**纯叠加**：

| 文件 | 作用 |
| --- | --- |
| `user_config.h` | 只有 `#define`，没有代码 |
| `user_config_apply.h` | 4 个 `static inline` 函数，把开关灌进原固件已有的全局量 |

在主 `.ino` 里只插入了 **6 处**受 `#if` 包裹的调用点，全部在原有流程的关键位置：

| 位置 | 函数 | 为什么必须在这一点 |
| --- | --- | --- |
| 原 `nvs_flash_init()` 成功之后、车型探测之前 | `p4UserCfgApplyProfile()` | 启动流程要先读到车型，晚了就进配车模式了 |
| 原 `p4CanGpioLoadAtBoot()` 之后 | `p4UserCfgApplyCan()` | 该函数会用 NVS/默认值覆盖引脚，必须排在它后面才不会被冲掉 |
| 原 `preheatMenuLoad()` 之后 | `p4UserCfgApplyFeatures()` | 所有 `xxxLoad()` 已完成；且要早于 `if (labMenuEnabled)` 的分支 |
| 3 处 Wi-Fi/web 启动点 | `#if P4UCFG_WEB_ENABLED` | 不建 web 任务 |
| `BOOT OK` 之后 | `#if P4UCFG_USBLOG_ENABLED` | 不启动 USB 日志任务 |

**没有修改任何原有函数体、没有删任何代码**——`.ino` 的改动是 54 行新增 + 1 行位置调整（`#include <Arduino.h>` 下移），
其余全部是 `#if` 包裹。设置 `P4CAN_USER_CONFIG 0` 即可完全回到上游行为。

---

## 6. 已验证 / 未验证

**已验证（本机实测编译）**

| 场景 | 结果 |
| --- | --- |
| 默认配置（全部 `-1`、Wi-Fi 开） | 编译通过，固件 **1,547,908 B / 全局变量 157,448 B**，与原版**字节数完全相同** → 默认路径确定是空操作 |
| 用户配置（车型=4、关闭 Wi-Fi/BLE、仅监听、NAG+、TLSSC+） | 编译通过，固件 **1,093,286 B（34%）**，无报错 |
| 关闭 Wi-Fi 但车型留 `0` | 编译期触发 `#warning` 提醒你设备会没有可用接口 |
| postv3 / prev3 双变体 | 均通过 |

**未验证（需要真机）**

- 车型写死之后 CAN 是否真的能起、三路总线是否正确对应你的车；
- 每个功能开关的实际效果（这些依赖真车/台架信号）；
- 仅监听模式下的行为；
- 关闭 Wi-Fi 后 USB 日志是否如预期输出文本。

**编译通过 ≠ 功能正确。** 第一次上台架请把 `CFG_CAN_LISTEN_ONLY` 设 `1`，只收不发。

---

## 7. 常见问题

**Q：改了 `user_config.h` 但行为没变？**
A：确认那一项不是 `-1`；确认真的重新编译烧录了（看 `build/logs/` 最新日志的时间戳）。

**Q：想回到网页控制？**
A：把对应项改回 `-1`。注意 `CFG_BOOT_PROFILE` 一旦设过并烧过，车型已经写进 Flash，
改回 `0` 后仍然沿用那个车型（网页可以再改）。

**Q：想完全回到上游原版？**
A：把 `P4CAN_USER_CONFIG` 改成 `0` 重新编译即可，或者直接从
`上游仓库对应目录` 复制 `.ino` 回来（再删掉两个 `user_config*.h`）。

**Q：加了开关会不会影响以后升级上游新版本？**
A：会有一点合并工作量，但改动集中在 `.ino` 的 6 个 `#if` 块和两个新文件；
把这两个文件一起带走，在新版本上按同样的 6 个位置插入即可。

**Q：为什么车型要写 Flash，别的开关不写？**
A：启动流程在 NVS 初始化之后、CAN 初始化之前就要判断"有没有车型"，这个判断读的是 NVS。
其他开关都在内存里覆盖，不写 Flash，避免磨损。
