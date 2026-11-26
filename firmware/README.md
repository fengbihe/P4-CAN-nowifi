# firmware/ — 参与编译的源码树

本目录只有一个子目录 `P4-CAN-Universal-v3.35.3/`，即 **Arduino sketch**。
它下面**所有 `.ino` / `.cpp` / `.c` 都会被 arduino-cli 编译**，`.h` 只被包含。
除 `src/` 外，其它子目录里的 `.cpp` **不会**被编译（已实测确认）。

> ⚠️ 目录名必须等于主程序名：Arduino 要求 `<目录名>/<目录名>.ino`。
> 改名会导致 `Can't open sketch: main file missing from sketch`。

---

## 上游 sketch 内部结构

| 路径 | 是否参与固件编译 | 说明 |
| --- | --- | --- |
| `P4-CAN-Universal-v3.35.3.ino` | ✅ 编译 | 主程序：启动流程、NVS 初始化、三路 CAN 安装、任务创建 |
| `*.h`（根目录 ~90 个） | ✅ 被包含 | 全部业务与驱动头文件，见下方分组 |
| `p4_twai_driver.c` | ✅ 编译 | **自研 TWAI 驱动**（替换 IDF 原版，支持 P4 的三路控制器 + R79 优先级/MUX1 时序） |
| `p4_twai_hal_v1.c` | ✅ 编译 | TWAI HAL 的 v1 兼容实现 |
| `p4_twai_diag.c` | ✅ 编译 | ISR 级诊断采样 |
| `src/p4_tinycrypt/*.c` | ✅ 编译 | tinycrypt ECC 的 ESP 移植层（`src/` 下递归编译，共 4 个 `.o`） |
| `automatic_shift/` | ✅ 仅头文件 | SmartShift 自动换挡模块（`tests/` 里的 `.cpp` 是宿主机测试，**不编译**，已确认 0 个 `.o`） |
| `third_party/tinycrypt/` | ❌ 不编译 | 上游 tinycrypt v555 副本，仅宿主机测试用。固件实际用 ESP-IDF 自带的同名头文件，见 `../docs/BUILD_REPORT.md` §4.2 |
| `vendor/twai/*.c.txt` | ❌ 不编译 | 上游 TWAI 驱动基线快照（`.txt` 后缀），供比对 |
| `tools/` | ❌ 不编译 | 看板打包与校验脚本（`build_dashboard.py` / `embed_dashboard.py` / `verify_*.py` / `check_dashboard_ui.js`） |
| `assets/` | ❌ 不编译 | `apple-touch-icon.png`，网页面板图标 |
| `fonts/` | ❌ 不编译 | Geist 字体 `.woff2`；实际字体已 gzip 内嵌在 `lab_fonts.h` |
| `zh-CN/` | ❌ 不编译 | 中文看板 HTML；要打中文版需手动替换，步骤见 `../docs/BUILD_REPORT.md` §4.3 |

---

## 根目录头文件分组（便于定位）

| 分组 | 代表文件 |
| --- | --- |
| 主程序/配置 | `pin_config.h`、`build_opt.h`、`service_settings.h`、`fsd_settings.h` |
| CAN 驱动与状态 | `p4_can.h`、`can_core.h`、`can_runtime.h`、`can_gpio_pure.h`、`can_gpio_runtime.h`、`p4_twai_priority.h`、`p4_twai_diag.h` |
| 诊断/持久化 | `can_error_journal*.h`、`can_busoff_persistence*.h`、`nvs_checked.h` |
| 车辆逻辑 | `vehicle_logic.h`、`vehicle_profile.h`、`r79_*.h`、`mode_h_*.h`、`ulc_compositor_pure.h` |
| 功能模块 | `auto_blinker_pure.h`、`summon_state_pure.h`、`preheat_*.h`、`continuous_ap_*.h`、`tsl9_*.h` |
| 面板/前端 | `index_html.h`（gzip 内嵌看板）、`dashboard_icon.h`、`lab_fonts.h`、`web_api.h`、`r79_timing_web.h` |
| USB / 网络 / BLE | `can_usb_logger*.h`、`p4_network.h`、`s3xy_ble.h`、`p4_ble_crypto.h`、`p4_ble_security.h` |
| 纯函数（可测） | 所有 `*_pure.h`（无 Arduino 依赖，宿主机测试直接包含） |

---

## 改动记录

相对上游只有三处变化，其中前两处是必须的，第三处是新增功能：

| 变化 | 说明 | 是否影响原逻辑 |
| --- | --- | --- |
| 目录名 `P4can-universal-unlock` → `P4-CAN-Universal-v3.35.3` | Arduino 要求目录名 == 主 `.ino` 名，否则报 `main file missing from sketch` | 不影响 |
| 新增 `user_config.h` | **编译期用户配置区**，24 个开关，纯 `#define` | 不影响（不编译进任何逻辑，除非被引用） |
| 新增 `user_config_apply.h` + `.ino` 净增 53 行 | 把开关灌进原有全局量；6 处调用点全在 `#if` 内 | 设 `P4CAN_USER_CONFIG 0` 即完全回到上游行为 |

`.ino` 文件名与其余全部文件内容保持原样（`diff -r` 可验证）。上游原始状态见 `上游仓库对应目录`。

**用户配置区说明见 [`../docs/USER_CONFIG.md`](../docs/USER_CONFIG.md)。**
