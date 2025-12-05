# DeviceTree 說明

在 `arch/arm64/boot/dts/rockchip/rk3399.dtsi` 中可以看到 PCIe 的 Device Tree 定義如下：

```dts
pcie0: pcie@f8000000 {
	compatible = "rockchip,rk3399-pcie";
	#address-cells = <3>;
	#size-cells = <2>;
	aspm-no-l0s;
	clocks = <&cru ACLK_PCIE>, <&cru ACLK_PERF_PCIE>,
		 <&cru PCLK_PCIE>, <&cru SCLK_PCIE_PM>;
	clock-names = "aclk", "aclk-perf", "hclk", "pm";
	bus-range = <0x0 0x1f>;
	max-link-speed = <1>;
	linux,pci-domain = <0>;
	msi-map = <0x0 &its 0x0 0x1000>;
	interrupts = <GIC_SPI 49 IRQ_TYPE_LEVEL_HIGH 0>,
		     <GIC_SPI 50 IRQ_TYPE_LEVEL_HIGH 0>,
		     <GIC_SPI 51 IRQ_TYPE_LEVEL_HIGH 0>;
	interrupt-names = "sys", "legacy", "client";
	#interrupt-cells = <1>;
	interrupt-map-mask = <0 0 0 7>;
	interrupt-map = <0 0 0 1 &pcie0_intc 0>,
			<0 0 0 2 &pcie0_intc 1>,
			<0 0 0 3 &pcie0_intc 2>,
			<0 0 0 4 &pcie0_intc 3>;
	phys = <&pcie_phy>;
	phy-names = "pcie-phy";
	ranges = <0x83000000 0x0 0xfa000000 0x0 0xfa000000 0x0 0x1e00000
		  0x81000000 0x0 0xfbe00000 0x0 0xfbe00000 0x0 0x100000>;
	reg = <0x0 0xf8000000 0x0 0x2000000>,
	      <0x0 0xfd000000 0x0 0x1000000>;
	reg-names = "axi-base", "apb-base";
	resets = <&cru SRST_PCIE_CORE>, <&cru SRST_PCIE_MGMT>,
		 <&cru SRST_PCIE_MGMT_STICKY>, <&cru SRST_PCIE_PIPE>,
		 <&cru SRST_PCIE_PM>, <&cru SRST_P_PCIE>,
		 <&cru SRST_A_PCIE>;
	reset-names = "core", "mgmt", "mgmt-sticky", "pipe",
		      "pm", "pclk", "aclk";
	status = "disabled";
	pcie0_intc: interrupt-controller {
		interrupt-controller;
		#address-cells = <0>;
		#interrupt-cells = <1>;
	};
};
```

現在我們來逐行解析：

---

### `compatible = "rockchip,rk3399-pcie";`

這行會對應到驅動程式中的 `of_device_id` 結構，用來匹配驅動與裝置樹中的節點。

```c
// drivers/pci/host/pcie-rockchip.c
static const struct of_device_id rockchip_pcie_of_match[] = {
	{ .compatible = "rockchip,rk3399-pcie", },
	{}
};
```

當 Kernel 解析 Device Tree 時，會根據 `compatible` 中的字串與驅動中的清單做比對，若匹配成功，則會綁定對應的驅動程式。

---

### `#address-cells = <3>;`

### `#size-cells = <2>;`

這兩行的設定表示：

* `#address-cells = <3>`：子節點的地址欄位需使用三個 `32-bit` 整數表示，通常用於 PCIe 這種需要表示完整 PCI bus 位址的設備（高位、中位、低位）。
* `#size-cells = <2>`：用兩個 `32-bit` 整數來表示該節點或子節點的記憶體區段大小。

這些值會在解析 `ranges` 屬性時使用，用來定義 PCI bus 與 SoC memory bus 的對應關係。

---

### `aspm-no-l0s`

表示 ASPM (Active State Power Management) 不支援 L0s 狀態。在驅動中，當 Device Tree 中有 `aspm-no-l0s` 屬性時，會清除 Root Complex 端的 L0s 能力。

