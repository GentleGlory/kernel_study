# 參數用途
* perst_inactive_ms: PERST# (PCIe Reset) 信號的無效時間延遲參數。
* s2r_perst_inactive_ms: 專門用於 suspend-to-resume 場景。
* prsnt_gpio: 主要是用來偵測 PCIe 是否存在的 GPIO 。
* bifurcation: 允許將一個多 lane 的 PCIe 控制器分割成多個獨立的較窄 lane 控制器使用。 
	使用場景:
	- 當需要連接兩個獨立的單 lane PCIe 設備時
    - 而不是一個雙 lane 設備時
    - 可以提供更多的 PCIe 連接埠
* supports-clkreq: `supports-clkreq = true` 允許系統在合適的時候移除/關閉 PCIe
參考時鐘，以實現更好的功耗優化。
* skip-scan-in-resume: 為跳過 Resume 時設備掃描的控制參數，主要針對 WiFi 等設備的電源管理優化。
* is_lpbk and is_signal_test: 功能主要用於硬體測試和驗證，一般產品使用中不會啟用，屬於工程測試和開發階段的專用功能。
* comp_prst: 這個功能主要用於 PCIe 硬體驗證和認證測試，確保硬體設計符合 PCIe 規範要求。
	- 和 lpbk-master 類似，都會設定 is_signal_test = true
  	- 都屬於 PCIe 測試模式，不用於正常系統運作
  	- compliance 模式專注於電氣特性測試，loopback 模式專注於數據路徑測試

* n_fts: 用來配置 N_FTS (Number of Fast Training Sequences) 參數，這是 PCIe 鏈路訓練和省電機制的重要設定。N_FTS 是指從 L0s 省電狀態恢復到 L0 正常狀態時需要的 快速訓練序列 (Fast Training Sequence) 數量。較小的 N_FTS 值 → 更快的 L0s 恢復 → 更好的性能。較大的 N_FTS 值 → 更慢的 L0s 恢復 → 更可靠的訓練。
	- N_FTS 太小：可能導致鏈路訓練失敗
	- N_FTS 太大：增加不必要的恢復延遲



# Rockchip PCIe probe 流程
## rk_pcie_probe 
`rk_pcie_probe` 函數有兩種執行模式：
- 執行緒模式： 當 CONFIG_PCIE_RK_THREAD_INIT 啟用時，會使用執行緒來做初始化。
- 直接模式： 直接呼叫 `rk_pcie_really_probe` 初始化。

## rk_pcie_really_probe
1. 資源初始化：
```c
match = of_match_device(rk_pcie_of_match, dev);
data = (struct rk_pcie_of_data *)match->data;
rk_pcie = devm_kzalloc(dev, sizeof(*rk_pcie), GFP_KERNEL);
pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
```
- 匹配設備樹節點
- 分配 rk_pcie 和 dw_pcie 結構體記憶體

2. 變數賦值
```c
rk_pcie->pci = pci;
rk_pcie->msi_vector_num = data ? data->msi_vector_num : 0;
rk_pcie->intx = 0xffffffff;
pci->dev = dev;
pci->ops = &dw_pcie_ops;
platform_set_drvdata(pdev, rk_pcie);
```
- 關聯結構體
- 設定 MSI 中斷向量數量
- 設定操作函數
- 儲存驅動程式私有資料

3. 韌體資源取得
```c
ret = rk_pcie_resource_get(pdev, rk_pcie);
```
- 解析設備樹配置
- 取得 GPIO 、 Clks 、 Resets 、 Regulator 、 PHY 等資源

4. 硬體初始化
```c
ret = rk_pcie_hardware_io_config(rk_pcie);
```
- 執行硬體初始化。包括電源、Clks 和 PHY 配置等

5. 設定 Host config
```c
ret = rk_pcie_host_config(rk_pcie);
```
- 配置 PCIe 主機控制器暫存器等等
- 設定記憶體映射、中斷路由等等

