#pragma once
// ============================================================================
//  P4-CAN Universal v3.35.2 —— 用户配置区（编译期开关）
// ============================================================================
//
//  用法：像 STM32 工程一样，改这里的 #define，然后重新编译烧录即可。
//        不需要手机 App，也不需要连 192.168.4.1 的网页面板。
//
//    ./scripts/build.sh postv3        (或 VS Code 里 Cmd+Shift+B)
//    ./scripts/flash.sh  postv3       (或 VS Code Run Task)
//
//  取值约定（三态开关）：
//     -1  = 不干预，沿用设备 Flash(NVS) 里保存的值 / 网页设置   ← 默认
//      0  = 强制关闭
//      1  = 强制开启
//
//  ★ 编译期开关的优先级高于网页设置：
//    每次开机都会在 NVS 加载完成后覆盖一次，所以网页里改了也会在下次开机被这项覆盖。
//    想交回网页控制，把对应项改回 -1。
//
//  ★ 车型（CFG_BOOT_PROFILE）是唯一会写 Flash 的开关：它必须写入 NVS，
//    因为启动流程要靠它判断"是否进入配车模式"。其余开关只在内存里生效，不写 Flash。
//    同一车型重复上电不会重复写（会先比对）。
//
// ============================================================================


// ---------------------------------------------------------------------------
//  0. 总开关
// ---------------------------------------------------------------------------
//  0 = 完全忽略本文件，固件行为与上游原始版本一模一样（想对比时用）
//  1 = 启用本文件的配置区
#define P4CAN_USER_CONFIG               1


// ---------------------------------------------------------------------------
//  1. 车型（最关键的一项）
// ---------------------------------------------------------------------------
//  这是"能不能进 CAN 工作状态"的开关。原固件必须在网页里选车型才会启动 CAN；
//  这里直接写死，开机就按选定车型跑，完全不依赖网页。
//
//     0 = 不干预（沿用 NVS / 网页选的车型）★注意：没网页时等于永远进不了 CAN
//     1 = Model Y L          （三总线，VEHICLE/BODY/PARTY）
//     2 = Model Y Juniper    （三总线标准拓扑）
//     3 = Model Y Legacy     （三总线标准拓扑）
//     4 = Model 3 Highland   （三总线标准拓扑）
//     5 = Model 3 Legacy     （三总线标准拓扑）
//
//  你板子的三路 CAN 接法是 1BODY / 2CHASSIS / 3PARTY + GPIO20/21、22/23、26/27，
//  对应 Model 3 / Model Y 的标准三总线拓扑。
#define CFG_BOOT_PROFILE                5

//  转向信号变体（只对支持该选项的车型有意义）
//     -1 = 用车型默认值（推荐）
//      1 = STALK      （有实体拨杆）
//      2 = STALKLESS  （无拨杆，方向盘按键）
#define CFG_TURN_SIGNAL                 1


// ---------------------------------------------------------------------------
//  2. 三路 CAN 引脚（按网表 2026-11-21 校准过的默认值）
// ---------------------------------------------------------------------------
//  -1 = 用固件内置默认值（BODY 20/21、CHASSIS 22/23、PARTY 26/27）
//  你自己的 PCB 就是这个接法，正常情况下全部留 -1。
//  只有改板/飞线之后才需要改这里。可填范围 0..54，
//  且不能占用 6/14/15/16/17/18/19/24/25/54（Wi-Fi SDIO、USB、C6 复位等）。
#define CFG_CAN_BODY_TX                 -1   // 默认 20
#define CFG_CAN_BODY_RX                 -1   // 默认 21
#define CFG_CAN_CHASSIS_TX              -1   // 默认 22
#define CFG_CAN_CHASSIS_RX              -1   // 默认 23
#define CFG_CAN_PARTY_TX                -1   // 默认 26
#define CFG_CAN_PARTY_RX                -1   // 默认 27


// ---------------------------------------------------------------------------
//  3. CAN 收发模式（安全相关，建议第一次上台架先设 1）
// ---------------------------------------------------------------------------
//     -1 = 不干预（沿用 Flash 里保存的模式；出厂默认是"正常收发"）
//      0 = 正常模式（会向总线发送帧）
//      1 = 只监听 LISTEN_ONLY（一个字节都不发，只收）
//
//  ★ 台架调试建议先设 1。注意：只监听模式下三路控制器都不会发送，
//    原固件的部分功能要求三路控制器都"就绪"，因此某些注入类功能不会生效。
#define CFG_CAN_LISTEN_ONLY             0


