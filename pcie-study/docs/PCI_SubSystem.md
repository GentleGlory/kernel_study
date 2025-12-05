# pci_host_bridge
## 代碼位置
```
include/linux/pci.h
```
## 用途 
代表 PCI 主機橋接器（ Host Bridge ）
- 這是系統層級的結構，不是單一 PCI 設備
- 負責連接 CPU / 內存系統與 PCI 匯流排
- 管理整個 PCI 匯流排樹狀結構的根部
- 每個 PCI 主機控制器對應一個 pci_host_bridge

## 欄位
### struct device dev
- 對應的設備結構。
- dev.parent 為 `devm_pci_alloc_host_bridge` 傳入的 dev。 最常見的是 PCI 控制器的 platform_dev->dev; 
	```
	  Platform Device (PCIe 控制器)
      └── PCI Host Bridge
          └── PCI Root Bus
              └── PCI Devices
	```
### struct pci_bus *bus
- 指向根匯流排 （ root bus ）

### struct pci_ops *ops
- PCI 操作函數集

### struct pci_ops *child_ops
- 子匯流排的操作函數集。 需要 ATU 進行位置轉換
	```
	PCIe Controller (Bus 0)
	├── Root Port (Bus 0, Device 0) ← 使用 ops
	└── PCI-to-PCI Bridge
      └── Secondary Bus (Bus 1) ← 使用 child_ops
          ├── Device A (Bus 1, Device 0)
          └── Device B (Bus 1, Device 1)
	```

### struct list_head windows
- I/O 和記憶體窗口資源

### int domain_nr 
- PCI 網域編號


## 關係圖
```
  PCI Host Bridge (pci_host_bridge)
      └── Root Bus (pci_bus)
          ├── PCI Device 1 (pci_dev)
          ├── PCI Device 2 (pci_dev)
          └── PCI-to-PCI Bridge (pci_dev)
              └── Secondary Bus (pci_bus)
                  ├── PCI Device 3 (pci_dev)
                  └── PCI Device 4 (pci_dev)
```

# pci_dev


# pci_bus

# PCIe Switch 
- 一個 PCIe Switch 會在 Linux PCI 子系統呈現為多個 pci_dev 結構：
	1. Upstream Port 的 pci_dev
		- 類型: PCI_EXP_TYPE_UPSTREAM (0x5)
		- 位置: 連接到 pci_host_bridge 的 root bus 上
		- 作用: Switch 的入口埠
		- 數量: 每個 Switch 只有 1 個
	
	2. Downstream Port 的 pci_dev
		- 類型: PCI_EXP_TYPE_DOWNSTREAM (0x6)
		- 位置: 位於 Switch 內部的 secondary bus 上
		- 作用: Switch 的出口埠，各自連接不同的端點設備
		- 數量: 每個 Switch 可有多個 (通常 2-8 個)

- 實際的 PCI 拓撲枚舉
```
  pci 0000:00:00.0: Root Port (pci_host_bridge 創建的根端口)
  ├─pci 0000:01:00.0: Upstream Port (Switch 入口) ← pci_dev #1
  │ └─pci 0000:02:00.0: Downstream Port (Switch 出口1) ← pci_dev #2
  │   └─pci 0000:03:00.0: PCIe Device (實際設備)
  ├─pci 0000:02:01.0: Downstream Port (Switch 出口2) ← pci_dev #3
  │ └─pci 0000:04:00.0: PCIe Device (實際設備)
  └─pci 0000:02:02.0: Downstream Port (Switch 出口3) ← pci_dev #4
    └─pci 0000:05:00.0: PCIe Device (實際設備)
```
- 重要觀念
PCIe Switch 不是單一 pci_dev，而是：
	- 1 個 Upstream Port pci_dev (連接上級)
	- N 個 Downstream Port pci_dev (連接下級設備)
	- 1 個虛擬 PCI-to-PCI Bridge (內部路由邏輯)

# pci_scan_root_bus_bridge
## 主要作用 
`pci_scan_root_bus_bridge` 是 PCI 設備枚舉的核心函數，負責
1. 註冊 Host Bridge 到系統
2. 掃描所有 PCI 設備
3. 建立 PCI 拓樸結構
4. 建立設備物件

## 執行流程分析
1. 尋找匯流排號資源
```c
resource_list_for_each_entry(window, &bridge->windows)
	if (window->res->flags & IORESOURCE_BUS) {
		bridge->busnr = window->res->start;
		found = true;
		break;
	}
```
- 在 bridge->windows 中尋找 IORESOURCE_BUS 類型的資源
- 設定跟匯流排號 bridge->busnr
- 通常來自設備樹的 bus-range 屬性

2. 註冊 Host Bridge
```c
ret = pci_register_host_bridge(bridge);
if (ret < 0)
	return ret;
```
- 建立根匯流排 (struct pci_bus)
- 設定匯流排操作函數 (bridge->ops, bridge->child_ops)
- 註冊到 Linux 設備模型
- 初始化資源管理

3. 獲取匯流排資訊
```c
b = bridge->bus;        // 根匯流排
bus = bridge->busnr;    // 根匯流排號
```
4. 處理匯流排號範圍
```c
if (!found) {
	dev_info(&b->dev,
		"No busn resource found for root bus, will use [bus %02x-ff]\n",
		bus);
	pci_bus_insert_busn_res(b, bus, 255);
}
```
如果沒找到 BUS 資源：
- 使用預設範圍：[bus, 255]
- 插入匯流排號資源到資源樹

