#pragma once
// ============================================================================
//  user_config.h 的生效逻辑（不要在这里配置，去 user_config.h 改开关）
// ============================================================================
//
//  ★ 本头文件必须在主 .ino 的**其它所有头文件之后**被包含，
//    因为它直接用到了 p4CanTxPins / nagCfg / tlsscEnabled / gearShiftConfig 等
//    在别处定义的全局量。
// ============================================================================
#include "user_config.h"

#if P4CAN_USER_CONFIG

#include "vehicle_profile.h"
#include "can_core.h"
#include "can_usb_logger.h"
#include "p4can_core_state.h"
#include "fsd_settings.h"
#include "vehicle_logic.h"
#include "continuous_ap_fsm_pure.h"
#include "continuous_ap_runtime.h"
#include "automatic_shift/gear_shift_runtime.h"
#include "preheat_runtime.h"
#include "s3xy_ble.h"
#include "service_settings.h"
#include "p4_can.h"

// ---------------------------------------------------------------------------
//  1) 车型：必须在 NVS 车型探测之前调用
// ---------------------------------------------------------------------------
//  与 NVS 中已保存的车型一致时不写 Flash（避免每次开机磨损），
//  不一致时用工程自带的 vehicleProfileSave() 写入，语义与网页选车型完全相同。
static inline bool p4UserCfgApplyProfile() {
#if (CFG_BOOT_PROFILE >= 1) && (CFG_BOOT_PROFILE <= 5)
  const uint8_t wantId = (uint8_t)CFG_BOOT_PROFILE;
  if (!vehicleProfileValid(wantId)) return false;
  const uint8_t wantTopology = (uint8_t)vehicleProfileDefaultTopology(wantId);
  const uint8_t wantTurn =
      (CFG_TURN_SIGNAL >= 0) ? (uint8_t)CFG_TURN_SIGNAL : (uint8_t)TURN_SIGNAL_UNSET;
  const uint8_t resolvedTurn =
      (uint8_t)vehicleProfileResolvedTurn(wantId, wantTopology, wantTurn);

  uint8_t haveId = (uint8_t)VEHICLE_PROFILE_NONE;
  uint8_t haveTurn = (uint8_t)TURN_SIGNAL_UNSET;
  bool readOk = false;
  CheckedPreferences probe;
  if (probe.begin(VEHICLE_PROFILE_NAMESPACE, true)) {
    haveId = probe.getUChar(VEHICLE_PROFILE_KEY, (uint8_t)VEHICLE_PROFILE_NONE);
    haveTurn = probe.getUChar(VEHICLE_TURN_KEY, (uint8_t)TURN_SIGNAL_UNSET);
    readOk = probe.ok();
  }
  probe.end();
  if (readOk && haveId == wantId && haveTurn == resolvedTurn) return true;

  return vehicleProfileSave(wantId, wantTopology, wantTurn);
#else
  return true;
#endif
}

// ---------------------------------------------------------------------------
//  2) CAN 引脚 + 收发模式：必须在 p4CanGpioLoadAtBoot() 之后、p4CanInstall() 之前
// ---------------------------------------------------------------------------
//  p4CanGpioLoadAtBoot() 会用 NVS/内置默认值覆盖引脚，所以这里必须排在它后面。
static inline void p4UserCfgApplyCan() {
#if CFG_CAN_BODY_TX >= 0
  p4CanTxPins[P4_BODY] = CFG_CAN_BODY_TX;
#endif
#if CFG_CAN_BODY_RX >= 0
  p4CanRxPins[P4_BODY] = CFG_CAN_BODY_RX;
#endif
#if CFG_CAN_CHASSIS_TX >= 0
  p4CanTxPins[P4_CHASSIS] = CFG_CAN_CHASSIS_TX;
#endif
#if CFG_CAN_CHASSIS_RX >= 0
  p4CanRxPins[P4_CHASSIS] = CFG_CAN_CHASSIS_RX;
#endif
#if CFG_CAN_PARTY_TX >= 0
  p4CanTxPins[P4_PARTY] = CFG_CAN_PARTY_TX;
#endif
#if CFG_CAN_PARTY_RX >= 0
  p4CanRxPins[P4_PARTY] = CFG_CAN_PARTY_RX;
#endif
#if CFG_CAN_LISTEN_ONLY >= 0
  // 与网页"监听模式"同一个变量，CAN 安装时由 p4CanPassive() 读取
  __atomic_store_n(&canUsbPassiveMode, CFG_CAN_LISTEN_ONLY > 0, __ATOMIC_RELEASE);
#endif
}