// ---------------------------------------------------------------------------
//  4. 后台任务开关
// ---------------------------------------------------------------------------
//  Wi-Fi + 网页面板（192.168.4.1）
//    1 = 开启（原行为）。但 ESP32-P4 没有射频，必须外挂 C6/C5 才真的能连上。
//    0 = 完全跳过 esp-hosted 初始化，不建 web 任务 —— 你的板子目前没有无线模块，
//        设 0 可以省掉一大段无效启动流程。
#define CFG_ENABLE_WIFI_WEB             0

//  S3XY 按钮蓝牙（BLE 主机）
//    1 = 开启（原行为，受 Wi-Fi 是否起来约束）
//    0 = 不建 BLE 任务，同时把 s3xyBluetoothEnabled 强制为关
#define CFG_ENABLE_BLE_TASK             0

//  USB CAN logger（把 CAN 帧以二进制协议打到 USB CDC）
//    1 = 开启（原行为）。开着时 USB 串口在 "BOOT OK" 之后就是二进制帧，看不了解码文本。
//    0 = 不启动该任务，USB 串口保持文本输出（调试更方便）
#define CFG_ENABLE_USB_LOGGER           1


// ---------------------------------------------------------------------------
//  5. 驾驶类功能开关
// ---------------------------------------------------------------------------
//  Nag killer（方向盘扭力提示抑制）
#define CFG_NAG                         -1
//  Nag 模式（原网页里的 mode 值，不确定就留 -1 用 NVS 值）
#define CFG_NAG_MODE                    -1

//  TLSSC（交通灯/停车标志控制），部分区域默认不可用
#define CFG_TLSSC                       1
//  TLSSC 高速门控（只在高速工况启用）
#define CFG_TLSSC_HIGHWAY_GATE          -1

//  ISA 提示音抑制
#define CFG_ISA_SUPPRESS                -1

//  FSD 可视化（★ 上游 README 提示会影响 AP 并引起自动泊车报错）
#define CFG_FSD_VISUAL                  -1

//  R79 / EU 限制解锁
//     -1 = 不干预
//      1 = 解锁（用注入的 "1"，即 EU 解锁）
//      0 = 用原厂行为（STOCK）
#define CFG_R79_UNLOCK                  -1

//  连续 AP（experimental）
#define CFG_CONTINUOUS_AP               -1
//  触发手势，只有 CFG_CONTINUOUS_AP 打开时才需要
//      1 = StalkDouble（拨杆双击）
//      2 = ScrollSingle（滚轮单击）
//      3 = ScrollDouble（滚轮双击）
#define CFG_CONTINUOUS_AP_METHOD        -1

//  SmartShift 自动换挡
#define CFG_SMARTSHIFT                  -1

//  电池预热（Beta）
#define CFG_PREHEAT                     -1

//  LAB 菜单（研究/捕获、驾驶监控捕获等实验功能的总闸）
//    注意：打开会分配较大的 RAM 缓冲，并在启动时初始化捕获模块。
//    ★ 召唤解锁（CFG_SUMMON_HEARTBEAT）在上游固件里被强制要求 LAB 打开，
//      两个必须一起开，否则召唤解锁会被拒绝（summonHeartbeatOverrideApply 内部校验 labMenuEnabled）。
#define CFG_LAB_MENU                    1


// ---------------------------------------------------------------------------
//  5.5 EAP 与召唤
// ---------------------------------------------------------------------------
//  这一组对应网页里的"Advanced EAP / 自动变道 / 自动转向灯"和"Summon Unlock"。
//  固件里 Advanced EAP 要求拓扑为 BODY+CHASSIS+PARTY（Model 3/Y 标准三总线），
//  你的车满足；Model Y L(1) 走自己的分线实现。
//
//  自动变道（NOA 里自动打转向灯完成变道）
#define CFG_AUTO_LANE_CHANGE            1
//  自动变道发到哪条总线：1=CAN1 BODY，2=CAN2 CHASSIS，3=两条都发（默认）
//  -1 = 不干预，保留原值（BOTH）
#define CFG_AUTO_LANE_CHANGE_BUS        -1
//  自动转向灯（Auto Blinker）
#define CFG_AUTO_BLINKER                1
//  免确认变道（不需要再确认"免费变道"，EU 区外的等效替代，注意上游说 EU 无效）
#define CFG_ULC_NO_CONFIRM              -1

