#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_device.h>

// TODO(human): Add necessary header files and struct frank_pcie_data definition here
#include <linux/of.h>
#include <linux/io.h>
#include <linux/of_pci.h>
#include <linux/of_device.h>

struct frank_pcie {
	struct device *dev;	
	struct pci_host_bridge *pci_host;
	void __iomem * config_space;
	struct resource *mem;
};

// TODO(human): Implement frank_pcie_read and frank_pcie_write functions here
// Uncomment the #if 0 block and implement the functions

int frank_pcie_config_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *val)
{
	struct frank_pcie *frank_pcie = (struct frank_pcie *) bus->sysdata;
	struct device *dev = frank_pcie->dev;
	u32 offset;
	*val = 0xffffffff;
	
	if (where & (size - 1) || size > 4 || where >= 4096) {
		dev_err(dev, "Missing aligned config access where:%d, size: %d\n",
				where, size);
		return -EINVAL;
	}

	offset = (bus->number << 20) | (devfn << 12) + where;

	if (size == 4 ) {
		*val = readl(frank_pcie->config_space + offset);
	} else if (size == 2) {
		*val = readw(frank_pcie->config_space + offset);
	} else if (size == 1) {
		*val = readb(frank_pcie->config_space + offset);
	} else {
		dev_err(dev, "Unsupport size:%d\n",size);
		return -EINVAL;
	}

	return 0;
}

int frank_pcie_config_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 val)
{
	struct frank_pcie *frank_pcie = (struct frank_pcie *) bus->sysdata;
	struct device *dev = frank_pcie->dev;
	u32 offset;
	
	if (where & (size - 1) || size > 4 || where >= 4096) {
		dev_err(dev, "Missing aligned config access where:%d, size: %d\n",
				where, size);
		return -EINVAL;
	}

	offset = (bus->number << 20) | (devfn << 12) + where;

	if (size == 4 ) {
		writel(val, frank_pcie->config_space + offset);
	} else if (size == 2) {
		writew(val, frank_pcie->config_space + offset);
	} else if (size == 1) {
		writeb(val, frank_pcie->config_space + offset);
	} else {
		dev_err(dev, "Unsupport size:%d\n",size);
		return -EINVAL;
	}

	return 0;
}

struct pci_ops frank_pcie_ops = {
	.read = frank_pcie_config_read,
	.write = frank_pcie_config_write,
};

static int frank_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct frank_pcie *frank_pcie;
	struct resource* res;
	int ret = 0;

	dev_info(dev, "frank_pcie probing\n");

	frank_pcie = devm_kzalloc(dev, sizeof(struct frank_pcie), GFP_KERNEL);
	if (!frank_pcie) {
		dev_err(dev, "Cannot alloc private data\n");
		ret = -ENOMEM;
		goto error;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "config_space");
	if (!res) {
		dev_err(dev, "Cannot get config resource\n");
		ret = -ENODEV;
		goto error;
	}

	frank_pcie->config_space = devm_ioremap_resource(dev, res);
	if (IS_ERR(frank_pcie->config_space)) {
		dev_err(dev, "Cannot map the config space\n");
		ret = PTR_ERR(frank_pcie->config_space);
		goto error;
	}

	frank_pcie->pci_host = devm_pci_alloc_host_bridge(dev, 0);
	if (!frank_pcie->pci_host) {
		dev_err(dev, "Failed to alloc host bridge\n");
		ret = -ENOMEM;
		goto error;
	}

	frank_pcie->pci_host->dev.parent = dev;
	frank_pcie->pci_host->ops = &frank_pcie_ops;
	frank_pcie->pci_host->sysdata = frank_pcie;

	dev_set_drvdata(dev, frank_pcie);

	ret = pci_host_probe(frank_pcie->pci_host);
	if (ret) {
		dev_err(dev, "Failed to probe pcie host\n");		
	}

error:
	return ret;	
}

static void frank_pcie_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "frank_pcie removed\n");
}



static const struct of_device_id frank_pcie_of_match[] = {
	{ .compatible = "frank,pcie-host", },
	{},
};
MODULE_DEVICE_TABLE(of, frank_pcie_of_match);

static struct platform_driver frank_pcie_platform_driver = {
	.probe = frank_pcie_probe,
	.remove = frank_pcie_remove,
	.driver = {
		.name = "frank_pcie",
		.of_match_table = frank_pcie_of_match,
	},
};

module_platform_driver(frank_pcie_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank");
MODULE_DESCRIPTION("Frank PCIe Host Controller Test Driver");
MODULE_VERSION("1.0");