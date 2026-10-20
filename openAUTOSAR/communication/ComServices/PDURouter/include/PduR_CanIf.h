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
/* =====================================================================
 * Com 层回调：由 PduR 在收到数据 / 发送确认时调用
 * 位置：PduR 之上，Com 之下（PduR → Com 方向）
 * 作用：把接收到的信号报文解包给应用、把发送完成状态上报应用
 * ===================================================================== */







#ifndef PDUR_CANIF_H_
#define PDUR_CANIF_H_

#include "PduR.h"

#if PDUR_ZERO_COST_OPERATION == STD_OFF

/**
 * 接收通知回调 —— PduR 收到一帧"普通信号报文"后调用 Com
 * @param RxPduId     接收 PDU 的 ID（Com 靠它定位是哪条接收报文）
 * @param PduInfoPtr  指向收到的数据描述（含数据指针、长度）
 *
 * 触发链路（正常接收流程）：
 *   CAN 总线 → Can 驱动
 *     → CanIf_RxIndication()              // CanIf 收到帧
 *       → PduR_CanIfRxIndication()        // PduR 收到通知，查路由
 *         → Com_RxIndication(RxPduId, info)  // PduR 路由到 Com（信号报文）
 *           → 应用层通过 Rte_Read 读取解包后的信号值
 *
 * BMS 场景：VCU 下发充电控制指令，PduR 把该帧路由给 Com，
 *           应用层 Runnable 调用 Rte_Read 读到充电电压/电流目标值。
 *
 * 裸机对应：你接收中断里"收到帧后解析出各个信号"的那一步。
 */
void PduR_CanIfRxIndication(PduIdType CanRxPduId,const PduInfoType* PduInfoPtr);
/**
 * 发送完成确认回调 —— 底层确认"上一帧真正发出去了"后调用 Com
 * @param TxPduId 发送 PDU 的 ID（Com 靠它定位是哪条发送报文）
 *
 * 触发链路（正常发送确认流程）：
 *   应用 Runnable → Rte_Write → Com 组包
 *     → PduR_Transmit() → CanIf_Transmit() → Can 驱动入邮箱
 *       CAN 硬件发送成功 → 发送中断
 *       → CanIf_TxConfirmation()
 *         → PduR_CanIfTxConfirmation()
 *           → Com_TxConfirmation(TxPduId)  // 最终上报应用"这帧发完了"
 *
 * BMS 场景：BMS 每 10ms 上报电池状态帧，收到此回调才知道该帧真正上总线，
 *           才能安全复用发送缓冲区（多帧连发时尤其关键）。
 *
 * 裸机对应：你发送完成中断里"清标志、释放发送缓冲区"的那一步。
 */
void PduR_CanIfTxConfirmation(PduIdType CanTxPduId);

#endif

#endif /* PDUR_CANIF_H_ */
