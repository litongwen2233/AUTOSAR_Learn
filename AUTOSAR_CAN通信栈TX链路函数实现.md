AUTOSAR 发送链路文档（SWC → CAN 硬件）
============================

> 本文档梳理一条 CAN 发送报文从应用层软件组件（SWC）到 CAN 硬件控制器的完整调用链路，覆盖 **Com、PduR、CanIf、Can 驱动** 四个 BSW 层级，逐层说明各函数职责、关键机制与参数传递。

* * *

一、整体链路总览
--------

    ┌─────────────────────────────────────────────────────────────┐
    │  应用层 SWC                                                    │
    │      │  (RTE 调用)                                             │
    │      ▼                                                         │
    │  Com_SendSignal           ← 写入信号值到 Com 发送缓存           │
    │      │                                                         │
    │      ▼  (打包 / 触发发送)                                       │
    │  Com_Internal_TriggerIPduSend  ← Com 内部真正执行发送           │
    │      │                                                         │
    │      ▼                                                         │
    │  PduR_ComTransmit         ← Com 进入 PduR 的入口                │
    │      │                                                         │
    │      ▼                                                         │
    │  PduR_ARC_Transmit        ← 查路由表 + 循环分发所有目标          │
    │      │                                                         │
    │      ▼                                                         │
    │  PduR_ARC_RouteTransmit   ← 单目标转发（switch 分发）            │
    │      │                                                         │
    │      ▼                                                         │
    │  CanIf_Transmit           ← CanIf 校验 + 组装 + 下抛            │
    │      │                                                         │
    │      ▼                                                         │
    │  Can_Write                ← CAN 驱动写硬件发送邮箱              │
    │      │                                                         │
    │      ▼                                                         │
    │  CAN 硬件控制器  ───► CAN 总线                                 │
    └─────────────────────────────────────────────────────────────┘

**核心分层说明：**

| 层级  | 模块    | 职责                           |
| --- | ----- | ---------------------------- |
| 应用层 | SWC   | 提供信号值，通过 RTE 调用 Com 接口       |
| 通信层 | Com   | 信号打包成 I-PDU、发送节流、更新位管理       |
| 路由层 | PduR  | I-PDU 路由分发（一对多）              |
| 接口层 | CanIf | 参数校验、控制器/通道模式检查、映射到 CAN 硬件对象 |
| 驱动层 | Can   | 操作 CAN 控制器寄存器，写入发送邮箱         |

* * *

二、链路各节点详解
---------

### 节点 0：Com_SendSignal（应用层入口）

    uint8 Com_SendSignal(Com_SignalIdType SignalId, const void *SignalDataPtr);

* **职责**：SWC 通过 RTE 调用，把一个信号的值写入 Com 的发送缓存
* **动作**：将 `SignalDataPtr` 指向的数据拷贝到对应 I-PDU 的数据区，并（若配置）**置位该信号的更新位**
* **返回**：0=成功；非 0=失败

> 此接口仅"写值"，不直接触发硬件发送；实际发送由发送模式（周期/触发）驱动。

* * *

### 节点 1：Com_Internal_TriggerIPduSend（Com 内部执行发送）

    Std_ReturnType Com_Internal_TriggerIPduSend(PduIdType ComTxPduId);

**职责**：Com 内部真正执行"发送一条发送 I-PDU"的核心实现。

**关键流程**：

| 步骤  | 动作                                 |
| --- | ---------------------------------- |
| ①   | `PDU_ID_CHECK` 校验 PDU 句柄           |
| ②   | 检查 `PDUR_COM_SUPPORT` 编译开关         |
| ③   | 取出静态配置 `IPdu` 与运行态 `Arc_IPdu`      |
| ④   | `Irq_Save` 关中断，保护临界区               |
| ⑤   | 检查发送缓冲区是否被锁定（`isPduBufferLocked`）  |
| ⑥   | 检查发送条件：最小间隔定时器为 0 且 I-PDU 已启动      |
| ⑦   | 调用发送前回调 `ComIPduCallout`（可选）       |
| ⑧   | 组装 `PduInfoType`（数据指针 + 长度，处理动态信号） |
| ⑨   | 调用 `PduR_ComTransmit` 下发           |
| ⑩   | 成功后清除各信号更新位、重置最小间隔定时器              |

**关键机制：**

