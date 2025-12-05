# PCIe Configuration Space Layout

## Type 0 Header (端點裝置) - Standard Configuration Space (0x00-0xFF)

| Offset | Size | Field Name | Description | Access |
|--------|------|------------|-------------|---------|
| 0x00 | 2 bytes | Vendor ID | 廠商識別碼 | RO |
| 0x02 | 2 bytes | Device ID | 裝置識別碼 | RO |
| 0x04 | 2 bytes | Command | 命令暫存器 | R/W |
| 0x06 | 2 bytes | Status | 狀態暫存器 | RO/RW1C |
| 0x08 | 1 byte | Revision ID | 版本識別碼 | RO |
| 0x09 | 3 bytes | Class Code | 裝置類別代碼 | RO |
| 0x0C | 1 byte | Cache Line Size | 快取行大小 | R/W |
| 0x0D | 1 byte | Latency Timer | 延遲計時器 | R/W |
| 0x0E | 1 byte | Header Type | 標頭類型 | RO |
| 0x0F | 1 byte | BIST | 內建自我測試 | R/W |
| 0x10 | 4 bytes | BAR0 | 基底位址暫存器 0 | R/W |
| 0x14 | 4 bytes | BAR1 | 基底位址暫存器 1 | R/W |
| 0x18 | 4 bytes | BAR2 | 基底位址暫存器 2 | R/W |
| 0x1C | 4 bytes | BAR3 | 基底位址暫存器 3 | R/W |
| 0x20 | 4 bytes | BAR4 | 基底位址暫存器 4 | R/W |
| 0x24 | 4 bytes | BAR5 | 基底位址暫存器 5 | R/W |
| 0x28 | 4 bytes | Cardbus CIS Pointer | Cardbus CIS 指標 | RO |
| 0x2C | 2 bytes | Subsystem Vendor ID | 子系統廠商 ID | RO |
| 0x2E | 2 bytes | Subsystem ID | 子系統 ID | RO |
| 0x30 | 4 bytes | Expansion ROM Base Address | 擴展 ROM 基底位址 | R/W |
| 0x34 | 1 byte | Capabilities Pointer | 能力指標 | RO |
| 0x35-0x3B | 7 bytes | Reserved | 保留 | - |
| 0x3C | 1 byte | Interrupt Line | 中斷線 | R/W |
| 0x3D | 1 byte | Interrupt Pin | 中斷腳位 | RO |
| 0x3E | 1 byte | Min_Gnt | 最小授權 | RO |
| 0x3F | 1 byte | Max_Lat | 最大延遲 | RO |

## PCIe Capabilities Structure (能力結構)

| Capability | Capability ID | Description | Key Registers |
|------------|---------------|-------------|---------------|
| **PCIe Capability** | 0x10 | PCIe 基本能力 | Device Control/Status, Link Control/Status |
| **MSI Capability** | 0x05 | 訊息信號中斷 | Message Control, Message Address, Message Data |
| **MSI-X Capability** | 0x11 | 擴展訊息中斷 | Message Control, Table Offset, PBA Offset |
| **Power Management** | 0x01 | 電源管理 | PM Control/Status, Data Register |
| **AER Capability** | 0x0001 | 進階錯誤報告 | Uncorrectable/Correctable Error Status |
| **Virtual Channel** | 0x0002 | 虛擬通道 | Port VC Capability, VC Resource Control |
| **Device Serial Number** | 0x0003 | 裝置序號 | Serial Number (64-bit) |
| **Alternative Routing-ID** | 0x000E | 替代路由 ID | ARI Capability, ARI Control |

## Extended Configuration Space (0x100-0xFFF)
### 1. 存取方式
- **Legacy方式**: 只能存取前 256 bytes
- **ECAM (Enhanced Configuration Access Mechanism)**: 可存取完整 4KB 空間
- **Memory-mapped**: 透過 memory-mapped I/O 存取

### 2. Extended Capability 結構
與傳統 Capability 不同，Extended Capability 使用 32-bit header：

```
Offset +0: Extended Capability Header (32 bits)
  ├── Bits 0-15:  Extended Capability ID
  ├── Bits 16-19: Capability Version  
  └── Bits 20-31: Next Capability Offset (in DWORDs)

Offset +4: Capability-specific registers...
```

### 常見的 Extended Capabilities

#### 重要的 Extended Capability IDs:

**0x0001: Advanced Error Reporting (AER)**
- 提供詳細的錯誤報告和恢復機制
- 包含 Correctable/Uncorrectable Error Status/Mask 暫存器

**0x0002: Virtual Channel (VC)**
- 支援多個虛擬通道以實現 QoS
- 允許不同類型流量使用不同優先級

**0x0003: Device Serial Number**
- 提供設備的唯一序號識別

**0x0004: Power Budgeting**
- 電源管理和預算分配

**0x0005: Root Complex Link Declaration**
- Root Complex 的連結資訊

**0x000A: Link Declaration**
- 連結相關的擴展資訊

**0x000B: Internal RC Link Control**
- Root Complex 內部連結控制

**0x000D: Access Control Services (ACS)**
- 提供 I/O 虛擬化支援的存取控制

**0x000E: Alternative Routing-ID Interpretation (ARI)**
- 擴展設備識別能力

**0x0010: Single Root I/O Virtualization (SR-IOV)**
- 硬體虛擬化支援

**0x0013: Multicast**
- 多播功能支援

**0x0015: Resizable BAR**
- 動態調整 BAR 大小

**0x001E: Data Link Feature**
- 資料連結層的擴展功能

**0x0023: Designated Vendor-Specific Extended Capability (DVSEC)**
- 廠商特定的擴展功能

### Extended Capability 解析範例

```c
// 解析 Extended Capabilities
uint32_t ext_cap_offset = 0x100;  // 第一個 Extended Capability

while (ext_cap_offset != 0) {
    uint32_t ext_cap_header = pci_read_config_dword(ext_cap_offset);
    
    uint16_t ext_cap_id = ext_cap_header & 0xFFFF;
    uint8_t  cap_version = (ext_cap_header >> 16) & 0xF;
    uint16_t next_offset = (ext_cap_header >> 20) & 0xFFF;
    
    switch (ext_cap_id) {
        case 0x0001:  // AER
            parse_aer_capability(ext_cap_offset);
            break;
        case 0x0010:  // SR-IOV
            parse_sriov_capability(ext_cap_offset);
            break;
        // 其他 extended capabilities...
    }
    
    ext_cap_offset = next_offset ? (next_offset << 2) : 0;
}
```

### 存取方法差異

#### 1. 傳統 Configuration Space (0x00-0xFF)
```c
// 使用 CF8h/CFCh ports (x86)
outl(config_address, 0xCF8);
value = inl(0xCFC);
```

#### 2. Extended Configuration Space (0x100-0xFFF)
```c
// 必須使用 Memory-mapped (ECAM)
volatile uint32_t *ecam_base = map_ecam_space();
uint32_t offset = (bus << 20) | (device << 15) | (function << 12) | reg;
value = ecam_base[offset >> 2];
```

## Command Register (0x04) 位元定義

| Bit | Field Name | Description |
|-----|------------|-------------|
| 0 | I/O Space Enable | 啟用 I/O 空間存取 |
| 1 | Memory Space Enable | 啟用記憶體空間存取 |
| 2 | Bus Master Enable | 啟用匯流排主控 |
| 3 | Special Cycle Enable | 啟用特殊週期 |
| 4 | Memory Write and Invalidate | 記憶體寫入並無效化 |
| 5 | VGA Palette Snoop | VGA 調色盤監聽 |
| 6 | Parity Error Response | 同位檢查錯誤回應 |
| 8 | SERR# Enable | 啟用 SERR# 信號 |
| 9 | Fast Back-to-Back Enable | 啟用快速背對背 |
| 10 | Interrupt Disable | 停用中斷 |

## Status Register (0x06) 位元定義

| Bit | Field Name | Description |
|-----|------------|-------------|
| 3 | Interrupt Status | 中斷狀態 |
| 4 | Capabilities List | 能力清單存在 |
| 5 | 66MHz Capable | 支援 66MHz |
| 7 | Fast Back-to-Back Capable | 支援快速背對背 |
| 8 | Master Data Parity Error | 主控資料同位檢查錯誤 |
| 11 | Signaled Target Abort | 信號目標中止 |
| 12 | Received Target Abort | 接收目標中止 |
| 13 | Received Master Abort | 接收主控中止 |
| 14 | Signaled System Error | 信號系統錯誤 |
| 15 | Detected Parity Error | 偵測到同位檢查錯誤 |

## BAR (Base Address Register) 格式

