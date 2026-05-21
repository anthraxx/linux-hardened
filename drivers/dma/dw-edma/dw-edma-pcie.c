// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018-2019 Synopsys, Inc. and/or its affiliates.
 * Synopsys DesignWare eDMA PCIe driver
 *
 * Author: Gustavo Pimentel <gustavo.pimentel@synopsys.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/dma/edma.h>
#include <linux/pci-epf.h>
#include <linux/msi.h>

#include "dw-edma-core.h"

#define DW_BLOCK(a, b, c) \
	{ \
		.bar = a, \
		.off = b, \
		.sz = c, \
	},

struct dw_edma_block {
	enum pci_barno			bar;
	off_t				off;
	size_t				sz;
};

struct dw_edma_pcie_data {
	/* eDMA registers location */
	struct dw_edma_block		rg;
	/* eDMA memory linked list location */
	struct dw_edma_block		ll_wr[EDMA_MAX_WR_CH];
	struct dw_edma_block		ll_rd[EDMA_MAX_RD_CH];
	/* eDMA memory data location */
	struct dw_edma_block		dt_wr[EDMA_MAX_WR_CH];
	struct dw_edma_block		dt_rd[EDMA_MAX_RD_CH];
	/* Other */
	u32				version;
	enum dw_edma_mode		mode;
	u8				irqs;
	u16				wr_ch_cnt;
	u16				rd_ch_cnt;
};

static const struct dw_edma_pcie_data snps_edda_data = {
	/* eDMA registers location */
	.rg.bar				= BAR_0,
	.rg.off				= 0x00001000,	/*  4 Kbytes */
	.rg.sz				= 0x00002000,	/*  8 Kbytes */
	/* eDMA memory linked list location */
	.ll_wr = {
		/* Channel 0 - BAR 2, offset 0 Mbytes, size 2 Mbytes */
		DW_BLOCK(BAR_2, 0x00000000, 0x00200000)
		/* Channel 1 - BAR 2, offset 2 Mbytes, size 2 Mbytes */
		DW_BLOCK(BAR_2, 0x00200000, 0x00200000)
	},
	.ll_rd = {
		/* Channel 0 - BAR 2, offset 4 Mbytes, size 2 Mbytes */
		DW_BLOCK(BAR_2, 0x00400000, 0x00200000)
		/* Channel 1 - BAR 2, offset 6 Mbytes, size 2 Mbytes */
		DW_BLOCK(BAR_2, 0x00600000, 0x00200000)
	},
	/* eDMA memory data location */
	.dt_wr = {
		/* Channel 0 - BAR 2, offset 8 Mbytes, size 14 Mbytes */
		DW_BLOCK(BAR_2, 0x00800000, 0x00e00000)
		/* Channel 1 - BAR 2, offset 22 Mbytes, size 14 Mbytes */
		DW_BLOCK(BAR_2, 0x01600000, 0x00e00000)
	},
	.dt_rd = {
		/* Channel 0 - BAR 2, offset 36 Mbytes, size 14 Mbytes */
		DW_BLOCK(BAR_2, 0x02400000, 0x00e00000)
		/* Channel 1 - BAR 2, offset 50 Mbytes, size 14 Mbytes */
		DW_BLOCK(BAR_2, 0x03200000, 0x00e00000)
	},
	/* Other */
	.version			= 0,
	.mode				= EDMA_MODE_UNROLL,
	.irqs				= 1,
	.wr_ch_cnt			= 2,
	.rd_ch_cnt			= 2,
};

static int dw_edma_pcie_irq_vector(struct device *dev, unsigned int nr)
{
	return pci_irq_vector(to_pci_dev(dev), nr);
}

static const struct dw_edma_core_ops dw_edma_pcie_core_ops = {
	.irq_vector = dw_edma_pcie_irq_vector,
};