5. 掃描子匯流排
```c
max = pci_scan_child_bus(b);
```
這是設備枚舉的核心，會：
- 掃描每個設備位置 (0-31)
- 讀取 Vendor/Device ID
- 建立 pci_dev 結構體
- 掃描 PCIe Bridge 和下游匯流排
- 遞迴掃描子匯流排

6. 更新匯流排範圍
```c
if (!found)
	pci_bus_update_busn_res_end(b, max);
```
更新匯流排資源的結束號為實際掃描到的最大匯流排號。

## pci_scan_root_bus_bridge 調用層次

```
pci_scan_root_bus_bridge()
    ├── pci_register_host_bridge()          // 註冊 host bridge
    │   ├── device_add()                    // 將 bridge 加入 device model
    │   ├── pci_set_bus_of_node()          // 設定 Device Tree 節點
    │   └── pci_set_bus_msi_domain()       // 設定 MSI domain
    └── pci_scan_child_bus()                // 掃描 root bus 的子裝置
        └── pci_scan_child_bus_extend(bus, 0) // available_buses = 0
            └── pci_scan_slot()             // 掃描每個 slot (devfn 0, 8, 16...)
                └── pci_scan_single_device()
                    ├── pci_get_slot()     // 檢查是否已存在
                    ├── pci_scan_device()  // 掃描新裝置
                    │   ├── pci_bus_read_dev_vendor_id() // 讀取 Vendor/Device ID → map_bus
                    │   ├── pci_alloc_dev()            // 分配 pci_dev 結構
                    │   └── pci_setup_device()         // 設定裝置 → 更多 map_bus 呼叫
                    └── pci_device_add()    // 將裝置加入 bus
```
# PCI 深度優先搜尋
```
  pci_scan_root_bus_bridge()
    └── pci_scan_child_bus_extend(root_bus, 0)

        1. 【第一階段：掃描當前 bus 上的所有裝置】
        ├── for (devfn = 0; devfn < 256; devfn += 8)
        │   └── pci_scan_slot(bus, devfn)        // 掃描 slot 0, 8, 16, 24...
        │       └── pci_scan_single_device()
        │           ├── pci_scan_device()        // 讀取 Vendor/Device ID
        │           └── pci_device_add()         // 加入 bus

        2. 【第二階段：掃描已配置的 bridge (pass=0)】
        ├── for_each_pci_bridge(dev, bus)
        │   └── pci_scan_bridge_extend(bus, dev, max, 0, 0)
        │       └── if (child_bus_exists)
        │           └── pci_scan_child_bus_extend(child_bus, 0)  // 遞迴！

        3. 【第三階段：重新配置 bridge (pass=1)】
        └── for_each_pci_bridge(dev, bus)
            └── pci_scan_bridge_extend(bus, dev, cmax, buses, 1)
                └── if (need_rescan)
                    └── pci_scan_child_bus_extend(child_bus, available_buses)  // 遞迴！
```
# PCI BAR 配置
PCI BAR 配置是分成 兩個階段 進行的：

1. BAR 偵測與大小計算（掃描時期）
位置：drivers/pci/probe.c:1902 在 pci_setup_device() 中 pci_read_bases(dev, 6, PCI_ROM_ADDRESS);  // 讀取 6 個 BAR + ROM BAR
```
流程：
  pci_scan_device()
    └── pci_setup_device()
        └── pci_read_bases(dev, 6, PCI_ROM_ADDRESS)  // drivers/pci/probe.c:322
            └── for (pos = 0; pos < 6; pos++)
                └── __pci_read_base(dev, pci_bar_unknown, res, reg)
                    ├── 寫入 0xFFFFFFFF 到 BAR 暫存器
                    ├── 讀回值來計算 BAR 大小
                    ├── 恢復原始 BAR 值
                    └── 設定 resource 結構（但不分配實際地址）
```
此階段僅：
  - 偵測 BAR 類型（Memory/IO、32-bit/64-bit、Prefetchable）
  - 計算 BAR 大小
  - 設定 dev->resource[i] 的 flags 和 size
  - 不分配實際的記憶體位址

2. BAR 位址分配（掃描完成後）
位置：drivers/pci/probe.c:3078 在 pci_host_probe() 中 pci_bus_assign_resources(bus);
```
  流程：
  pci_host_probe()
    └── pci_scan_root_bus_bridge()     // 完成所有裝置掃描
    └── pci_bus_assign_resources(bus)  // drivers/pci/setup-bus.c:1413
        └── __pci_bus_assign_resources(bus, NULL, NULL)
            └── for_each_pci_dev_on_bus(dev, bus)
                └── pci_assign_resource(dev, i)  // drivers/pci/setup-res.c:325
                    └── _pci_assign_resource(dev, resno, size, align)
                        └── __pci_assign_resource(bus, dev, resno, size, align)
                            ├── 在 host bridge windows 中尋找合適空間
                            ├── 分配實際記憶體/IO 位址
                            └── 寫入位址到裝置的 BAR 暫存器
```
