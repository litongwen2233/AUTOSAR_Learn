/* =====================================================================
 * CanIf：CAN 接口层（CAN Interface）
 * 位于 MCAL 的 Can 驱动 与 上层（PduR/Com）之间，
 * 作用是"屏蔽 CAN 控制器差异，统一管理收发通道、控制器状态、唤醒等"
 * ===================================================================== */


#ifndef CANIF_H_
#define CANIF_H_
#include "Modules.h"

#if defined(USE_PDUR)
#include "PduR.h"
#endif

#if defined(USE_COM)
#include "Com.h"
#endif

#define CANIF_VENDOR_ID          VENDOR_ID_ARCCORE
#define CANIF_MODULE_ID          MODULE_ID_CANIF
#define CANIF_AR_MAJOR_VERSION   3
#define CANIF_AR_MINOR_VERSION   1
#define CANIF_AR_PATCH_VERSION   5

#define CANIF_SW_MAJOR_VERSION   1
#define CANIF_SW_MINOR_VERSION   3
#define CANIF_SW_PATCH_VERSION   0

#if defined(USE_DET)
#include "Det.h"
#endif
#include "CanIf_Types.h"
// At the moment of porting th software to the CMake build system 
// the inclusion of "CanIf_SpecialPdus.h" seems to be redundant
//#include "CanIf_SpecialPdus.h"
#include "CanIf_Cfg.h"


// Service IDs
#define CANIF_INIT_ID                0x01
#define CANIF_INIT_CONTROLLER_ID     0x02
#define CANIF_SET_CONTROLLER_MODE_ID 0x03
#define CANIF_GET_CONTROLLER_MODE_ID 0x04
#define CANIF_TRANSMIT_ID            0x05
#define CANIF_READTXPDUDATA_ID       0x06
#define CANIF_READTXNOTIFSTATUS_ID   0x07
#define CANIF_READRXNOTIFSTATUS_ID   0x08
#define CANIF_SETPDUMODE_ID          0x09
#define CANIF_GETPDUMODE_ID          0x0A
#define CANIF_SETDYNAMICTX_ID        0x0C
#define CANIF_SET_TRANSCEIVERMODE_ID     0x0D
#define CANIF_GET_TRANSCEIVERMODE_ID     0x0E
#define CANIF_GET_TRCVMODEREASON_ID      0x0F
#define CANIF_SET_TRANSCEIVERWAKEMODE_ID 0x10
#define CANIF_CHECKWAKEUP_ID          0x11
#define CANIF_CHECKVALIDATION_ID      0x12
#define CANIF_TXCONFIRMATION_ID       0x13
#define CANIF_RXINDICATION_ID         0x14
#define CANIF_CANCELTXCONFIRMATION_ID 0x15
#define CANIF_CONTROLLER_BUSOFF_ID    0x16

#define CANIF_SETWAKEUPEVENT_ID       0x40
#define CANIF_ARCERROR_ID           0x41

/**
 * 初始化 CanIf 模块（整体初始化）
 * @param ConfigPtr 指向 CanIf 全局配置结构（由配置工具生成的静态配置）
 *
 * 调用时机：ECU 启动阶段，在 Can 驱动初始化之后、使用通信功能之前，由 EcuM 调用一次。
 * 裸机对应：你裸机里初始化 CAN 控制器前对全局收发缓冲、回调函数指针的初始化。
 */
void CanIf_Init(const CanIf_ConfigType *ConfigPtr);

/**
 * 初始化单个 CAN 控制器
 * @param Controller          控制器索引（对应硬件上的第几个 CAN 外设，如 CAN0/CAN1）
 * @param ConfigurationIndex  配置索引（一套代码支持多套配置，默认传 0）
 *
 * 调用时机：EcuM/CanSM 在需要启用某个 CAN 通道时调用。
 * 裸机对应：`CAN_Init(CAN1, 500kbps)` 那一步。
 */
void CanIf_InitController(uint8  Controller,
	    uint8  ConfigurationIndex);

