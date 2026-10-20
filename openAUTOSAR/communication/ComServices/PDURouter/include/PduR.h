/* -------------------------------- Arctic Core ------------------------------
 * Arctic Core - the open source AUTOSAR platform http://arccore.com
 *
 * Copyright (C) 2009  ArcCore AB <contact@arccore.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * -------------------------------- Arctic Core ------------------------------*/








#ifndef PDUR_H
#define PDUR_H

#include "Modules.h"

#define PDUR_VENDOR_ID		   VENDOR_ID_ARCCORE
#define PDUR_AR_MAJOR_VERSION  3
#define PDUR_AR_MINOR_VERSION  1
#define PDUR_AR_PATCH_VERSION  5
#define PDUR_SW_MAJOR_VERSION  2
#define PDUR_SW_MINOR_VERSION  0
#define PDUR_SW_PATCH_VERSION  0



// ERROR CODES
#define PDUR_E_CONFIG_PTR_INVALID 		0x06
#define PDUR_E_INVALID_REQUEST 			0x01
#define PDUR_E_PDU_ID_INVALID			0x02
#define PDUR_E_TP_TX_REQ_REJECTED		0x03
#define PDUR_E_DATA_PTR_INVALID			0x05
#define PDUR_E_BUFFER_ERROR				0x06

#define PDUR_INSTANCE_ID	0

#include "Modules.h"

#include "PduR_Cfg.h"
#include "PduR_Types.h"
#include "PduR_PbCfg.h"

#include "PduR_If.h"

#if PDUR_COM_SUPPORT == STD_ON
#include "PduR_Com.h"
#endif
#if PDUR_CANIF_SUPPORT == STD_ON
#include "PduR_CanIf.h"
#endif
#if PDUR_LINIF_SUPPORT == STD_ON
#include "PduR_LinIf.h"
#endif
#if PDUR_CANTP_SUPPORT == STD_ON
#include "PduR_CanTp.h"
#endif
#if PDUR_J1939TP_SUPPORT == STD_ON
#include "PduR_J1939Tp.h"
#endif
#if PDUR_DCM_SUPPORT == STD_ON
#include "PduR_Dcm.h"
#endif
#if PDUR_SOAD_SUPPORT == STD_ON
#include "PduR_SoAd.h"
#endif
#if PDUR_J1939TP_SUPPORT == STD_ON
#include "PduR_J1939Tp.h"
#endif


/* Contain the current state of the PDU router. The router is uninitialized
 * until PduR_Init has been run.
 */
//PduR_StateType PduRState;

extern const PduR_PBConfigType *PduRConfig;

/*
 * The state of the PDU router.
 */
extern PduR_StateType PduRState;

#define PduR_IsUpModule(_mod) ((_mod > ARC_PDUR_UP_MODULES) && (_mod < ARC_PDUR_LOIF_MODULES))
#define PduR_IsIfModule(_mod) ((_mod > ARC_PDUR_LOIF_MODULES) && (_mod < ARC_PDUR_LOTP_MODULES))
#define PduR_IsTpModule(_mod) ((_mod > ARC_PDUR_LOTP_MODULES) && (_mod < ARC_PDUR_END_OF_MODULES))
#define PduR_IsLoModule(_mod) (PduR_IsIfModule(_mod) || PduR_IsTpModule(_mod))


#if (PDUR_DEV_ERROR_DETECT == STD_ON)

#define PDUR_DET_REPORTERROR(_x,_y,_z,_o) Det_ReportError(_x,_y,_z,_o)

#define PDUR_VALIDATE_INITIALIZED(_api,...) \
	if ((PduRState == PDUR_UNINIT) || (PduRState == PDUR_REDUCED)) { \
		Det_ReportError(MODULE_ID_PDUR, PDUR_INSTANCE_ID, _api, PDUR_E_INVALID_REQUEST); \
		return __VA_ARGS__; \
	}

#define PDUR_VALIDATE_PDUPTR(_api, _pduPtr, ...) \
	if ((_pduPtr == NULL) && (PDUR_DEV_ERROR_DETECT)) { \
		Det_ReportError(MODULE_ID_PDUR, PDUR_INSTANCE_ID, _api, PDUR_E_DATA_PTR_INVALID); \
		return __VA_ARGS__; \
	}

#define PDUR_VALIDATE_PDUID(_api, _pduId, ...) \
	if ((_pduId >= PduRConfig->NRoutingPaths) && PDUR_DEV_ERROR_DETECT) { \
		Det_ReportError(MODULE_ID_PDUR, PDUR_INSTANCE_ID, _api, PDUR_E_PDU_ID_INVALID); \
		return __VA_ARGS__; \
	}


#else
#define PDUR_DET_REPORTERROR(_x,_y,_z,_o)
#define PDUR_VALIDATE_INITIALIZED(_api,...)
#define PDUR_VALIDATE_PDUPTR(_api, _pduPtr, ...)
#define PDUR_VALIDATE_PDUID(_api, _pduId, ...)

#endif

/*** 发送控制类（核心对外接口） */
/**
 * 取消一个尚未完成/正在排队的发送请求
 * @param PduCancelReason 取消原因：
 *    PDUR_CANCEL_TIMEOUT      超时取消
 *    PDUR_CANCEL_RXPDU_DATA   （接收数据取消，规范扩展）
 *    PDUR_CANCEL_TRANSMIT     （主动取消发送）
 * @param PduId              要取消的 PDU ID
 * @return E_OK 取消成功 / E_NOT_OK 无法取消（如已发出或ID非法）
 *
 * 触发场景：上层调用了 CanIf_Transmit 入队后，因某种原因（如
 *           新的更高优先级数据到来、上层模块被重置）想撤回该帧。
 *
 * 裸机对应：你在发送邮箱里"清掉一个还没发出/正在排队的报文"。
 */
