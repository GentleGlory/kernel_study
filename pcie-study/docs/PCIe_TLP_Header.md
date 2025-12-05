# PCIe TLP Header 欄位詳解

## TLP Header 基本結構

TLP Header 是 **3 或 4 個 DWord (12 或 16 bytes)** 的固定格式，包含路由和控制資訊。

---

## 📋 第一個 DWord (DW0) - 通用欄位

### **Byte 0 (位元 31:24)**

| 位元 | 欄位名稱 | 長度 | 描述 |
|------|----------|------|------|
| 31:29 | **Format (Fmt)** | 3 bits | TLP 格式類型 |
| 28:24 | **Type** | 5 bits | TLP 類型 |

#### **Format (Fmt) 編碼**
| Fmt | 描述 | Header 長度 | Data Payload |
|-----|------|-------------|--------------|
| 000 | 3DW Header, No Data | 3 DWords | 無 |
| 001 | 4DW Header, No Data | 4 DWords | 無 |
| 010 | 3DW Header, With Data | 3 DWords | 有 |
| 011 | 4DW Header, With Data | 4 DWords | 有 |
| 100 | TLP Prefix | - | 特殊用途 |

#### **Type 常見編碼**
| Type | TLP 類型 | 描述 |
|------|----------|------|
| 00000 | Memory Read Request | 記憶體讀取請求 |
| 00001 | Memory Read Request (Locked) | 鎖定記憶體讀取 |
| 01000 | Memory Write Request | 記憶體寫入請求 |
| 00010 | I/O Read Request | I/O 讀取請求 |
| 00011 | I/O Write Request | I/O 寫入請求 |
| 00100 | Configuration Read Type 0 | 配置讀取 Type 0 |
| 00101 | Configuration Write Type 0 | 配置寫入 Type 0 |
| 00110 | Configuration Read Type 1 | 配置讀取 Type 1 |
| 00111 | Configuration Write Type 1 | 配置寫入 Type 1 |
| 01010 | Completion | 完成封包 |
| 01011 | Completion (Locked) | 鎖定完成封包 |
| 10000 | Message Request | 訊息請求 |

### **Byte 1 (位元 23:16)**

| 位元 | 欄位名稱 | 描述 |
|------|----------|------|
| 23 | **T** | TLP Processing Hints |
| 22:20 | **TC (Traffic Class)** | 流量類別 (QoS) |
| 19:16 | **Attr[3:0]** | 屬性欄位 |

#### **Traffic Class (TC)**
- **3 bits**: 支援 8 個優先等級 (TC0-TC7)
- **TC0**: 最低優先
- **TC7**: 最高優先

#### **Attributes (Attr)**
| 位元 | 名稱 | 描述 |
|------|------|------|
| Attr[2] | **IDO** | ID-based Ordering |
| Attr[1] | **RO** | Relaxed Ordering |
| Attr[0] | **NS** | No Snoop |

### **Byte 2 (位元 15:8)**

| 位元 | 欄位名稱 | 描述 |
|------|----------|------|
| 15:14 | **TH** | TLP Processing Hints |
| 13:12 | **TD** | TLP Digest |
| 11:10 | **EP** | Poisoned |
| 9:8 | **Attr[5:4]** | 擴展屬性 |

### **Byte 3 (位元 7:0)**

| 位元 | 欄位名稱 | 描述 |
|------|----------|------|
| 7:2 | **AT (Address Type)** | 位址類型 |
| 1:0 | **Length[9:8]** | 資料長度高位 |

---

## 📋 第二個 DWord (DW1) - 長度和路由

### **完整的第二個 DWord**

| 位元 | 欄位名稱 | 描述 |
|------|----------|------|
| 31:22 | **Length[9:0]** | Payload 長度 (DWords) |
| 21:16 | **Requester ID[15:8]** | 請求者 ID 高位 |
| 15:0 | **Requester ID[7:0] + Tag** | 請求者 ID 低位 + Tag |

#### **Length 編碼**
- **10 bits**: 支援 0-1023 DWords
- **0**: 表示 1024 DWords
- **單位**: DWord (4 bytes)

#### **Requester ID**
- **16 bits**: Bus:Device.Function 格式
- **[15:8]**: Bus Number
- **[7:3]**: Device Number  
- **[2:0]**: Function Number

#### **Tag**
- **8 bits**: 用於配對 Request 和 Completion
- **範圍**: 0-255

---

## 📋 第三個 DWord (DW2) - 位址或其他資訊

### **Memory Request (3DW)**

| 位元 | 欄位名稱 | 描述 |
|------|----------|------|
| 31:2 | **Address[31:2]** | 32位元位址 (DWord 對齊) |
| 1:0 | **Reserved** | 保留 (必須為 00) |