static int dw_edma_pcie_probe(struct pci_dev *pdev,
			      const struct pci_device_id *pid)
{
	const struct dw_edma_pcie_data *pdata = (void *)pid->driver_data;
	struct device *dev = &pdev->dev;
	struct dw_edma_chip *chip;
	int err, nr_irqs;
	struct dw_edma *dw;
	int i, mask;

	if (!pdata)
		return -ENODEV;

	/* Enable PCI device */
	err = pcim_enable_device(pdev);
	if (err) {
		pci_err(pdev, "enabling device failed\n");
		return err;
	}

	/* Mapping PCI BAR regions */
	mask = BIT(pdata->rg.bar);
	for (i = 0; i < pdata->wr_ch_cnt; i++) {
		mask |= BIT(pdata->ll_wr[i].bar);
		mask |= BIT(pdata->dt_wr[i].bar);
	}
	for (i = 0; i < pdata->rd_ch_cnt; i++) {
		mask |= BIT(pdata->ll_rd[i].bar);
		mask |= BIT(pdata->dt_rd[i].bar);
	}
	err = pcim_iomap_regions(pdev, mask, pci_name(pdev));
	if (err) {
		pci_err(pdev, "eDMA BAR I/O remapping failed\n");
		return err;
	}

	pci_set_master(pdev);

	/* DMA configuration */
	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (!err) {
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
		if (err) {
			pci_err(pdev, "consistent DMA mask 64 set failed\n");
			return err;
		}
	} else {
		pci_err(pdev, "DMA mask 64 set failed\n");

		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			pci_err(pdev, "DMA mask 32 set failed\n");
			return err;
		}

		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			pci_err(pdev, "consistent DMA mask 32 set failed\n");
			return err;
		}
	}

	/* Data structure allocation */
	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dw = devm_kzalloc(dev, sizeof(*dw), GFP_KERNEL);
	if (!dw)
		return -ENOMEM;

	/* IRQs allocation */
	nr_irqs = pci_alloc_irq_vectors(pdev, 1, pdata->irqs,
					PCI_IRQ_MSI | PCI_IRQ_MSIX);
	if (nr_irqs < 1) {
		pci_err(pdev, "fail to alloc IRQ vector (number of IRQs=%u)\n",
			nr_irqs);
		return -EPERM;
	}

	/* Data structure initialization */
	chip->dw = dw;
	chip->dev = dev;
	chip->id = pdev->devfn;

	dw->chip = chip;
	dw->version = pdata->version;
	dw->mode = pdata->mode;
	dw->nr_irqs = nr_irqs;
	dw->core = &dw_edma_pcie_core_ops;
	dw->wr_ch_cnt = pdata->wr_ch_cnt;
	dw->rd_ch_cnt = pdata->rd_ch_cnt;

	dw->rg_region.vaddr = pcim_iomap_table(pdev)[pdata->rg.bar];
	dw->rg_region.vaddr += pdata->rg.off;
	dw->rg_region.paddr = pdev->resource[pdata->rg.bar].start;
	dw->rg_region.paddr += pdata->rg.off;
	dw->rg_region.sz = pdata->rg.sz;

	for (i = 0; i < dw->wr_ch_cnt; i++) {
		struct dw_edma_region *ll_region = &dw->ll_region_wr[i];
		struct dw_edma_region *dt_region = &dw->dt_region_wr[i];
		const struct dw_edma_block *ll_block = &pdata->ll_wr[i];
		const struct dw_edma_block *dt_block = &pdata->dt_wr[i];

		ll_region->vaddr = pcim_iomap_table(pdev)[ll_block->bar];
		ll_region->vaddr += ll_block->off;
		ll_region->paddr = pdev->resource[ll_block->bar].start;
		ll_region->paddr += ll_block->off;
		ll_region->sz = ll_block->sz;

		dt_region->vaddr = pcim_iomap_table(pdev)[dt_block->bar];
		dt_region->vaddr += dt_block->off;
		dt_region->paddr = pdev->resource[dt_block->bar].start;
		dt_region->paddr += dt_block->off;
		dt_region->sz = dt_block->sz;
	}

	for (i = 0; i < dw->rd_ch_cnt; i++) {
		struct dw_edma_region *ll_region = &dw->ll_region_rd[i];
		struct dw_edma_region *dt_region = &dw->dt_region_rd[i];
		const struct dw_edma_block *ll_block = &pdata->ll_rd[i];
		const struct dw_edma_block *dt_block = &pdata->dt_rd[i];

		ll_region->vaddr = pcim_iomap_table(pdev)[ll_block->bar];
		ll_region->vaddr += ll_block->off;
		ll_region->paddr = pdev->resource[ll_block->bar].start;
		ll_region->paddr += ll_block->off;
		ll_region->sz = ll_block->sz;

		dt_region->vaddr = pcim_iomap_table(pdev)[dt_block->bar];
		dt_region->vaddr += dt_block->off;
		dt_region->paddr = pdev->resource[dt_block->bar].start;
		dt_region->paddr += dt_block->off;
		dt_region->sz = dt_block->sz;
	}

	/* Debug info */
	pci_dbg(pdev, "Version:\t%u\n", dw->version);

	pci_dbg(pdev, "Mode:\t%s\n",
		dw->mode == EDMA_MODE_LEGACY ? "Legacy" : "Unroll");

	pci_dbg(pdev, "Registers:\tBAR=%u, off=0x%.8lx, sz=0x%zx bytes, addr(v=%p, p=%pa)\n",
		pdata->rg.bar, pdata->rg.off, pdata->rg.sz,
		dw->rg_region.vaddr, &dw->rg_region.paddr);

	for (i = 0; i < dw->wr_ch_cnt; i++) {
		pci_dbg(pdev, "L. List:\tWRITE CH%.2u, BAR=%u, off=0x%.8lx, sz=0x%zx bytes, addr(v=%p, p=%pa)\n",
			i, pdata->ll_wr[i].bar, pdata->ll_wr[i].off,
			dw->ll_region_wr[i].sz, dw->ll_region_wr[i].vaddr,
			&dw->ll_region_wr[i].paddr);

		pci_dbg(pdev, "Data:\tWRITE CH%.2u, BAR=%u, off=0x%.8lx, sz=0x%zx bytes, addr(v=%p, p=%pa)\n",
			i, pdata->dt_wr[i].bar, pdata->dt_wr[i].off,
			dw->dt_region_wr[i].sz, dw->dt_region_wr[i].vaddr,
			&dw->dt_region_wr[i].paddr);
	}

	for (i = 0; i < dw->rd_ch_cnt; i++) {
		pci_dbg(pdev, "L. List:\tREAD CH%.2u, BAR=%u, off=0x%.8lx, sz=0x%zx bytes, addr(v=%p, p=%pa)\n",
			i, pdata->ll_rd[i].bar, pdata->ll_rd[i].off,
			dw->ll_region_rd[i].sz, dw->ll_region_rd[i].vaddr,
			&dw->ll_region_rd[i].paddr);

		pci_dbg(pdev, "Data:\tREAD CH%.2u, BAR=%u, off=0x%.8lx, sz=0x%zx bytes, addr(v=%p, p=%pa)\n",
			i, pdata->dt_rd[i].bar, pdata->dt_rd[i].off,
			dw->dt_region_rd[i].sz, dw->dt_region_rd[i].vaddr,
			&dw->dt_region_rd[i].paddr);
	}

	pci_dbg(pdev, "Nr. IRQs:\t%u\n", dw->nr_irqs);

	/* Validating if PCI interrupts were enabled */
	if (!pci_dev_msi_enabled(pdev)) {
		pci_err(pdev, "enable interrupt failed\n");
		return -EPERM;
	}

	dw->irq = devm_kcalloc(dev, nr_irqs, sizeof(*dw->irq), GFP_KERNEL);
	if (!dw->irq)
		return -ENOMEM;

	/* Starting eDMA driver */
	err = dw_edma_probe(chip);
	if (err) {
		pci_err(pdev, "eDMA probe failed\n");
		return err;
	}

	/* Saving data structure reference */
	pci_set_drvdata(pdev, chip);

	return 0;
}

static void dw_edma_pcie_remove(struct pci_dev *pdev)
{
	struct dw_edma_chip *chip = pci_get_drvdata(pdev);
	int err;

	/* Stopping eDMA driver */
	err = dw_edma_remove(chip);
	if (err)
		pci_warn(pdev, "can't remove device properly: %d\n", err);

	/* Freeing IRQs */
	pci_free_irq_vectors(pdev);
}

static const struct pci_device_id dw_edma_pcie_id_table[] = {
	{ PCI_DEVICE_DATA(SYNOPSYS, EDDA, &snps_edda_data) },
	{ }
};
MODULE_DEVICE_TABLE(pci, dw_edma_pcie_id_table);

static struct pci_driver dw_edma_pcie_driver = {
	.name		= "dw-edma-pcie",
	.id_table	= dw_edma_pcie_id_table,
	.probe		= dw_edma_pcie_probe,
	.remove		= dw_edma_pcie_remove,
};

module_pci_driver(dw_edma_pcie_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Synopsys DesignWare eDMA PCIe driver");
MODULE_AUTHOR("Gustavo Pimentel <gustavo.pimentel@synopsys.com>");