* **最小发送间隔（Minimum Delay Timer）**：`ComTxIPduMinimumDelayTimer` 每次发送后重置为 `ComTxIPduMinimumDelayFactor`，递减到 0 前禁止再发，用于**限制发送频率**。
* **更新位（Update Bit）**：发送成功后 `CLEARBIT` 清除各信号更新位，标记"已发送"。
* **发送前回调（Callout）**：`ComIPduCallout` 可对数据预处理，返回 FALSE 则拒绝发送。
* **动态信号长度**：含动态信号时，长度 = 固定部分 + `Com_Arc_DynSignalLength`。

> ⚠️ **实现警示**：该函数在多个失败分支（缓冲区锁定、条件不满足、PduR 失败）中**直接 `return` 而未调用 `Irq_Restore(state)`**，会导致中断被永久关闭，属严重缺陷。实际工程应在所有返回路径前统一恢复中断。

* * *

### 节点 2：PduR_ComTransmit（Com → PduR 入口）

* **职责**：Com 调用 PduR 的发送接口，作为"从 Com 进入 PduR"的入口
* **作用**：将 Com 的发送请求交给 PduR，由 PduR 按配置路由到目标模块
* 参数：`ArcIPduOutgoingId`（出向 I-PDU 的 PduR 句柄）+ `PduInfoPackage`（数据描述）

* * *

### 节点 3：PduR_ARC_Transmit（PduR 路由分发器）

    Std_ReturnType PduR_ARC_Transmit(PduIdType PduId, const PduInfoType* PduInfo, uint8 serviceId);

**职责**：PduR 的发送入口，按 `PduId` 查出路由路径，**循环分发给路由路径上的所有目标**。

**流程：**

1. 三重校验：`PDUR_VALIDATE_INITIALIZED` / `PDUR_VALIDATE_PDUPTR` / `PDUR_VALIDATE_PDUID`
2. 用 `PduId` 直接索引取出 `PduRConfig->RoutingPaths[PduId]`
3. 遍历 `PduRDestPdus[]`（以 `NULL` 为结束哨兵），逐个调用 `PduR_ARC_RouteTransmit`
4. `retVal |= ...` 汇总结果：**全体成功才 E_OK，任一失败即 E_NOT_OK**

> 💡 **实现提示**：`retVal |=` 依赖 `E_OK=0 / E_NOT_OK=1` 的假设。更稳妥的写法是显式判断 `!= E_OK` 后置位。

* * *

### 节点 4：PduR_ARC_RouteTransmit（单目标转发）

    Std_ReturnType PduR_ARC_RouteTransmit(const PduRDestPdu_type * destination, const PduInfoType * pduInfo);

**职责**：根据 `destination->DestModule`，把 I-PDU 转发给对应目标模块。**switch 分发**：

| case               | 目标模块    | 调用接口               |
| ------------------ | ------- | ------------------ |
| `ARC_PDUR_CANIF`   | CanIf   | `CanIf_Transmit`   |
| `ARC_PDUR_COM`     | Com     | `Com_RxIndication` |
| `ARC_PDUR_LINIF`   | LinIf   | `LinIf_Transmit`   |
| `ARC_PDUR_CANTP`   | CanTp   | `CanTp_Transmit`   |
| `ARC_PDUR_SOADIF`  | SoAdIf  | `SoAdIf_Transmit`  |
| `ARC_PDUR_SOADTP`  | SoAdTp  | `SoAdTp_Transmit`  |
| `ARC_PDUR_J1939TP` | J1939Tp | `J1939Tp_Transmit` |

**要点：**

* 每个分支由独立的编译开关（如 `PDUR_CANIF_SUPPORT == STD_ON`）控制
* **PduR 只负责路由，不判断通信开关**（开关由 ComM/CanSM 控制）
* `ARC_PDUR_COM` 分支调用的是 `Com_RxIndication`（void 返回），因此该分支下 `retVal` 保持默认 `E_NOT_OK`

* * *

### 节点 5：CanIf_Transmit（CanIf 接口层发送）

    Std_ReturnType CanIf_Transmit(PduIdType CanTxPduId, const PduInfoType *PduInfoPtr);

**职责**：将 I-PDU 下抛给 CAN 驱动（`Can_Write`），并做多层校验。

**校验链条：**