6. 軟體處理
```c
  ret = rk_pcie_init_irq_and_wq(rk_pcie, pdev);
  ret = rk_add_pcie_port(rk_pcie, pdev);
```
- 初始化中斷和工作佇列
- 建立 PCIe 埠並嘗試連接

7. 特殊功能初始化
```c
//如果是信號測試就退出:
if (rk_pcie->is_signal_test == true)
	return 0;

//熱插拔支援：
if (rk_pcie->slot_pluggable) {
	rk_pcie->hp_slot.plat_ops = &rk_pcie_gpio_hp_plat_ops;
	ret = register_gpio_hotplug_slot(&rk_pcie->hp_slot);
	gpiod_set_debounce(rk_pcie->hp_slot.gpiod, 200);
}

//DMA 初始化：
ret = rk_pcie_init_dma_trx(rk_pcie);

//建立偵錯檔案系統：
ret = rockchip_pcie_debugfs_init(rk_pcie);

```
8. 其他的設定
```c
device_init_wakeup(dev, true);
device_enable_async_suspend(dev);
rk_pcie->finish_probe = true;
```
- 啟用裝置喚醒功能
- 啟用異步掛起（適用於多埠 SoC）
- 標記初始化完成

## Interrupt
由 `rk_pcie_init_irq_and_wq` 去初始化中斷相關的項目。 Rockchip PCIe 總共有六個不同的中斷。
- PCIe System interrupt
- PCIe Legacy interrupt
- MSI / MSIX interrupt
- PCIe Error interrupt
- PCIe Message Receice interrupt
- PCIe Power Management interrupt

### Sys Interrupt
- 在 `rk_pcie_request_sys_irq` 做初始化。 
- `rk_pcie_sys_irq_handler` 為 sys irq 的處理函數
- 在 `rk_pcie_sys_irq_handler` 中主要處理了 DMA 傳輸完成、 DMA 錯誤和 hot reset 等系統等級的中斷事件。

### PCIe Legacy interrupt
- 使用 `rk_pcie_init_irq_domain` 去建立一個 `irq domain` 。
- 建立 Legacy 中斷的 IRQ domain，為四個 Legacy 中斷線創建虛擬中斷號映射。
- 頂層中斷處理為 `rk_pcie_legacy_int_handler` 函數。 `rk_pcie_legacy_int_handler` 處理中斷的時候。會去看是哪個 legacy 中斷引起的，然後再分發到相應的中段處理函數。
```c
static void rk_pcie_legacy_int_handler(struct irq_desc *desc)
  {
      struct irq_chip *chip = irq_desc_get_chip(desc);
      struct rk_pcie *rockchip = irq_desc_get_handler_data(desc);
      u32 reg, hwirq;

      chained_irq_enter(chip, desc);

      // 讀取 Legacy 中斷狀態
      reg = rk_pcie_readl_apb(rockchip, PCIE_CLIENT_INTR_STATUS_LEGACY);
      reg = reg & 0xf;  // 只關心低4位 (INTA~INTD)

      // 處理每個觸發的中斷
      while (reg) {
          hwirq = ffs(reg) - 1;  // 找到第一個設定位
          reg &= ~BIT(hwirq);    // 清除該位

          // 分發到相應的中斷處理函數
          ret = generic_handle_domain_irq(rockchip->irq_domain, hwirq);
          if (ret)
              dev_err(dev, "unexpected IRQ, INT%d\n", hwirq);
      }

      chained_irq_exit(chip, desc);
  }

```

### MSI interrupt
- `dw_pcie_msi_host_init` 函數負責初始化 DesignWare PCIe 控制器的 MSI (Message Signaled Interrupts) 主機支援。
- `dw_pcie_msi_host_init` 裡會呼叫 `dmam_alloc_coherent` 分配 DMA 一致性記憶體並作為 MSI 目標位址使用。
```c
msi_vaddr = dmam_alloc_coherent(dev, sizeof(u64), &pp->msi_data,
				GFP_KERNEL);
if (!msi_vaddr) {
	dev_err(dev, "Failed to alloc and map MSI data\n");
	dw_pcie_free_msi(pp);
	return -ENOMEM;
}
```
- `dw_pcie_msi_host_init` 還會設定 MSI IRQ domain ，並設置 `dw_chained_msi_isr` 為 `msi` 中斷的處理函數。
 