Std_ReturnType PduR_CancelTransmitRequest(
		PduR_CancelReasonType PduCancelReason, PduIdType PduId);
/**
 * 运行时修改某个 PDU 的路由参数（如发送模式、通道等）
 * @param PduParameterValue 要修改的参数值/类型（见 PduR_ParameterValueType）
 * @param PduId             目标 PDU ID
 *
 * 触发场景：通信模式切换（如诊断模式进入/退出）时，动态调整某条
 *           报文的发送参数。注意是异步请求，实际生效由底层处理。
 *
 * 裸机对应：你在运行时改了某条报文的发送优先级/使能。
 */		
void PduR_ChangeParameterRequest(PduR_ParameterValueType PduParameterValue,
		PduIdType PduId);

/* Zero Cost Operation function definitions
 * These macros replaces the original functions if zero cost
 * operation is desired. */
/* =====================================================================
 * 零成本操作（ZERO COST OPERATION）
 * 当 PDUR_ZERO_COST_OPERATION == STD_ON 时：
 * 这几个函数被编译为空宏/常量，不产生任何实际代码（运行时开销为0）。
 *
 * 意义：如果工程里 PduR 是"纯静态路由"（路由表编译期固定、无需
 *       动态查表或初始化），可开启零成本模式，省去函数调用开销。
 * ===================================================================== */
#if PDUR_ZERO_COST_OPERATION == STD_ON
#define PduR_Init(...)
#define PduR_GetVersionInfo(...)
#define PduR_GetConfigurationId(...) 0

#else // Not zero cost operation
//#error fail
/**
 * 初始化 PduR 模块
 * @param ConfigPtr 指向 PduR 全局配置结构（路由表，由配置工具生成）
 *
 * 调用时机：ECU 启动阶段，在 Com/CanIf 之前、通信功能使用前初始化一次。
 * 裸机对应：初始化全局路由表/查表指针。
 */
void PduR_Init(const PduR_PBConfigType* ConfigPtr);
/**
 * 获取 PduR 模块版本信息
 * @param versionInfo [out] 返回版本号结构
 * 用于调试和版本对齐检查。
 */
void PduR_GetVersionInfo(Std_VersionInfoType* versionInfo);

/**
 * 获取当前路由配置的 ID（用于区分多套配置）
 * @return 配置 ID 编号
 * 用于运行时确认当前用的是哪套路由表。
 */
uint32 PduR_GetConfigurationId(void);

/**
 * 发送缓冲：移动写指针
 * @param Buffer  发送缓冲结构
 * @param ptr     [in/out] 当前字节指针，写入后向前移动
 * 内部辅助：构建多字节数据时逐字节写并推进指针。
 */
void PduR_BufferInc(PduRTxBuffer_type *Buffer, uint8 **ptr);
/**
 * 发送缓冲：入队一帧数据
 * @param Buffer  发送缓冲结构
 * @param SduPtr  指向要写入缓冲的数据
 * 内部辅助：把一份 SDU 数据压入发送队列。
 */
void PduR_BufferQueue(PduRTxBuffer_type *Buffer, const uint8 * SduPtr);
/**
 * 发送缓冲：出队一帧数据
 * @param Buffer  发送缓冲结构
 * @param SduPtr  [out] 取出数据到该缓冲区
 * 内部辅助：从发送队列弹出一份数据。
 */
void PduR_BufferDeQueue(PduRTxBuffer_type *Buffer, uint8 *SduPtr);
/**
 * 发送缓冲：清空整个缓冲
 * @param Buffer 发送缓冲结构
 * 内部辅助：丢弃队列中所有未发送数据（如模块重置时）。
 */
void PduR_BufferFlush(PduRTxBuffer_type *Buffer);
/**
 * 检查发送缓冲是否已满
 * @param Buffer 发送缓冲结构
 * @return 非0表示已满（不能再入队）/ 0表示有空间
 * 内部辅助：入队前检查，防止缓冲溢出。
 */
uint8 PduR_BufferIsFull(PduRTxBuffer_type *Buffer);
/**
 * 低层接口接收通知：LoIf 收到数据后通知 PduR
 * @param PduId 收到的 PDU ID
 * @param SduPtr 指向收到的数据
 *
 * 触发链路：CanIf/LinIf 收到帧 → LoIf 处理后 → 此回调 → PduR 路由分发
 * 与 PduR_CanIfRxIndication 类似，但面向更底层的 LoIf 抽象。
 */
void PduR_LoIfRxIndication(PduIdType PduId, const uint8* SduPtr);
/**
 * 低层接口发送完成确认：LoIf 发出帧后通知 PduR
 * @param PduId 已发送的 PDU ID
 * 与 PduR_CanIfTxConfirmation 类似，面向 LoIf 抽象层。
 */
void PduR_LoIfTxConfirmation(PduIdType PduId);
/**
 * 低层接口触发发送：LoIf 需要数据时反向从 PduR 拉取
 * @param PduId  要触发的发送 PDU ID
 * @param SduPtr [out] 指向用于填充发送数据的缓冲区
 *
 * 这是"拉模式"发送：底层准备好发送时，主动调用此回调向 PduR 要数据，
 * PduR 把对应缓冲里的数据填到 SduPtr 交给底层发出。
 */
void PduR_LoIfTriggerTransmit(PduIdType PduId, uint8* SduPtr);

/*
 * Macros
 */
#define setTxConfP(_B) (_B->TxConfP = 1)
#define clearTxConfP(_B) (_B->TxConfP = 0)

#endif

#endif /* PDUR_H */
