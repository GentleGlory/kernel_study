# Register Mapping

## PCI Client and Core Register Address Mapping
| Mode | A[23] | Address Space Select | Address region |
| --| --|--|--|
| EP/RC | 0 | Client Register Set | 0xFD00_0000 ~ 0xFD7F_FFFF|
| EP/RC | 1 | Client Register Set | 0xFD80_0000 ~ 0xFDFF_FFFF|

總共為16Mbytes. 在驅動中是mapping到`apb_base`
```c
struct rockchip_pcie {
	void	__iomem *reg_base;		/* DT axi-base */
	void	__iomem *apb_base;		/* DT apb-base */
    ...
```

## Global Address Map for Core Local Management Bus
| Mode | A[22] | A[21] | A[20] | A[19:22] | Address Space[11:2] | Address Base |
| -- | -- | -- | -- | -- | -- | -- |
| 0(EP) | 0 | x | 0 | 0 | PCI/PCIe Function 0 Configureation Registers | 0xFD80_0000 |
| 0(EP) | 0 | x | 0 | 1~8 | PCI/PCIe Virtual Function 0~7 Configureation Registers | 0xFD81_0000 |
| 0(EP) | 0 | x | 0 | 9~255 | reserved |  |
| 1(RP) | 0 | 0 | 0 | 0 | Root Port Registers, nomal access | 0xFD80_0000 |
| 1(RP) | 0 | 1 | 0 | 0 | Root Port Registers. In this mode, certain RO fields in the configuration space can be written. Please see documentation of the RC mode registers for more information.  | 0xFDA0_0000 |
| X(EP/RP) | 0 | X | 1 | 0 | Local management registers  | 0xFD90_0000 |
| X(EP/RP) | 1 | 0 | X | X | Address Translation registers  | 0xFDC0_0000 |
| X(EP/RP) | 1 | 1 | X | X | PCIe DMA registers  | 0xFDE0_0000 |


## Remote Address Mapping
* 總共有64 Mbytes , 基底位址為 0xF800_0000。
* 分為兩部分。第一部分 Region 0 為Addr[25] == 0, 大小為 32 Mbytes。 第二部分為 Region 1~32, 總共大小為 32 Mbytes。

| A[25] | A[24:20] | Size(Mbytes) | Address Space[11:2] |
|---|---|---|---|
| 0 | X | 32 | Region 0 |
| 1 | 0 | 1 | Region 1 |
| 1 | 1 | 1 | Region 2 |
| 1 | 2 | 1 | Region 3 |
| 1 | 3 | 1 | Region 4 |
| 1 | ... | 1 | ... |
| 1 | 30 | 1 | Region 31 |
| 1 | 31 | 1 | Region 32 |

然後根據 device tree 的設定以及 `rockchip_cfg_atu` 函數的內容可知：
1. Region 1~30 為 memory mapping 的區域。
2. Region 31 為 I/0  mapping 的區域。
3. Region 32 為 Messagae mapping 的區域。

```c
static int rockchip_cfg_atu(struct rockchip_pcie *rockchip)
{
	struct device *dev = rockchip->dev;
	int offset;
	int err;
	int reg_no;

	for (reg_no = 0; reg_no < (rockchip->mem_size >> 20); reg_no++) {
		err = rockchip_pcie_prog_ob_atu(rockchip, reg_no + 1,
						AXI_WRAPPER_MEM_WRITE,
						20 - 1,
						rockchip->mem_bus_addr + (reg_no << 20),
						0);
		if (err) {
			dev_err(dev, "program RC mem outbound ATU failed\n");
			return err;
		}
	}

	err = rockchip_pcie_prog_ib_atu(rockchip, 2, 32 - 1, 0x0, 0);

	offset = rockchip->mem_size >> 20;
	for (reg_no = 0; reg_no < (rockchip->io_size >> 20); reg_no++) {
		err = rockchip_pcie_prog_ob_atu(rockchip, 
						reg_no + 1 + offset,
						AXI_WRAPPER_IO_WRITE,
						20 - 1,
						rockchip->io_bus_addr + (reg_no << 20),
						0);
		if (err) {
			dev_err(dev, "program RC io outbound ATU failed\n");
			return err;
		}
	}

	rockchip_pcie_prog_ob_atu(rockchip, reg_no + 1 + offset,
				AXI_WRAPPER_NOR_MSG,
				20 - 1, 0, 0);

	rockchip->msg_bus_addr = rockchip->mem_bus_addr + 
			((reg_no + offset) << 20);

	return err;
}
```