```c
// Clear L0s from RC's link cap
if (of_property_read_bool(dev->of_node, "aspm-no-l0s")) {
	status = rockchip_pcie_read(rockchip, PCIE_RC_CONFIG_LINK_CAP);
	status &= ~PCIE_RC_CONFIG_LINK_CAP_L0S;
	rockchip_pcie_write(rockchip, status, PCIE_RC_CONFIG_LINK_CAP);
}
```

---

### Clock and Reset Settings

```dts
clocks = <&cru ACLK_PCIE>, <&cru ACLK_PERF_PCIE>,
	 <&cru PCLK_PCIE>, <&cru SCLK_PCIE_PM>;
clock-names = "aclk", "aclk-perf", "hclk", "pm";

resets = <&cru SRST_PCIE_CORE>, <&cru SRST_PCIE_MGMT>,
	 <&cru SRST_PCIE_MGMT_STICKY>, <&cru SRST_PCIE_PIPE>,
	 <&cru SRST_PCIE_PM>, <&cru SRST_P_PCIE>,
	 <&cru SRST_A_PCIE>;
reset-names = "core", "mgmt", "mgmt-sticky", "pipe",
	"pm", "pclk", "aclk";
```

這些 clock 和 reset 設定會在 `rockchip_pcie_parse_dt` 中解析，並儲存在 `struct rockchip_pcie` 中。

```c
struct rockchip_pcie {
	...
	struct reset_control *core_rst;
	struct reset_control *mgmt_rst;
	struct reset_control *mgmt_sticky_rst;
	struct reset_control *pipe_rst;
	struct reset_control *pm_rst;
	struct reset_control *pclk_rst;
	struct reset_control *aclk_rst;
	struct clk *aclk_pcie;
	struct clk *aclk_perf_pcie;
	struct clk *hclk_pcie;
	struct clk *clk_pcie_pm;
	...
};
```

---

### Bus Range Settings

```dts
bus-range = <0x0 0x1f>;
```

在 `rockchip_pcie_probe` 呼叫 `of_pci_get_host_bridge_resources` 解析。

```c
int of_pci_get_host_bridge_resources(...) {
	...
	err = of_pci_parse_bus_range(dev, bus_range);
	if (err) {
		bus_range->start = busno;
		bus_range->end = bus_max;
		bus_range->flags = IORESOURCE_BUS;
	} else {
		if (bus_range->end > bus_range->start + bus_max)
			bus_range->end = bus_range->start + bus_max;
	}
	pci_add_resource(resources, bus_range);
	...
}
```

---

### Link Speed Setting

```dts
max-link-speed = <1>;
```

透過 `of_pci_get_max_link_speed` 解析。

```c
int of_pci_get_max_link_speed(struct device_node *node) {
	u32 max_link_speed;
	if (of_property_read_u32(node, "max-link-speed", &max_link_speed) ||
	    max_link_speed > 4)
		return -EINVAL;
	return max_link_speed;
}
```

| 數字 | 速度等級                 | 傳輸速度（每 lane） |
| -- | -------------------- | ------------ |
| 1  | PCIe Gen1 (2.5 GT/s) | 約 250 MB/s   |
| 2  | PCIe Gen2 (5.0 GT/s) | 約 500 MB/s   |
| 3  | PCIe Gen3 (8.0 GT/s) | 約 985 MB/s   |
| 4  | PCIe Gen4 (16 GT/s)  | 約 1969 MB/s  |

---

### PCI Domain Setting

```dts
linux,pci-domain = <0>;
```

透過 `of_get_pci_domain_nr` 取得。用來區分多個 PCIe 控制器的 domain。

PCI 裝置完整位址格式：`[domain]:[bus]:[device].[function]`

---

### MSI Mapping

```dts
msi-map = <0x0 &its 0x0 0x1000>;
```

格式如下：

```
msi-map = <device-id-base phandle msi-base msi-count>;
```

