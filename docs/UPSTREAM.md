# 上游来源与改动声明

## 基线

| 项 | 值 |
| --- | --- |
| 上游仓库 | https://github.com/06066060606060/P4can-universal-unlock |
| 基线版本 | **v3.35.3** |
| 基线 commit | `81a42103e8dd2ee600f27ff3ecd1ac542802a029`（2026-11-21，main） |
| 许可证 | GNU GPL v3 or later（见 `LICENSE`） |

本仓库是上述项目的**衍生作品**，沿用同一许可证。

## 改动总览

相对基线，**没有删除或改写任何上游原有逻辑**。所有改动为新增文件或在 `#if` 内追加调用。

### 新增文件

| 文件 | 说明 | 来源 |
| --- | --- | --- |
| `firmware/.../user_config.h` | 编译期配置区，32 个开关，纯 `#define` | 本项目 |
| `firmware/.../user_config_apply.h` | 配置生效逻辑，5 个 `static inline` 函数 | 本项目 |
| `firmware/.../sketch.yaml` | arduino-cli 工程文件，固定 `default_fqbn` | 本项目 |
| `firmware/.../eap_anti_restart.h` | 上电防重启修复 | **上游 PR #15，原样移植，逐字节一致** |
| `scripts/` | macOS 编译/烧录/采集/IntelliSense 脚本 | 本项目 |

### 修改文件

| 文件 | 追加行数 | 内容 |
| --- | --- | --- |
| `P4-CAN-Universal-v3.35.3.ino` | +58 | 10 处配置钩子，全部在 `#if` 内 |
| `can_usb_logger_task.h` | +22 | 新增 `LOGGER DIAG` 命令（纯追加，不改原协议） |
| `can_runtime.h` | +3 | 防重启修复的 2 处观察 + 1 处回放调用 |
| `service_settings.h` | +2 | 引入 `eap_anti_restart.h` |

### 复核方法

```bash
git clone https://github.com/06066060606060/P4can-universal-unlock.git upstream
cd upstream && git checkout 81a4210
diff -r . ../P4-CAN-nowifi/firmware/P4-CAN-Universal-v3.35.3
```

预期只看到上表列出的新增文件与少量追加行。

> 注意：`can_runtime.h` 与 `can_usb_logger_task.h` 上游使用 **CRLF** 换行，
> 本仓库已保持一致，避免整文件被误判为改动。

## 第三方署名

`eap_anti_restart.h` 来自上游分支 `tlssc-fix`，
提交 `5b84178` — *fix(TLSSC Restore): cache and replay patched 0x331 to avoid MCU reboot on every power cycle*，
PR #15（`baicyber/fix/eap-anti-restart`）。**版权归原作者**，本项目未作任何修改。

上游仓库根目录未提供 LICENSE 文件，仅子模块内有
`automatic_shift/LICENSE` 与 `third_party/tinycrypt/LICENSE-ESP-IDF`。
本仓库按上游 README 声明的 GPL v3+ 补齐了完整许可证文本。

## 未采用的上游改动

分支 `tlssc-fix` 中除 `eap_anti_restart.h` 外的部分（`web_api.h` 的统计接口）
未采用——本方案无网页，相应诊断改由 `LOGGER DIAG` 提供。

## 未随发布提供的内容

- 板级网表、BOM、Gerber 等硬件设计文件（属开发者私有资料）
- 编译产物 `build/`（可自行编译复现）
- `.vscode/c_cpp_properties.json`（含本机绝对路径，由 `scripts/gen_intellisense.py` 生成）

## 关于预编译二进制与 SHA 复现

**建议只发布源码，不发布预编译 `.bin`。** 原因：ESP-IDF 会把 `__FILE__` 的
**绝对路径**打进固件（断言字符串等），实测二进制里含有
`/Users/<用户名>/...` 这样的编译者本机路径，发布出去会泄露目录结构。

同时这也影响 SHA 复现的边界：

- `build.sh` 导出 `SOURCE_DATE_EPOCH` 固定了内嵌的 `__DATE__`/`__TIME__`，
  所以**同一目录、同一源码**重复编译，SHA256 完全一致（已实测）。
- 但**换个目录编译**，内嵌路径变长/变短，SHA 会不同（实测同一份源码在两处编译相差 40 字节）。

所以复现校验请在**相同路径**下进行，或只比对源码 diff 而不比对二进制哈希。
