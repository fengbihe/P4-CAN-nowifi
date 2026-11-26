# .vscode/ — VS Code 工作区配置

打开文件夹时选 `P4-CAN/`（本目录的父目录），这些配置才会生效。

| 文件 | 作用 |
| --- | --- |
| `tasks.json` | Build / Flash / Monitor / 端口探测等任务，`Cmd+Shift+B` 直接编译 |
| `settings.json` | 关闭 cpptools 自动打开 上游仓库 的索引、指定终端与文件关联 |
| `c_cpp_properties.json` | IntelliSense 配置（**本机生成，不入库**），运行 `scripts/gen_intellisense.py` 产生 |

## c_cpp_properties.json 为什么是"生成"的

Arduino-ESP32 3.x 的 esp32p4 目标用 GCC 的 `-iprefix <libs>/include/` +
`-iwithprefixbefore <相对路径>` 来挂 ESP-IDF 组件的头文件路径
（见 `esp32p4-libs/3.3.12/flags/includes`）。**VS Code 的 C/C++ 扩展不认这种写法**，
而主 `.ino` 在 `compile_commands.json` 里没有条目（那里只有 arduino-cli 生成的 `.ino.cpp`），
于是只能靠 `includePath` 兜底 —— 如果它不全，就会满屏报：

```
无法打开 源 文件 "freertos/FreeRTOS.h" (dependency of "Arduino.h")
无法打开 源 文件 "soc/soc_caps.h"      (dependency of "WiFi.h")
无法打开 源 文件 "driver/twai.h"
...
未定义标识符 "BaseType_t" / "esp_err_t" / "uint32_t" 不是类型名 / 应输入声明
```

后面那一大串「缺少显式类型」「应输入声明」都是**连带的**，不是真错误。

所以这些路径由 `scripts/gen_intellisense.py` 展开成**绝对路径**（383 个目录）+ 41 个宏
（其中 `ESP_PLATFORM` 必须有，否则 NimBLE 的 `syscfg.h` 不会去包含 `esp_nimble_cfg.h`，
会报 `MYNEWT_VAL_xxx 未定义`）写进本文件。

重新生成：

```bash
./scripts/build.sh postv3 --ide-db      # 编译并刷新 IntelliSense 配置
# 或单独执行
python3 scripts/gen_intellisense.py
```

生成后如果 VS Code 还显示旧的红波浪线，执行
`Cmd+Shift+P → C/C++: Reset IntelliSense Database`，或者直接 `Developer: Reload Window`。

脚本最后会做自检，直接告诉你那 14 个关键头文件是否都能解析；全部通过时打印：
`✅ 关键头文件全部可解析（14/14）`。


## 任务一览（`Terminal → Run Task`）

| 任务 | 说明 |
| --- | --- |
| `P4-CAN: Build (postv3)` | ⭐ **默认编译任务**，`Cmd+Shift+B` 触发 |
| `P4-CAN: Build (prev3)` | 芯片 v3.00 之前的版本 |
| `P4-CAN: Build all variants` | 两个变体都编 |
| `P4-CAN: Clean rebuild (postv3)` | 删缓存重编 |
| `P4-CAN: Generate IntelliSense database` | 生成 `build/compile_commands.json` |
| `P4-CAN: List serial ports` | 列出 `/dev/cu.*`，不烧录 |
| `P4-CAN: Flash (postv3)` | 分段烧录（执行时会要求输入 `y` 确认） |
| `P4-CAN: Flash (prev3)` | prev3 分段烧录 |
| `P4-CAN: Flash merged 32MB image` | 整片镜像烧到 `0x0` |
| `P4-CAN: Flash without confirmation` | 跳过确认（仅在你已确认处于下载模式时用） |
| `P4-CAN: Monitor serial` | 115200 串口监视，`Ctrl+]` 退出 |

## 首次使用建议

1. 打开本文件夹后先跑一次 **Generate IntelliSense database**（约 1 分钟），
   否则代码跳转/补全不全，因为 Arduino 的 include 路径是编译时动态生成的。