// ---------------------------------------------------------------------------
//  3) 各功能开关：必须在所有 xxxLoad() 之后、labMenuEnabled 分支之前
// ---------------------------------------------------------------------------
static inline void p4UserCfgApplyFeatures() {
#if CFG_NAG >= 0
  portENTER_CRITICAL(&nagCfgMux);
  nagCfg.enabled = (CFG_NAG > 0);
  portEXIT_CRITICAL(&nagCfgMux);
#endif
#if CFG_NAG_MODE >= 0
  portENTER_CRITICAL(&nagCfgMux);
  nagCfg.mode = (uint8_t)CFG_NAG_MODE;
  portEXIT_CRITICAL(&nagCfgMux);
#endif
#if CFG_TLSSC >= 0
  tlsscEnabled = (CFG_TLSSC > 0);
#endif
#if CFG_TLSSC_HIGHWAY_GATE >= 0
  tlsscHighwayGateEnabled = (CFG_TLSSC_HIGHWAY_GATE > 0);
#endif
#if CFG_ISA_SUPPRESS >= 0
  isaSuppressionEnabled = (CFG_ISA_SUPPRESS > 0);
#endif
#if CFG_FSD_VISUAL >= 0
  fsdSettingsEnabled = (CFG_FSD_VISUAL > 0);
#endif
#if CFG_PREHEAT >= 0
  preheatMenuEnabled = (CFG_PREHEAT > 0);
#endif
#if CFG_LAB_MENU >= 0
  labMenuEnabled = (CFG_LAB_MENU > 0);
#endif
#if CFG_R79_UNLOCK >= 0
  // r79Hw3Enabled == true 表示"用原厂 R79 行为"；解锁要把注入的 "1" 送出去
  portENTER_CRITICAL(&r79LabMux);
  r79Hw3Enabled = (CFG_R79_UNLOCK <= 0);
  portEXIT_CRITICAL(&r79LabMux);
#endif
#if CFG_SMARTSHIFT >= 0
  gearShiftConfig.enabled = (CFG_SMARTSHIFT > 0);
#endif
#if CFG_CONTINUOUS_AP >= 0
  {
    ContApConfig want = continuousApControlSnapshot();
    want.enabled = (CFG_CONTINUOUS_AP > 0);
#if CFG_CONTINUOUS_AP_METHOD > 0
    want.method = (ContApMethod)CFG_CONTINUOUS_AP_METHOD;
#endif
    const ContApConfig cur = continuousApControlSnapshot();
    // 只在需要变更时才调用（该函数会写 NVS，避免每次开机磨损）
    if (cur.enabled != want.enabled || cur.method != want.method) {
      (void)continuousApControlApply(want);
    }
  }
#endif

  // ---- EAP：自动变道 ----
  // uiAutoLaneChangeEnabled 是运行时闸门，由 autoLaneChangeLabCfgLoadAndMigrate() 从 NVS 载入，
  // 这里直接覆盖，语义与网页 /api/lab/auto-lane-change/update 相同，但不写 Flash。
#if CFG_AUTO_LANE_CHANGE >= 0
  portENTER_CRITICAL(&autoLc293Mux);
  uiAutoLaneChangeEnabled = (CFG_AUTO_LANE_CHANGE > 0);
  portEXIT_CRITICAL(&autoLc293Mux);
#endif
#if (CFG_AUTO_LANE_CHANGE_BUS >= 1) && (CFG_AUTO_LANE_CHANGE_BUS <= 3)
  portENTER_CRITICAL(&autoLc293Mux);
  uiAutoLaneChangeTargetBus = (uint8_t)CFG_AUTO_LANE_CHANGE_BUS;
  portEXIT_CRITICAL(&autoLc293Mux);
#endif

  // ---- EAP：自动转向灯 ----
  // persist=false：只在内存生效，不写 NVS，避免每次开机磨损（开关本身每次启动都会重新灌入）
#if CFG_AUTO_BLINKER >= 0
  (void)setAutoBlinkerEnabled(CFG_AUTO_BLINKER > 0, false);
#endif

  // ---- 免确认变道 ----
#if CFG_ULC_NO_CONFIRM >= 0
  ulcNoConfirmEnabled = (CFG_ULC_NO_CONFIRM > 0);
#endif

  // ---- China Restore：DAS_autopilotConfig → SELF_DRIVING ----
  // serviceSettingsLoad() 已在前面执行，这里覆盖运行时标志（不写 NVS）。
#if CFG_CHINA_RESTORE >= 0
  portENTER_CRITICAL(&serviceSettingsMux);
  chinaRestoreEnabled = (CFG_CHINA_RESTORE > 0);
  portEXIT_CRITICAL(&serviceSettingsMux);
#endif

  // ---- 召唤解锁 ----
#if CFG_SUMMON_HEARTBEAT >= 0
  {
    const uint8_t summonValue = (CFG_SUMMON_HEARTBEAT_VALUE >= 0)
        ? (uint8_t)CFG_SUMMON_HEARTBEAT_VALUE : 2u;
    // 该函数内部会校验 labMenuEnabled / 拓扑，并占用 canTxBarrierMutex
    if (!summonHeartbeatOverrideApply(CFG_SUMMON_HEARTBEAT > 0, summonValue)) {
      Serial.println("[P4-CAN] USER_CONFIG: summon heartbeat override rejected "
                     "(check CFG_LAB_MENU / topology / TX barrier)");
    } else {
      Serial.printf("[P4-CAN] USER_CONFIG: summon heartbeat override enabled=%d value=%u\n",
                    (int)(CFG_SUMMON_HEARTBEAT > 0), (unsigned)summonValue);
    }
  }
#endif
}