### Memory BAR
| Bits | Field | Description |
|------|-------|-------------|
| 0 | Type | 0 = Memory Space |
| 2:1 | Memory Type | 00=32-bit, 10=64-bit |
| 3 | Prefetchable | 1=Prefetchable |
| 31:4 | Base Address | 記憶體基底位址 |

### I/O BAR
| Bits | Field | Description |
|------|-------|-------------|
| 0 | Type | 1 = I/O Space |
| 1 | Reserved | 保留 |
| 31:2 | Base Address | I/O 基底位址 |


## Type 1 Header - Standard Configuration Space (0x00-0xFF)

| Offset | Size | Field Name | Description | Access |
|--------|------|------------|-------------|---------|
| 0x00 | 2 bytes | Vendor ID | 廠商識別碼 | RO |
| 0x02 | 2 bytes | Device ID | 裝置識別碼 | RO |
| 0x04 | 2 bytes | Command | 命令暫存器 | R/W |
| 0x06 | 2 bytes | Status | 狀態暫存器 | RO/RW1C |
| 0x08 | 1 byte | Revision ID | 版本識別碼 | RO |
| 0x09 | 3 bytes | Class Code | 裝置類別代碼 (060400h for PCI-PCI Bridge) | RO |
| 0x0C | 1 byte | Cache Line Size | 快取行大小 | R/W |
| 0x0D | 1 byte | Latency Timer | 延遲計時器 | R/W |
| 0x0E | 1 byte | Header Type | 標頭類型 (01h for Type 1) | RO |
| 0x0F | 1 byte | BIST | 內建自我測試 | R/W |
| 0x10 | 4 bytes | BAR0 | 基底位址暫存器 0 | R/W |
| 0x14 | 4 bytes | BAR1 | 基底位址暫存器 1 | R/W |
| 0x18 | 1 byte | Primary Bus Number | 主要匯流排編號 | R/W |
| 0x19 | 1 byte | Secondary Bus Number | 次要匯流排編號 | R/W |
| 0x1A | 1 byte | Subordinate Bus Number | 從屬匯流排編號 | R/W |
| 0x1B | 1 byte | Secondary Latency Timer | 次要延遲計時器 | R/W |
| 0x1C | 1 byte | I/O Base | I/O 基底位址 | R/W |
| 0x1D | 1 byte | I/O Limit | I/O 限制位址 | R/W |
| 0x1E | 2 bytes | Secondary Status | 次要狀態暫存器 | RO/RW1C |
| 0x20 | 2 bytes | Memory Base | 記憶體基底位址 | R/W |
| 0x22 | 2 bytes | Memory Limit | 記憶體限制位址 | R/W |
| 0x24 | 2 bytes | Prefetchable Memory Base | 可預取記憶體基底位址 | R/W |
| 0x26 | 2 bytes | Prefetchable Memory Limit | 可預取記憶體限制位址 | R/W |
| 0x28 | 4 bytes | Prefetchable Base Upper 32 Bits | 可預取基底位址高 32 位元 | R/W |
| 0x2C | 4 bytes | Prefetchable Limit Upper 32 Bits | 可預取限制位址高 32 位元 | R/W |
| 0x30 | 2 bytes | I/O Base Upper 16 Bits | I/O 基底位址高 16 位元 | R/W |
| 0x32 | 2 bytes | I/O Limit Upper 16 Bits | I/O 限制位址高 16 位元 | R/W |
| 0x34 | 1 byte | Capability Pointer | 能力指標 | RO |
| 0x35-0x37 | 3 bytes | Reserved | 保留 | - |
| 0x38 | 4 bytes | Expansion ROM Base Address | 擴展 ROM 基底位址 | R/W |
| 0x3C | 1 byte | Interrupt Line | 中斷線 | R/W |
| 0x3D | 1 byte | Interrupt Pin | 中斷腳位 | RO |
| 0x3E | 2 bytes | Bridge Control | 橋接器控制暫存器 | R/W |

## Type 0 vs Type 1 Header 主要差異