2. 编译问题会在 `问题` 面板里显示（`tasks.json` 里配了 `$gcc` 问题匹配器）。
3. 任务失败时看 `build/logs/` 下最新的日志文件，里面有完整编译输出。

## 把它当成一个"工程"打开（推荐）

你的 VS Code 里已经装了 **`vscode-arduino-cli-wrapper`**（tanakamasayuki，1.10.0）。
它就是 arduino-cli 的工程化封装，装上之后这个项目就有"正经工程"的观感了：

| 你会得到 | 说明 |
| --- | --- |
| 状态栏按钮 | `Compile` / `Upload` / `Monitor`，以及 FQBN / 串口 / 波特率 / 告警等级，点一下就能改 |
| 资源管理器里的 **Arduino CLI** 视图 | 列出 sketch、板卡参数、常用动作（编译、上传、调试、串口、Board Details…） |
| 一键 Build Check | 按 `sketch.yaml` 里定义的配置逐个编译并汇总错误 |
| 编译日志 | 彩色输出到伪终端，错误可点击跳到行 |
| IntelliSense 联动 | 编译时同步 includePath |

**第一次用**：命令面板执行 `Arduino CLI: Check CLI Version`，确认它找到
`/opt/homebrew/bin/arduino-cli`；然后状态栏点板卡那栏选 FQBN（或直接依赖下面的 `sketch.yaml`）。

### 工程文件 `sketch.yaml`（已建好）

`firmware/P4-CAN-Universal-v3.35.3/sketch.yaml` 里写了 `default_fqbn`，所以：

- 命令行 `arduino-cli compile firmware/P4-CAN-Universal-v3.35.3` **不用再给 `--fqbn`**；
- 扩展读它来记住板级参数。

**为什么不写 `profiles:`**（这一点我实测过，写在文档里免得你踩）：

一旦在 `sketch.yaml` 里定义 `profiles`，arduino-cli 会改用
`~/Library/Arduino15/internal/` 下的**私有平台副本**来构建。后果有两个：

1. 额外占用 **5.9 GB** 磁盘（我测试时真的生成了，已删掉）；
2. 同样配置编出来的固件比 `build.sh` 多 **40 字节**（因为内部路径不同，`__FILE__` 之类被打进产物），
   破坏了"可重复编译、SHA256 一致"这个性质。

所以只用 `default_fqbn`：实测 `arduino-cli compile`（不给 `--fqbn`）和 `./scripts/build.sh postv3`
产出的 SHA256 完全一致（`0e1cd269…`）。

### 和现有 scripts / tasks 的关系

三者都调同一个 arduino-cli，产物写不同目录，**互不冲突**（实测并存后 SHA256 不变）：

- `scripts/build.sh` → `build/firmware/<变体>/`（带命名整理与 SHA256 打印）
- `.vscode/tasks.json` → 就是调上面的脚本
- Arduino CLI 扩展 → 用自己的构建目录，适合随手点一下

⚠️ **一个冲突点**：扩展在编译时会更新 IntelliSense includePath，可能重写
`.vscode/c_cpp_properties.json`。如果哪天 `.ino` 又开始报"无法打开源文件 freertos/FreeRTOS.h"，
重新跑一次 `python3 scripts/gen_intellisense.py` 就好（见下方说明）。

## 为什么没用 PlatformIO

你机器上也装了 PlatformIO IDE，它确实是"最像工程"的方案（`.pio` 目录、库管理、一键调试）。
但这个项目不适合：

- PlatformIO 官方板卡库里**搜不到 esp32-p4**（registry 查询 0 命中），要自己写 board 定义；
- 需要把 Arduino sketch 改造成 `src/main.cpp` 布局，会破坏"保留原项目结构与功能"的前提；
- 这个固件依赖 Arduino-ESP32 **3.3.12 的特定板级选项**（`app3M_fat9M_16MB`、`hwcdc`、`qio`）
  和三路 TWAI 自研驱动，迁到 PlatformIO 要重新对齐一大堆 sdkconfig，风险远大于收益。

结论：**继续用 arduino-cli + 已装的 wrapper 扩展**，既有工程观感，又不动物理结构。