/**
 * 设置控制器工作模式（请求切换状态）
 * @param Controller      控制器索引
 * @param ControllerMode  目标模式：
 *    CANIF_CSM_UNINIT       未初始化
 *    CANIF_CSM_STOPPED      停止（总线关闭/睡眠）
 *    CANIF_CSM_STARTED      运行（可收发）
 *    CANIF_CSM_SLEEP        休眠（低功耗）
 * @return E_OK 成功 / E_NOT_OK 失败（如模式切换被拒绝）
 *
 * 调用时机：CanSM（CAN 状态管理）根据总线状态请求切换，如唤醒时置 STARTED。
 * 裸机对应：你把 CAN 外设从 sleep 唤醒、进入正常模式（`CAN_Enter_Normal_Mode()`）。
 */		
Std_ReturnType CanIf_SetControllerMode(uint8 Controller,
		CanIf_ControllerModeType ControllerMode);

/**
 * 读取当前控制器工作模式
 * @param Controller           控制器索引
 * @param ControllerModePtr    [out] 返回当前模式
 * @return E_OK / E_NOT_OK
 *
 * 裸机对应：`CAN_Get_State()` 查询控制器当前是 run 还是 sleep。
 */
Std_ReturnType CanIf_GetControllerMode(uint8 Controller,
		CanIf_ControllerModeType *ControllerModePtr);

/**
 * 发送一帧 CAN 报文（CanIf 对外的最核心发送接口）
 * @param CanTxPduId 发送 PDU 的 ID（配置里为每个发送报文分配的唯一编号）
 * @param PduInfoPtr 指向 PDU 数据描述（含数据指针、长度、标识符）
 * @return E_OK 已接受 / E_NOT_OK 拒绝发送（如控制器未就绪）
 *
 * 调用时机：上层 PduR 调用（PduR_Transmit → CanIf_Transmit），
 *          CanIf 内部再调用底层 Can_Write 发给驱动。
 * 裸机对应：`CAN_Transmit(CAN1, &tx_msg)` —— 但从"直接发驱动"变成了
 *          标准接口，上层不关心底层用的是 STM32 还是 S32K 的 CAN。
 */
Std_ReturnType CanIf_Transmit(PduIdType CanTxPduId,
		const PduInfoType *PduInfoPtr);

/* =====================================================================
 * 读取接收 PDU 数据（条件编译）
 * CANIF_READRXPDU_DATA_API == STD_ON 时才编译
 * ===================================================================== */

 /**
 * 读取接收缓冲区中某条 PDU 的数据（主动读取方式）
 * @param CanRxPduId 接收 PDU 的 ID
 * @param PduInfoPtr [in/out] 传入缓冲区，返回收到的数据
 * @return E_OK 有数据并读出 / E_NOT_OK 无新数据
 *
 * 裸机对应：你在接收中断里 `CAN_Read_RxMailbox()` 主动取走数据。
 * 注意：AUTOSAR 接收通常走"回调通知"（CanIf_RxIndication）而不是主动读，
 *       此函数是提供"轮询读取"备选路径。
 */
#if ( CANIF_READRXPDU_DATA_API == STD_ON )
Std_ReturnType CanIf_ReadRxPduData(PduIdType CanRxPduId,
		PduInfoType *PduInfoPtr);
#endif

/* =====================================================================
 * 读取发送/接收 PDU 的通知状态（条件编译）
 * CANIF_READTXPDU_NOTIFY_STATUS_API == STD_ON 时才编译
 * ===================================================================== */

/**
 * 查询某条发送 PDU 是否已发送完成
 * @param CanTxPduId 发送 PDU 的 ID
 * @return 通知状态（如 CANIF_TX_RX_NOTIFICATION 等）
 *
 * 裸机对应：查询发送邮箱的空闲标志位 `CAN_TxMailbox_IsEmpty()`。
 * 可用于非回调方式判断"上一帧发完没"。
 */
#if ( CANIF_READTXPDU_NOTIFY_STATUS_API == STD_ON )
CanIf_NotifStatusType CanIf_ReadTxNotifStatus(PduIdType CanTxPduId);
#endif