//  ★ China Restore / DAS_autopilotConfig 覆盖（很可能是泊车/召唤的总闸）
//    收到 CAN2 的 0x331(DLC=6) 时，把 data[0] 低 6 位改成 0x1B(SELF_DRIVING)，
//    让车认为自身具备 FSD 档能力。自动泊车、召唤、TLSSC 都属于这一档。
//    上游默认关闭，网页里是 Service 区的 "China Restore" 开关。
//     -1 = 不干预（默认关）   0 = 强制关   1 = 强制开
//    已知副作用：MCU 只在自己启动那一刻锁存该配置，若那时总线上没有改过的
//    0x331，就需要手动重启中控屏。上游有未合并分支 tlssc-fix 专门修这个。
#define CFG_CHINA_RESTORE               1

//  召唤解锁（Summon Unlock）
//     -1 = 不干预
//      0 = 强制关闭
//      1 = 强制开启  ★ 必须同时把 CFG_LAB_MENU 设为 1
#define CFG_SUMMON_HEARTBEAT            1
//  召唤心跳覆盖值，固件允许 0..3。原作者代码里的初始值是 2。
//    -1 = 用固件默认值 2（推荐先用这个）
//  说明：上游没有文档说明哪个值对应 ±85 m，网页端也是让用户自己选，
//        所以这里不猜；台架上可以逐个试。
#define CFG_SUMMON_HEARTBEAT_VALUE      -1


// ---------------------------------------------------------------------------
//  6. 编译期自检（防止配出"上电变砖"的组合）
// ---------------------------------------------------------------------------
#if P4CAN_USER_CONFIG
#  if (CFG_ENABLE_WIFI_WEB == 0) && (CFG_BOOT_PROFILE == 0)
#    warning "关闭了 Wi-Fi/网页，但 CFG_BOOT_PROFILE 仍为 0：若 Flash 里没有车型，设备将停在配车模式且无任何接口可用。建议把 CFG_BOOT_PROFILE 设为 1..5。"
#  endif
#  if (CFG_ENABLE_WIFI_WEB == 0) && (CFG_ENABLE_USB_LOGGER == 0)
#    warning "网页和 USB 日志都关闭了：设备运行后没有任何输出通道，只能靠行为判断状态。"
#  endif
#  if (CFG_BOOT_PROFILE < -1) || (CFG_BOOT_PROFILE > 5)
#    error "CFG_BOOT_PROFILE 只能是 0(不干预) 或 1..5(车型编号)"
#  endif
#  if (CFG_TURN_SIGNAL != -1) && (CFG_TURN_SIGNAL != 1) && (CFG_TURN_SIGNAL != 2)
#    error "CFG_TURN_SIGNAL 只能是 -1(车型默认) / 1(STALK 有拨杆) / 2(STALKLESS 无拨杆)"
#  endif
//  Model 3 Highland 在上游固件里没有默认转向变体（vehicleProfileSpec 里是 UNSET），
//  网页端也必须显式指定，否则 vehicleProfileSave() 会校验失败。这里提前拦住。
#  if (CFG_BOOT_PROFILE == 4) && (CFG_TURN_SIGNAL <= 0)
#    error "选了 Model 3 Highland 就必须同时设 CFG_TURN_SIGNAL：1=STALK(有拨杆) 或 2=STALKLESS(无拨杆)，固件没有默认值"
#  endif
//  召唤解锁在上游被硬性绑定 LAB：summonHeartbeatOverrideApply() 内部会检查 labMenuEnabled，
//  否则直接返回 false（网页端也是返回 409 "LAB disabled"）。这里提前拦住，避免烧进去悄悄不生效。
#  if (CFG_SUMMON_HEARTBEAT > 0) && (CFG_LAB_MENU != 1)
#    error "召唤解锁要求 LAB 打开：请同时设 CFG_LAB_MENU 1（上游固件强制校验 labMenuEnabled）"
#  endif
#  if (CFG_SUMMON_HEARTBEAT_VALUE > 3)
#    error "CFG_SUMMON_HEARTBEAT_VALUE 只能是 -1(默认2) 或 0..3"
#  endif
#  if (CFG_AUTO_LANE_CHANGE_BUS != -1) && (CFG_AUTO_LANE_CHANGE_BUS < 1 || CFG_AUTO_LANE_CHANGE_BUS > 3)
#    error "CFG_AUTO_LANE_CHANGE_BUS 只能是 -1(不干预) / 1(CAN1) / 2(CAN2) / 3(两条都发)"
#  endif
#endif
