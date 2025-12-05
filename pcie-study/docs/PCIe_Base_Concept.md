# PCI/PCIe 核心概念與設備訪問總結

本文檔從軟體開發的角度，濃縮整理了 PCI 與 PCIe 的核心概念，包括地址空間、設備訪問、數據傳輸層次、配置過程和路由方式。

## 1\. 核心思想：地址空間映射

從軟體角度看，最方便的設備訪問方式是像讀寫記憶體一樣，通過位址直接操作。PCI/PCIe 的核心設計理念正是基於此：**將 CPU 的地址空間映射到 PCI/PCIe 的地址空間**。

這個過程由 **PCI/PCIe 主機控制器 (Host Bridge 或 Root Complex)** 完成。開發者操作的是 CPU 位址，硬體會自動將其轉換為對應的 PCI/PCIe 位址，從而訪問目標設備。

## 2\. 設備的分類與識別

PCI/PCIe 設備可簡單分為兩類：

  * **Endpoint (或稱 Agent)**：終端功能設備，如網卡、顯卡，是 PCI/PCIe 樹的最末端。
  * **Bridge (橋接器)**：用於擴展匯流排，連接更多的設備。所有系統都至少有一個根橋 (Root Bridge/Root Complex)。

系統通過讀取設備配置空間中的 `Header Type` 欄位來識別設備類型。

  * `0x00`: Endpoint (Type 0 Header)
  * `0x01`: PCI/PCIe Bridge (Type 1 Header)

## 3\. 設備配置過程 (Enumeration)

系統啟動時，需要對所有 PCI/PCIe 設備進行掃描和配置，這個過程稱為 **枚舉 (Enumeration)**。核心是使用 **深度優先** 的演算法，遍歷整個 PCI/PCIe 拓撲樹。

#### 3.1 配置流程概覽

1.  **發現設備**：軟體從 `Bus 0, Device 0, Function 0` (BDF) 開始，嘗試讀取設備的 Vendor ID，以判斷設備是否存在。
2.  **識別與配置**：
      * **如果是 Endpoint**：讀取其 BAR (Base Address Registers) 來了解所需的地址空間大小，然後分配一段 PCI/PCIe 地址空間，並將基地址寫回 BAR。
      * **如果是橋接器**：為其分配匯流排號碼，寫入其 `Primary Bus Number`、`Secondary Bus Number` 和 `Subordinate Bus Number` 欄位。`Secondary Bus Number` 是橋下游新匯流排的編號，`Subordinate Bus Number` 則記錄了下游可達的最大匯流排號。
3.  **深度優先掃描**：如果發現一個橋接器，軟體會立即進入該橋所連接的 `Secondary Bus` 繼續掃描，完成下游所有設備的配置後，再回溯到上一層繼續掃描同級設備。
4.  **更新橋資訊**：當一個橋下游的所有匯流排都掃描完畢後，軟體會更新該橋的 `Subordinate Bus Number` 為實際掃描到的最大匯流排號。

#### 3.2 配置請求 (Configuration Request)

  * **Type 0 配置請求**：用於訪問 **與橋直接相連** 的設備。當目標設備的 Bus 號與當前匯流排號相同時使用。
  * **Type 1 配置請求**：用於訪問 **橋下游** 的設備。當目標設備的 Bus 號大於當前匯流排號時使用。此請求會被橋轉發，並在到達目標設備所在的匯流排時，由最後一級橋轉換為 Type 0 請求。

## 4\. PCIe 封包傳輸層次與路由

PCIe 是基於封包 (Packet) 的序列匯流排，其數據傳輸遵循嚴格的分層結構。

#### 4.1 數據封包的分層結構