/**
 * 查询某条接收 PDU 是否有新数据通知
 * @param CanRxPduId 接收 PDU 的 ID
 * @return 通知状态
 *
 * 裸机对应：查询接收邮箱的非空标志 `CAN_RxMailbox_IsFull()`。
 */
#if ( CANIF_READTXPDU_NOTIFY_STATUS_API == STD_ON )
CanIf_NotifStatusType CanIf_ReadRxNotifStatus(PduIdType CanRxPduId);
#endif

/* =====================================================================
 * PDU 收发模式设置（在线/离线）
 * ===================================================================== */

/**
* 设置某控制器的 PDU 收发模式
* @param Controller     控制器索引
* @param PduModeRequest 请求模式：
*    CANIF_SET_PDU_MODE_ON   在线（可收发）
*    CANIF_SET_PDU_MODE_OFF  离线（禁止收发）
*    CANIF_SET_PDU_MODE_RX_ONLY 只收不发
* @return E_OK / E_NOT_OK
*
* 调用时机：CanSM/应用层在需要暂停通信（如诊断静默、总线调试）时调用。
* 裸机对应：你在调试时关闭某路 CAN 的收发使能。
*/
Std_ReturnType CanIf_SetPduMode( uint8 Controller, CanIf_ChannelSetModeType PduModeRequest );
/**
 * 读取某控制器的当前 PDU 收发模式
 * @param Controller  控制器索引
 * @param PduModePtr  [out] 返回当前模式
 * @return E_OK / E_NOT_OK
 */
Std_ReturnType CanIf_GetPduMode( uint8 Controller, CanIf_ChannelGetModeType *PduModePtr );

/* =====================================================================
 * 运行时动态配置 PDU（ARC 扩展，条件编译）
 * CANIF_ARC_RUNTIME_PDU_CONFIGURATION == STD_ON 时才编译
 * 说明：这是 Arctic Core / openAUTOSAR 的扩展接口，标准 AUTOSAR 规范里没有
 * ===================================================================== */

#if ( CANIF_ARC_RUNTIME_PDU_CONFIGURATION == STD_ON )
/**
 * 动态修改发送 PDU 的 CAN ID（运行时改报文标识符）
 * @param CanTxPduId 发送 PDU 的 ID
 * @param CanId      新的 CAN ID
 *
 * 用途：多路复用的 CAN ID 切换（如不同车型用不同报文 ID）可在运行时改。
 */
void CanIf_SetDynamicTxId( PduIdType CanTxPduId, Can_IdType CanId );
/**
 * 按 PDU ID 查找发送 PDU 的配置项
 * @param id 发送 PDU ID
 * @return 指向发送 PDU 配置结构的指针（找不到返回 NULL）
 */
CanIf_TxPduConfigType * CanIf_FindTxPduEntry(PduIdType id);
/**
 * 按 PDU ID 查找接收 PDU 的配置项
 * @param id 接收 PDU ID
 * @return 指向接收 PDU 配置结构的指针（找不到返回 NULL）
 */
CanIf_RxPduConfigType * CanIf_FindRxPduEntry(PduIdType id);
/**
 * 获取指定通道的接收句柄（HRH, Hardware Receive Handler）
 * @param Channel 通道索引
 * @return 指向接收句柄配置的指针
 */
const CanIf_HrhConfigType* CanIf_Arc_GetReceiveHandler(CanIf_Arc_ChannelIdType Channel);
/**
 * 获取指定通道的发送句柄（HTH, Hardware Transmit Handler）
 * @param Channel 通道索引
 * @return 指向发送句柄配置的指针
 */
const CanIf_HthConfigType* CanIf_Arc_GetTransmitHandler(CanIf_Arc_ChannelIdType Channel);
#endif

/* =====================================================================
 * 收发器（Transceiver）控制（条件编译）
 * CANIF_TRANSCEIVER_API == STD_ON 时才编译
 * 说明：收发器即 CAN 总线物理层芯片（如 TJA1043），可控制其功耗模式
 * ===================================================================== */