| Offset | Type 0 (端點裝置) | Type 1 (橋接器) |
|--------|------------------|-----------------|
| 0x10-0x14 | BAR0, BAR1 | BAR0, BAR1 |
| 0x18-0x1B | BAR2, BAR3 | Primary Bus#, Secondary Bus#, Subordinate Bus#, Secondary Latency Timer |
| 0x1C-0x1F | BAR4, BAR5 | I/O Base, I/O Limit, Secondary Status |
| 0x20-0x27 | CardBus CIS Pointer, Subsystem Vendor ID, Subsystem ID | Memory Base/Limit, Prefetchable Memory Base/Limit |
| 0x28-0x2F | Expansion ROM Base | Prefetchable Base/Limit Upper 32 bits |
| 0x30-0x37 | - | I/O Base/Limit Upper 16 bits |
| 0x3E-0x3F | Min_Gnt, Max_Lat | Bridge Control |

## Bridge Control Register (0x3E) 位元定義

| Bit | Field Name | Description |
|-----|------------|-------------|
| 0 | Parity Error Response Enable | 啟用同位檢查錯誤回應 |
| 1 | SERR# Enable | 啟用 SERR# 信號 |
| 2 | ISA Enable | 啟用 ISA 模式 |
| 3 | VGA Enable | 啟用 VGA 調色盤監聽 |
| 4 | VGA 16-bit decode | VGA 16 位元解碼 |
| 5 | Master Abort Mode | 主控中止模式 |
| 6 | Secondary Bus Reset | 次要匯流排重置 |
| 7 | Fast Back-to-Back Enable | 啟用快速背對背 |
| 8 | Primary Discard Timer | 主要丟棄計時器 |
| 9 | Secondary Discard Timer | 次要丟棄計時器 |
| 10 | Discard Timer Status | 丟棄計時器狀態 |
| 11 | Discard Timer SERR# Enable | 丟棄計時器 SERR# 啟用 |

## 匯流排編號說明

| Field | Description | 用途 |
|-------|-------------|------|
| **Primary Bus Number** | 橋接器所在的匯流排編號 | 橋接器的上游匯流排 |
| **Secondary Bus Number** | 橋接器直接連接的下游匯流排編號 | 橋接器的直接下游匯流排 |
| **Subordinate Bus Number** | 橋接器下游所有匯流排的最大編號 | 定義橋接器管轄的匯流排範圍 |

## 記憶體視窗配置

### Memory Base/Limit (0x20-0x22)
| Bits | Field | Description |
|------|-------|-------------|
| 15:4 | Memory Base Address | 記憶體基底位址 [31:20] |
| 3:0 | Reserved | 保留 (0000b) |

### Prefetchable Memory Base/Limit (0x24-0x26)
| Bits | Field | Description |
|------|-------|-------------|
| 15:4 | Prefetchable Memory Base Address | 可預取記憶體基底位址 [31:20] |
| 3:1 | Reserved | 保留 |
| 0 | 64-bit Indicator | 1=支援 64 位元位址 |

### I/O Base/Limit (0x1C-0x1D)
| Bits | Field | Description |
|------|-------|-------------|
| 7:4 | I/O Base Address | I/O 基底位址 [15:12] |
| 3:1 | Reserved | 保留 |
| 0 | I/O Address Capability | 0=16-bit, 1=32-bit |

## PCIe-to-PCI Bridge 特殊考量

| 功能 | Description |
|------|-------------|
| **Transaction Ordering** | 確保 PCIe 和 PCI 之間的交易順序 |
| **Posted Write Buffer** | 緩衝已發布的寫入交易 |
| **Split Transaction** | 處理 PCI 分割交易 |
| **Clock Domain Crossing** | 處理不同時脈域間的信號 |
| **Protocol Translation** | PCIe TLP 到 PCI 交易的轉換 |

## Root Complex 特殊暫存器

| Capability | Description | 位置 |
|------------|-------------|------|
| **Root Capabilities** | RC 特有能力 | Extended Config Space |
| **Root Control** | RC 控制設定 | Extended Config Space |
| **Root Status** | RC 狀態資訊 | Extended Config Space |
| **Device Capabilities 2** | 進階裝置能力 | Extended Config Space |
| **Device Control 2** | 進階裝置控制 | Extended Config Space |

## Switch 特殊考量

| Port Type | Description | 功能 |
|-----------|-------------|------|
| **Upstream Port** | 連接到上游的埠 | 單一上游連接 |
| **Downstream Port** | 連接到下游的埠 | 多個下游連接 |
| **Virtual PCI-PCI Bridge** | 虛擬 PCI-PCI 橋接器 | 每個下游埠一個 |
| **Internal Routing** | 內部路由機制 | 封包轉發和仲裁 |