數據在發送時，會從上到下逐層封裝：

  * **事務層 (Transaction Layer)**：這是軟體主要互動的層級。它負責建立 **事務層封包 (Transaction Layer Packet, TLP)**。TLP 定義了交易的具體內容，例如記憶體讀寫、IO 讀寫或配置讀寫。
  
  * **數據鏈路層 (Data Link Layer)**：此層確保數據傳輸的可靠性。它接收來自事務層的 TLP，並在其前後加上前綴和後綴（如序號和校驗碼），形成 **數據鏈路層封包 (Data Link Layer Packet, DLLP)**。此層也負責錯誤檢測和重傳機制。
  * **物理層 (Physical Layer)**：此層負責實際的數據傳輸。它接收 DLLP，再次加上表示封包起始和結束的幀標記，形成 **物理層封包 (Physical Packet)**。最終，這些封包被轉換為電子訊號，通過差動訊號線 (Lane) 發送出去。

除了數據封裝，數據鏈路層和物理層也有自己專屬的封包，用於鏈路管理和流量控制。

#### 4.2 TLP 路由方式

TLP 封包的頭部定義了這筆交易的類型、路由方式和目標。PCIe 有三種路由方式：

  * **基於 ID 的路由 (ID-Based Routing)**

      * **用途**：主要用於 **設備配置** 過程 (配置讀/寫)，以及 Non-Posted 事務的 **完成封包 (Completion Packet)** 回應。
      * **原理**：使用 `<Bus, Device, Function>` (BDF) 作為目標 ID。橋接器根據其配置空間中的 `Secondary` 和 `Subordinate` 匯流排號來決定是否將封包轉發到下游。

  * **基於地址的路由 (Address-Based Routing)**

      * **用途**：用於配置完成後的常規數據交換，如 **內存讀/寫** 和 **IO 讀/寫**。
      * **原理**：TLP 封包中包含一個 32 位元或 64 位元的目標地址。
          * **Endpoint**：監聽匯流排，如果 TLP 中的地址落在自己 BAR 所設定的範圍內，就接收該封包。
          * **橋接器**：根據 TLP 中的地址是否落在其下游設備的地址範圍內（記錄在 `Memory Base/Limit` 等暫存器中），決定是否轉發封包。

  * **隱式路由 (Implicit Routing)**

      * **用途**：主要用於 **消息 (Message)** 類型的 TLP，如中斷、電源管理事件或廣播。
      * **原理**：路由目標不是一個具體的地址或 ID，而是隱含在消息的路由子欄位中，例如「路由到 Root Complex」、「廣播到下游所有設備」等。


# 5\.PCIe Device Tree Ranges 解析

## Ranges 屬性格式

PCIe 的 `ranges` 屬性格式為：
```
<child_address_cells> <parent_address_cells> <size_cells>
```

根據 Device Tree 設定：
- `#address-cells = <3>` (child)
- `#size-cells = <2>` (child)  
- Parent 的 address-cells = 2 (通常是 SoC 的設定)

每個 range 條目格式：
```
<PCI_address_space> <PCI_address_high> <PCI_address_low> <CPU_address_high> <CPU_address_low> <size_high> <size_low>
```

---

## PCI Address Space 編碼解析

### **PCI Address Space 第一個 DWord 格式**

```
位元 31-24: ss000000
s = Address Space 類型
```

| Address Space Code | 類型 | 描述 |
|-------------------|------|------|
| **0x00000800** | Configuration Space | 配置空間 |
| **0x81000000** | I/O Space | I/O 空間 |
| **0x82000000** | 32-bit Memory (Non-Prefetchable) | 32位元記憶體（不可預取）|
| **0x83000000** | 64-bit Memory (Prefetchable) | 64位元記憶體（可預取）|

### **詳細位元編碼**

| 位元 | 欄位 | 說明 |
|------|------|------|
| 31 | **n** | Relocatable (0=Yes, 1=No) |
| 30 | **p** | Prefetchable (0=No, 1=Yes) |
| 29 | **t** | Aliased (0=No, 1=Yes) |
| 28:26 | **ss** | Space Type (00=Config, 01=I/O, 10=Mem32, 11=Mem64) |
| 25:24 | **bb** | Bus Number |
| 23:16 | **dd** | Device Number |
| 15:8 | **ff** | Function Number |
| 7:0 | **rr** | Register Number |