#if ( CANIF_TRANSCEIVER_API == STD_ON )
/**
 * 设置收发器模式
 * @param Transceiver     收发器索引
 * @param TransceiverMode 目标模式（正常/待机/睡眠等）
 * @return E_OK / E_NOT_OK
 *
 * 用途：低功耗管理时把收发器切到 sleep，唤醒时切回 normal。
 */
Std_ReturnType CanIf_SetTransceiverMode( uint8 Transceiver, CanIf_TransceiverModeType TransceiverMode );
/**
 * 读取收发器当前模式
 * @param Transceiver      收发器索引
 * @param TransceiverModePtr [out] 返回当前模式
 */
Std_ReturnType CanIf_GetTransceiverMode( uint8 Transceiver, CanIf_TransceiverModeType *TransceiverModePtr );
/**
 * 读取收发器的唤醒原因
 * @param Transceiver      收发器索引
 * @param TrcvWuReasonPtr  [out] 返回唤醒原因（如总线活动唤醒）
 * @return E_OK / E_NOT_OK
 *
 * 用途：判断 ECU 是被 CAN 总线唤醒还是其他源唤醒。
 */
Std_ReturnType CanIf_GetTrcvWakeupReason( uint8 Transceiver, CanIf_TrcvWakeupReasonType *TrcvWuReasonPtr );
/**
 * 设置收发器唤醒模式（允许/禁止哪些唤醒源）
 * @param Transceiver     收发器索引
 * @param TrcvWakeupMode  [in/out] 唤醒模式
 */
Std_ReturnType CanIf_SetTransceiverWakeupMode( uint8 Transceiver, CanIf_TrcvWakeupModeType *TrcvWakeupMode );
#endif

/* =====================================================================
 * 唤醒事件处理（条件编译）
 * CANIF_WAKEUP_EVENT_API == STD_ON 时才编译
 * ===================================================================== */

#if ( CANIF_WAKEUP_EVENT_API == STD_ON )
/**
 * 检查某个唤醒源是否被 CanIf 认可为有效唤醒（第一阶段校验）
 * @param WakeupSource 唤醒源标识（由 EcuM 定义）
 * @return E_OK 该源被识别为唤醒 / E_NOT_OK
 *
 * 调用时机：EcuM 收到外部唤醒信号后，询问 CanIf"是不是你这条线唤醒的"。
 */
Std_ReturnType CanIf_CheckWakeup( EcuM_WakeupSourceType WakeupSource );
/**
 * 唤醒验证（第二阶段，进一步确认唤醒是否有效，防止误唤醒）
 * @param WakeupSource 唤醒源标识
 * @return E_OK 验证通过 / E_NOT_OK
 *
 * 用途：防止总线毛刺等误触发唤醒，需连续确认后才真正唤醒系统。
 */
Std_ReturnType CanIf_CheckValidation( EcuM_WakeupSourceType WakeupSource );
#endif

/* =====================================================================
 * 版本信息查询（条件编译）
 * CANIF_VERSION_INFO_API == STD_ON 时才编译
 * ===================================================================== */

#if ( CANIF_VERSION_INFO_API == STD_ON )
/**
 * 获取 CanIf 模块的版本信息
 * @param _vi [out] 版本信息结构体指针
 *
 * 用于调试、版本对齐检查。
 */
#define CanIf_GetVersionInfo(_vi) STD_GET_VERSION_INFO(_vi,CANIF)
#endif

/* =====================================================================
 * 扩展：获取通道默认配置索引（ARC 扩展，无条件编译）
 * ===================================================================== */

 /**
 * 获取指定通道的默认配置索引
 * @param Channel 通道索引
 * @return 默认配置索引（ConfigIndex）
 *
 * 用途：openAUTOSAR/Arctic Core 扩展接口，用于在配置索引与通道之间
 *       建立对应关系（一套代码多通道时的查表辅助）。
 */
uint8 CanIf_Arc_GetChannelDefaultConfIndex(CanIf_Arc_ChannelIdType Channel);

#endif /*CANIF_H_*/
//下层往上回调通知，上层往下请求"的模式