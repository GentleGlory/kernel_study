# PCIe ASPM L0s and L1
PCIe ASPM (Active State Power Management) 的 L0s 和 L1 是兩種不同深度的省電狀態，主要差別如下：

## L0s (L-zero-s) - 快速省電狀態

### 特性
- **最淺層的省電狀態**
- **單向性**: Upstream 和 Downstream 可以獨立進入 L0s
- **極快的恢復時間**: 通常 < 7μs
- **部分電路保持活躍**

### 進入條件
```
- Link 沒有 TLP (Transaction Layer Packet) 傳輸
- 可以由 Upstream 或 Downstream 單獨啟動
- 不需要雙方協商
```

### 電路狀態
- **PHY Layer**: 部分關閉，但保持同步
- **時鐘**: 參考時鐘保持運作
- **接收器**: 保持活躍狀態
- **發送器**: 進入低功耗模式

### 恢復機制
```
L0s → L0: 
- 發送 FTS (Fast Training Sequence)
- 重新建立 bit/symbol lock
- 恢復時間: 1-7μs
```

## L1 - 深度省電狀態

### 特性
- **較深層的省電狀態**
- **雙向性**: 需要 Link 兩端都同意才能進入
- **較長的恢復時間**: 通常 < 64μs
- **更多電路關閉**

### 進入條件
```
- 雙方都沒有 pending TLPs
- 需要通過 PM_Enter_L1 DLLP 協商
- 雙向握手確認
```

### 電路狀態
- **PHY Layer**: 大部分關閉
- **時鐘**: 可以關閉更多時鐘域
- **PLL**: 可能進入低功耗模式
- **更多 analog circuits**: 關閉

### 恢復機制
```
L1 → L0:
- 更複雜的恢復程序
- 重新初始化 PHY
- 重新建立 training
- 恢復時間: 8-64μs
```

## 詳細比較表

| 特性 | L0s | L1 |
|------|-----|-----|
| **省電深度** | 淺 | 深 |
| **恢復時間** | 1-7μs | 8-64μs |
| **進入方式** | 單向，無需協商 | 雙向協商 |
| **時鐘狀態** | 參考時鐘保持 | 更多時鐘可關閉 |
| **PHY狀態** | 部分活躍 | 大部分關閉 |
| **使用場景** | 短暫空閒 | 較長空閒 |
| **功耗節省** | 中等 | 較高 |

## Configuration Register 控制

在 PCIe Capability 的 Link Control Register (偏移 +16) 中：

```c
// Link Control Register bits
#define PCIE_LNKCTL_ASPM_L0S    0x01    // Enable L0s
#define PCIE_LNKCTL_ASPM_L1     0x02    // Enable L1
#define PCIE_LNKCTL_ASPM_BOTH   0x03    // Enable both L0s and L1

// 讀取當前 ASPM 設定
uint16_t link_ctrl = pci_read_config_word(cap_offset + 16);
bool l0s_enabled = link_ctrl & PCIE_LNKCTL_ASPM_L0S;
bool l1_enabled = link_ctrl & PCIE_LNKCTL_ASPM_L1;
```

## 時序圖範例

### L0s 進入/退出
```
Device A    Device B
   |           |
   |-- Idle -->|  (無 TLP 傳輸)
   |           |
   |<-- L0s ---|  (Device B 進入 L0s)
   |           |
   |-- TLP  -->|  (Device A 要傳輸)
   |-- FTS  -->|  (Fast Training Sequence)
   |<-- Ack ---|  (恢復到 L0)
   |-- Data -->|
```

### L1 進入/退出
```
Device A         Device B
   |               |
   |-- PM_Enter_L1 -->|  (請求進入 L1)
   |<-- PM_Ack_L1  ---|  (確認進入 L1)
   |                  |
   ====== L1 State ======
   |                  |
   |<-- Wake Signal --|  (某方要恢復)
   |-- Training    -->|  (重新 training)
   |<-- Training   ---|
   |-- Data       -->|  (恢復到 L0)
```

## 實際應用考量

### L0s 適用場景
- **CPU-GPU** 等高頻寬需求但有短暫空閒
- **網路卡** 在封包間隙
- **儲存控制器** 在命令間隙

### L1 適用場景
- **移動設備** 需要最大化電池壽命
- **伺服器** 在低負載時段
- **待機模式** 的週邊設備

### 設定建議
```c
// 一般桌面/伺服器環境
link_ctrl |= PCIE_LNKCTL_ASPM_L0S;  // 啟用 L0s

// 移動/省電環境
link_ctrl |= PCIE_LNKCTL_ASPM_BOTH; // 同時啟用 L0s 和 L1
```

ASPM 的選擇需要在功耗節省和效能之間找到平衡點，L0s 提供快速的省電能力，而 L1 則在更長的空閒時間提供更深度的省電效果。