| 欄位             | 說明                           |
| -------------- | ---------------------------- |
| device-id-base | 起始 RID                       |
| phandle        | 指向 GIC-ITS 的 node            |
| msi-base       | ITS 中的 virtual device ID 起始值 |
| msi-count      | 可用 ID 數量                     |

---

### Interrupts Settings

```dts
interrupts = <GIC_SPI 49 IRQ_TYPE_LEVEL_HIGH 0>,
	     <GIC_SPI 50 IRQ_TYPE_LEVEL_HIGH 0>,
	     <GIC_SPI 51 IRQ_TYPE_LEVEL_HIGH 0>;
interrupt-names = "sys", "legacy", "client";
#interrupt-cells = <1>;
interrupt-map-mask = <0 0 0 7>;
interrupt-map = <0 0 0 1 &pcie0_intc 0>,
		<0 0 0 2 &pcie0_intc 1>,
		<0 0 0 3 &pcie0_intc 2>,
		<0 0 0 4 &pcie0_intc 3>;

pcie0_intc: interrupt-controller {
		interrupt-controller;
		#address-cells = <0>;
		#interrupt-cells = <1>;
	};		
```

這樣設定表示 PCIe Device 產生的 INTA\~INTD 將透過 `pcie0_intc` 做中斷轉譯。

#### sys interrupt handler

處理 Core 與 PHY 事件。

#### legacy interrupt handler

用 `irq_set_chained_handler_and_data()` 而非 `devm_request_irq()` 是因為 legacy 中斷不是單純設備中斷，而是從 PCIe RC 接收到的“中斷分派器”訊號，需要透過 `irq_domain` 對應後轉發。

```c
irq_set_chained_handler_and_data(irq, rockchip_pcie_legacy_int_handler, rockchip);
```

處理函數如下：

```c
static void rockchip_pcie_legacy_int_handler(struct irq_desc *desc) {
	...
	reg = rockchip_pcie_read(rockchip, PCIE_CLIENT_INT_STATUS);
	reg = (reg & PCIE_CLIENT_INTR_MASK) >> PCIE_CLIENT_INTR_SHIFT;
	while (reg) {
		hwirq = ffs(reg) - 1;
		reg &= ~BIT(hwirq);
		virq = irq_find_mapping(rockchip->irq_domain, hwirq);
		if (virq)
			generic_handle_irq(virq);
		else
			dev_err(dev, "unexpected IRQ, INT%d\n", hwirq);
	}
	...
}
```

然後在 `rockchip_pcie_probe` 裡呼叫：

```c
rockchip_pcie_init_irq_domain(rockchip);
```

來建立對應的 `irq_domain`。

---

### PHY Setting

```dts
phys = <&pcie_phy>;
phy-names = "pcie-phy";
```

驅動在 `rockchip_pcie_parse_dt()` 中呼叫 `devm_phy_get()` 以取得對應的 PHY，並將其儲存於 `rockchip_pcie` 結構體中：

```c
static int rockchip_pcie_parse_dt(struct rockchip_pcie* rockchip)
{
	...

	rockchip->phy = devm_phy_get(dev, "pcie_phy");
	if (IS_ERR(rockchip->phy)) {
		if (PTR_ERR(rockchip->phy) != -EPROBE_DEFER)
			dev_err(dev, "Failed to get PCIe PHY\n");
		return PTR_ERR(rockchip->phy);
	}

	...
}
```

之後在 `rockchip_pcie_init_port()` 中，會透過 `phy_init()` 及 `phy_power_on()` 完成 PHY 的初始化與上電：

```c
static int rockchip_pcie_init_port(struct rockchip_pcie *rockchip)
{
	...

	err = phy_init(rockchip->phy);
	if (err) {
		dev_err(dev, "Failed to init PHY, err: %d\n", err);
		return err;
	}

	err = phy_power_on(rockchip->phy);
	if (err) {
		dev_err(dev, "Failed to power on PHY, err: %d\n", err);
		return err;
	}

	...
}
```