### **Memory Request (4DW)**

| DW2 | **Address[63:32]** | 64位元位址高32位 |
| DW3 | **Address[31:2] + Reserved[1:0]** | 64位元位址低30位 + 保留 |

### **Configuration Request**

| 位元 | 欄位名稱 | 描述 |
|------|----------|------|
| 31:24 | **Bus Number** | 目標匯流排編號 |
| 23:19 | **Device Number** | 目標裝置編號 |
| 18:16 | **Function Number** | 目標功能編號 |
| 15:12 | **Extended Register Number** | 擴展暫存器編號 |
| 11:2 | **Register Number** | 暫存器編號 |
| 1:0 | **Reserved** | 保留 |

### **I/O Request**

| 位元 | 欄位名稱 | 描述 |
|------|----------|------|
| 31:2 | **Address[31:2]** | I/O 位址 |
| 1:0 | **Reserved** | 保留 |

### **Completion**

| 位元 | 欄位名稱 | 描述 |
|------|----------|------|
| 31:16 | **Completer ID** | 完成者 ID |
| 15:13 | **Completion Status** | 完成狀態 |
| 12 | **BCM** | Byte Count Modified |
| 11:0 | **Byte Count** | 位元組數量 |

#### **Completion Status**
| 值 | 狀態 | 描述 |
|----|------|------|
| 000 | **SC** | Successful Completion |
| 001 | **UR** | Unsupported Request |
| 010 | **CRS** | Configuration Request Retry Status |
| 100 | **CA** | Completer Abort |

---

## 📋 第四個 DWord (DW3) - 64位元位址或完成資訊

### **64位元位址 (4DW Header)**

| 位元 | 欄位名稱 | 描述 |
|------|----------|------|
| 31:2 | **Address[31:2]** | 低32位位址 |
| 1:0 | **Reserved** | 保留 |

### **Completion (如果有第四個 DWord)**

| 位元 | 欄位名稱 | 描述 |
|------|----------|------|
| 31:16 | **Reserved** | 保留 |
| 15:0 | **Lower Address** | 原始請求的低位址 |

---

## 🔍 特殊 TLP 類型

### **Message TLP**

| DW | 欄位 | 描述 |
|----|------|------|
| DW2 | **Message Code** | 訊息類型碼 |
| DW3 | **Message-specific** | 訊息特定資料 |

#### **常見 Message 類型**
| Code | Message 類型 | 描述 |
|------|-------------|------|
| 0x00 | **Assert_INTA** | 中斷 A 觸發 |
| 0x01 | **Assert_INTB** | 中斷 B 觸發 |
| 0x02 | **Assert_INTC** | 中斷 C 觸發 |
| 0x03 | **Assert_INTD** | 中斷 D 觸發 |
| 0x04 | **Deassert_INTA** | 中斷 A 取消 |
| 0x14 | **PM_Active_State_Nak** | 電源管理 |
| 0x18 | **PME_Turn_Off** | 電源管理事件 |

---

## 📊 TLP Header 格式總覽

### **3DW Header (Memory Request)**
```
DW0: [Fmt:3][Type:5][TC:3][Attr:4][TH:1][TD:1][EP:1][Attr:2][AT:2][Length:10]
DW1: [Requester ID:16][Tag:8][Last DW BE:4][1st DW BE:4]  
DW2: [Address[31:2]:30][Reserved:2]
```

### **4DW Header (64-bit Memory Request)**
```
DW0: [Fmt:3][Type:5][TC:3][Attr:4][TH:1][TD:1][EP:1][Attr:2][AT:2][Length:10]
DW1: [Requester ID:16][Tag:8][Last DW BE:4][1st DW BE:4]
DW2: [Address[63:32]:32]
DW3: [Address[31:2]:30][Reserved:2]
```

### **Completion Header**
```
DW0: [Fmt:3][Type:5][TC:3][Attr:4][TH:1][TD:1][EP:1][Attr:2][AT:2][Length:10]
DW1: [Completer ID:16][Completion Status:3][BCM:1][Byte Count:12]
DW2: [Requester ID:16][Tag:8][Reserved:1][Lower Address:7]
```

---

## 🎯 重要注意事項

### **位址對齊**
- Memory Request: 必須 DWord 對齊 (低2位為00)
- 實際位址 = Header中的位址 + Byte Enable 解析

### **Byte Enable**
- **1st DW BE**: 第一個 DWord 的 Byte Enable
- **Last DW BE**: 最後一個 DWord 的 Byte Enable
- 用於指示實際存取的位元組

### **路由**
- 根據 TLP 類型使用不同的路由機制
- Memory: 位址路由
- Configuration: ID 路由  
- Message: 廣播或特定路由