- `dw_chained_msi_isr` 會在 `msi` 被觸發時讀取 MSI 中斷狀態暫存器，確定哪些 MSI 被觸發。然後將每個觸發的 MSI 分派到對應的中斷處理器。

## Host Init
### rk_add_pcie_port
- `rk_add_pcie_port` 函數是 PCIe 埠初始化和主機橋接器註冊 的核心函數，負責 RockChip PCIe 控制器註冊為 PCI  主機橋接器，使其能夠管理連接的 PCIe 設備。
- `rk_add_pcie_port` 會去呼叫 `dw_pcie_host_init` 去做真正的初始化。

### dw_pcie_host_init
1. 初始化鎖和資源
```c
raw_spin_lock_init(&pp->lock);

res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "config");
if (res) {
	pp->cfg0_size = resource_size(res);
	pp->cfg0_base = res->start;
	pp->va_cfg0_base = devm_pci_remap_cfg_resource(dev, res);
} else {
	dev_err(dev, "Missing *config* reg space\n");
	return -ENODEV;
}
```
- 初始化 spin lock
- 取得並映射配置空間資源

2. DBI 基址設定
```c
if (!pci->dbi_base) {
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	pci->dbi_base = devm_pci_remap_cfg_resource(dev, res);
}
```
3. 分配 PCI 主機橋接器
```c
bridge = devm_pci_alloc_host_bridge(dev, 0);
if (!bridge)
	return -ENOMEM;
pp->bridge = bridge;
```

4. I/O 範圍配置
```c
win = resource_list_first_type(&bridge->windows, IORESOURCE_IO);
if (win) {
	pp->io_size = resource_size(win->res);
	pp->io_bus_addr = win->res->start - win->offset;
	pp->io_base = pci_pio_to_address(win->res->start);
}

```
5. 設定操作函數
```c
bridge->ops = &dw_pcie_ops;
bridge->child_ops = &dw_child_pcie_ops;
```

6. 呼叫主機特定初始化
```c
if (pp->ops->host_init) {
	// 這裡會呼叫 RockChip 的 host_init 如果有設定這個函數的話
	ret = pp->ops->host_init(pp);  
	if (ret)
	return ret;
}
```

7. MSI 初始化
```c
if (pci_msi_enabled()) {
	// 確定是否有 MSI 控制器
	pp->has_msi_ctrl = !(pp->ops->msi_host_init ||
						of_property_read_bool(np, "msi-parent") ||
						of_property_read_bool(np, "msi-map"));

	// 初始化 MSI
	if (pp->ops->msi_host_init) {
		ret = pp->ops->msi_host_init(pp);
	} else if (pp->has_msi_ctrl) {
		ret = dw_pcie_msi_host_init(pp);
	}
}
```
8. 硬體檢測和設定
```c
dw_pcie_version_detect(pci);    // 檢測 PCIe 版本
dw_pcie_iatu_detect(pci);       // 檢測 iATU 功能

ret = dw_pcie_setup_rc(pp);     // 設定根複合體
if (ret)
	goto err_free_msi;

// 嘗試建立連接
if (!dw_pcie_link_up(pci)) {
	ret = dw_pcie_start_link(pci);
	if (ret)
	goto err_free_msi;
}

```
9. 註冊 PCI 主機橋接器
```c
dw_pcie_wait_for_link(pci);     // 等待連接建立

bridge->sysdata = pp;

ret = pci_host_probe(bridge);   // 註冊到 PCI 子系統
if (ret)
	goto err_stop_link;
```
### 整體流程圖
```
  rk_add_pcie_port()
      ↓
  設定 pp->ops = &rk_pcie_host_ops
      ↓
  配置 MSI 向量數量
      ↓
  dw_pcie_host_init()
      ↓
  ┌─ 初始化資源映射 (config, dbi)
  ├─ 分配 PCI 主機橋接器
  ├─ 設定 I/O 範圍
  ├─ 呼叫 RockChip host_init (如果有)
  ├─ 初始化 MSI 控制器
  ├─ 檢測硬體版本和功能
  ├─ 設定根複合體 (RC)
  ├─ 建立 PCIe 連接
  └─ 註冊到 PCI 子系統
      ↓
  return 0 (成功)
```