而在 `rockchip_pcie_suspend_noirq()` 中，則會呼叫 `phy_power_off()` 和 `phy_exit()` 來完成 suspend 階段的 PHY 關閉與資源釋放：

```c
static int __maybe_unused rockchip_pcie_suspend_noirq(struct device *dev)
{
	...

	phy_power_off(rockchip->phy);
	phy_exit(rockchip->phy);

	...
}
```

---

### Ranges Setting（PCIe Address Mapping）

```dts
ranges = <0x83000000 0x0 0xfa000000  0x0 0xfa000000  0x0 0x1e00000
          0x81000000 0x0 0xfbe00000  0x0 0xfbe00000  0x0 0x100000>;
```

驅動在 `rockchip_pcie_probe()` 中，透過 `of_pci_get_host_bridge_resources()` 來解析上述 `ranges` 屬性：

```c
static int rockchip_pcie_probe(struct platform_device *pdev)
{
	...

	err = of_pci_get_host_bridge_resources(dev->of_node, 0, 0xff,
					       &res, &io_base);
	if (err)
		goto err_init_port;

	...
}
```

這個 API 最終會呼叫 `of_pci_range_parser_init()` 將 `ranges` 初始化成一個可解析的結構，並透過 `for_each_of_pci_range()` 來依序解析每筆 entry：

```c
int of_pci_get_host_bridge_resources(...)
{
	...

	err = of_pci_range_parser_init(&parser, dev);
	if (err)
		goto parse_failed;

	for_each_of_pci_range(&parser, &range) {
		...
		of_pci_range_to_resource(&range, dev, res);
		pci_add_resource_offset(resources, res, res->start - range.pci_addr);
	}
}
```

---

#### `ranges` 格式說明

`ranges` 每一筆 entry 的結構為：

```
<child_bus_addr> <parent_phys_addr> <size>
```

其中：

* `child_bus_addr` 的長度由此 node 的 `#address-cells` 決定（RK3399 為 3）
* `parent_phys_addr` 的長度由根節點的 `#address-cells` 決定（RK3399 為 2）
* `size` 的長度由此 node 的 `#size-cells` 決定（RK3399 為 2）

所以每筆 `ranges` entry 佔用 3 + 2 + 2 = **7 個 32-bit cells**。

---

#### 🧾 實際解析

RK3399 的 `ranges` 將被解析為兩個 entry：

```text
Entry 1:
0x83000000 0x0 0xfa000000   // PCIe bus address
0x0        0xfa000000       // CPU physical address
0x0        0x1e00000        // size: 0x1e00000 (30M Bytes)

Entry 2:
0x81000000 0x0 0xfbe00000   // PCIe I/O address
0x0        0xfbe00000       // CPU physical address
0x0        0x100000         // size: 0x100000 (1M Bytes)
```

---

#### 🏷️ Address Type（根據 PCI spec）

每筆 `child_bus_addr` 的高 32-bit 表示其資源類型：

| 資源類型                | 標誌位值       |
| ------------------- | ---------- |
| I/O                 | 0x81000000 |
| Memory              | 0x82000000 |
| Prefetchable Memory | 0x83000000 |

這些資訊最終會被轉換為 `struct resource` 結構，供後續 PCI 裝置使用。

---

### Register Setting

```dts
reg = <0x0 0xf8000000 0x0 0x2000000>,
      <0x0 0xfd000000 0x0 0x1000000>;
reg-names = "axi-base", "apb-base";
```

根據 RK3399 根節點中 `#address-cells = <2>` 與 `#size-cells = <2>`，每筆 `reg` entry 為：

```
<phys_addr (2 cells)> <size (2 cells)>
```

因此可被解析為：

```text
AXI Base:
Phys Addr = 0xf8000000
Size      = 0x2000000  (32MB)

APB Base:
Phys Addr = 0xfd000000
Size      = 0x1000000  (16MB)
```

這些區域是 PCIe controller 的 MMIO 寄存器空間，會被 ioremap 到 kernel space 供驅動使用。

---