// ---------------------------------------------------------------------------
//  4) 任务开关：给主 .ino 用的编译期布尔
// ---------------------------------------------------------------------------
#define P4UCFG_WEB_ENABLED     (CFG_ENABLE_WIFI_WEB)
#define P4UCFG_BLE_ENABLED     (CFG_ENABLE_BLE_TASK)
#define P4UCFG_USBLOG_ENABLED  (CFG_ENABLE_USB_LOGGER)

// 关闭 BLE 时把功能标志也压成关，避免状态栏显示"已开启"但任务根本不存在
static inline void p4UserCfgApplyTaskGates() {
#if CFG_ENABLE_BLE_TASK == 0
  s3xyBluetoothEnabled = false;
#endif
}

// ---------------------------------------------------------------------------
//  5) 运行期自愈：由主 loop() 每秒调用一次
// ---------------------------------------------------------------------------
//  上游把召唤心跳覆盖设计成"RAM-only，每次开机和每次 CAN 全恢复都回到 disabled"
//  （见 vehicle_logic.h 的注释与 can_runtime.h 里 invalidate 流程中的
//   summonHeartbeatOverrideResetUnderTxBarrier()）。
//  配置区只在开机灌一次，所以行车中一旦发生 CAN 恢复，召唤覆盖就被悄悄清掉且不再恢复。
//  这里做周期性自愈：发现"配置要求开、实际却是关"就重新灌一次。
//  必须放在 loop() 里调用：summonHeartbeatOverrideApply() 自己会取 canTxBarrierMutex，
//  在持有该锁的恢复流程里调用会死锁。
static inline void p4UserCfgMaintain() {
#if CFG_SUMMON_HEARTBEAT > 0
  if (!labMenuEnabled) return;
  bool enabled;
  portENTER_CRITICAL(&lab3f8Mux);
  enabled = summonHeartbeatOverrideEnabled;
  portEXIT_CRITICAL(&lab3f8Mux);
  if (!enabled) {
    const uint8_t v = (CFG_SUMMON_HEARTBEAT_VALUE >= 0)
        ? (uint8_t)CFG_SUMMON_HEARTBEAT_VALUE : 2u;
    (void)summonHeartbeatOverrideApply(true, v);
  }
#endif
}

#endif  // P4CAN_USER_CONFIG