## PCIe Ranges
```dts
pcie@2a200000 {
	...
	#address-cells = <0x03>;
	#size-cells = <0x02>;
	bus-range = <0x00 0x0f>;
	ranges = <0x800 0x00 0x20000000 0x00 0x20000000 0x00 0x100000 0x81000000 0x00 0x20100000 0x00 0x20100000 0x00 0x100000 0x82000000 0x00 0x20200000 0x00 0x20200000 0x00 0xe00000 0x83000000 0x09 0x00 0x09 0x00 0x00 0x80000000>;
	...
};
```
解析 device tree 可以得到以下的 resources:
- Bus Range 資源:
```dts
bus-range = <0x00 0x0f>;
→ Bus 0-15 (IORESOURCE_BUS)
```

- PCI Address Ranges
1. Configuration Space (0x800 ...)
```
0x800 0x00 0x20000000 0x00 0x20000000 0x00 0x100000
  → Type: Configuration Space
  → PCI 地址: 0x20000000
  → CPU 地址: 0x20000000
  → 大小: 0x100000 (1MB)
  → 資源類型: 用於 PCIe 配置空間存取
```
2. I/O Space (0x81000000)
```
0x81000000 0x00 0x20100000 0x00 0x20100000 0x00 0x100000
  → 資源類型: IORESOURCE_IO
  → PCI 地址: 0x20100000
  → CPU 地址: 0x20100000
  → 大小: 0x100000 (1MB)
  → 這個會被 resource_list_first_type(&bridge->windows, IORESOURCE_IO) 找到
```
3. 32-bit Memory Space (0x82000000)
```
0x82000000 0x00 0x20200000 0x00 0x20200000 0x00 0xe00000
  → 資源類型: IORESOURCE_MEM (non-prefetchable)
  → PCI 地址: 0x20200000
  → CPU 地址: 0x20200000
  → 大小: 0xe00000 (14MB)
```
4. 64-bit Prefetchable Memory Space (0x83000000)
```
  0x83000000 0x09 0x00 0x09 0x00 0x00 0x80000000
  → 資源類型: IORESOURCE_MEM | IORESOURCE_PREFETCH | IORESOURCE_MEM_64
  → PCI 地址: 0x900000000 (64-bit)
  → CPU 地址: 0x900000000
  → 大小: 0x80000000 (2GB)
```

## ATU
- ATU 一個 Outbound 加 Inbound 的暫存器大小為 512 bytes 對齊.
- iatu_unroll_enabled 為 true 的時候代表 
	1. 你的 PCIe 控制器支援並使用更高效的 Unroll Mode
	2. 每個 ATU 區域都有獨立的暫存器地址空間
	3. ATU 操作性能更好，無需 viewport 選擇步驟
	4. 這是較新版本的 DesignWare PCIe IP 特性
- out bound ATU 主要在以下兩個地方設定：
	1. 主要設定位置：`dw_pcie_iatu_setup` 函數。在 `dw_pcie_setup_rc` 函數中呼叫。
	2. 動態配置位置：配置空間存取函數。 `dw_pcie_rd_other_conf` 和 `dw_pcie_wr_other_conf`
- outbound ATU 的空間配置為:
	* ATU 0: 保留給配置空間 (動態重新配置) 
	* ATU 1: 第一個記憶體區域
	* ATU 2: 第二個記憶體區域