| 顺序  | 检查项                                             | 失败原因                      |
| --- | ----------------------------------------------- | ------------------------- |
| ①   | 模块已初始化                                          | `CANIF_E_UNINIT`          |
| ②   | 数据指针非空                                          | `CANIF_E_PARAM_POINTER`   |
| ③   | PDU 句柄有效（`CanIf_FindTxPduEntry`）                | `CANIF_E_INVALID_TXPDUID` |
| ④   | 控制器处于 `CANIF_CS_STARTED`                        | 控制器未启动（CANIF_161）         |
| ⑤   | 通道处于 `CANIF_GET_TX_ONLINE` 或 `CANIF_GET_ONLINE` | 通道离线                      |

**组装与下抛：**
    canPdu.id          = txEntry->CanIfCanTxPduIdCanId;   // CAN ID
    canPdu.length      = PduInfoPtr->SduLength;           // 数据长度
    canPdu.sdu         = PduInfoPtr->SduDataPtr;          // 数据指针
    canPdu.swPduHandle = CanTxPduId;                      // 用于发送确认回传
    Can_Write(txEntry->CanIfCanTxPduHthRef->CanIfHthIdSymRef, &canPdu);

**要点：**

* `swPduHandle` 用于发送完成后 Can 驱动**回传确认**给 CanIf
* CanIf **不支持 Tx 缓冲**：`CAN_BUSY` 时直接返回 `E_NOT_OK`
* `CanIfCanTxPduHthRef` 关联 HTH（硬件发送句柄），映射到 CAN 驱动硬件对象

* * *

### 节点 6：Can_Write（CAN 驱动层）

* **职责**：操作 CAN 控制器寄存器，将 L-PDU 写入**发送邮箱（Hardware Object）**
* **返回值**：
  * `CAN_OK`：成功提交
  * `CAN_BUSY`：发送邮箱忙，无可用缓冲
  * `CAN_NOT_OK`：失败
* 发送完成后，CAN 硬件触发中断 → 驱动回调 CanIf 发送确认

* * *

三、发送确认（回调链，反方向）
---------------

发送真正完成后，确认沿**反方向**回调：
    CAN 硬件中断 → Can 驱动 → CanIf 回调(CanIfUserTxConfirmation)
         → PduR → Com_TxConfirmation

* **`CanIfUserTxConfirmation`**：目标上层可为 PduR、CanNm、CanTp
* **`Com_TxConfirmation`**：Com 层接收端，行为取决于确认模式（Immediate 时可能为空实现）

* * *

四、关键机制汇总表
---------

| 机制         | 所在层   | 说明                     |
| ---------- | ----- | ---------------------- |
| 信号打包       | Com   | 多个信号打包进一条 I-PDU        |
| 最小发送间隔     | Com   | 限制 I-PDU 发送频率          |
| 更新位        | Com   | 标记信号是否被更新，避免重复发送       |
| 发送前回调      | Com   | Callout 预处理/过滤数据       |
| 路由分发       | PduR  | 一条 I-PDU 可分发给多个目标（一对多） |
| 控制器/通道模式检查 | CanIf | 确保控制器启动、通道在线           |
| HTH 硬件句柄映射 | CanIf | 映射应用 L-PDU 到 CAN 硬件对象  |
| 发送确认回调     | 全链路   | 发送完成后反向通知上层            |

* * *

五、术语表
-----

| 简写    | 全称                       | 含义          |
| ----- | ------------------------ | ----------- |
| SWC   | Software Component       | 应用层软件组件     |
| RTE   | Runtime Environment      | 运行时环境       |
| I-PDU | Interaction Layer PDU    | 交互层协议数据单元   |
| L-PDU | Data Link Layer PDU      | 数据链路层协议数据单元 |
| HTH   | Hardware Transmit Handle | 硬件发送句柄      |
| HOH   | Hardware Object Handle   | 硬件对象句柄      |
| DET   | Default Error Tracer     | 默认错误追踪模块    |

* * *

六、可直接复用的链路速查（单行版）
-----------------

    SWC → RTE → Com_SendSignal → (打包) → Com_Internal_TriggerIPduSend    → PduR_ComTransmit → PduR_ARC_Transmit → PduR_ARC_RouteTransmit    → CanIf_Transmit → Can_Write → CAN 硬件

* * *

> 文档说明：本文档依据你提供的工程代码（`CanIf_TxPduConfigType`、`CanIf_Transmit`、`Com_TxConfirmation`、`Com_Internal_TriggerIPduSend`、`PduR_ARC_Transmit`、`PduR_ARC_RouteTransmit`）整理，未展开的部分（如 `PduR_ComTransmit`、`Can_Write` 内部实现）为通用 AUTOSAR 语义描述。
