// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 HiSilicon Limited. */

#include <linux/module.h>
#include <linux/pci.h>
#ifdef CONFIG_HISI_CCADA_HOST
#include <asm/hisi_cca_da.h>
#endif

#include "rme_acc.h"
#include "rme_sec.h"
#include "rme_hpre.h"
#include "rme_zip.h"

#define OVERRIDE_ONLY		1

static const struct pci_device_id rme_acc_ids[] = {
	{ PCI_DEVICE_DRIVER_OVERRIDE(PCI_VENDOR_ID_HUAWEI,
				     PCI_DEVICE_ID_HUAWEI_SEC_PF,
				     OVERRIDE_ONLY) },
	{ PCI_DEVICE_DRIVER_OVERRIDE(PCI_VENDOR_ID_HUAWEI,
				     PCI_DEVICE_ID_HUAWEI_HPRE_PF,
				     OVERRIDE_ONLY) },
	{ PCI_DEVICE_DRIVER_OVERRIDE(PCI_VENDOR_ID_HUAWEI,
				     PCI_DEVICE_ID_HUAWEI_ZIP_PF,
				     OVERRIDE_ONLY) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, rme_acc_ids);

static struct dentry *rme_debugfs_root;
static struct dentry *rme_sec_debugfs;
static struct dentry *rme_hpre_debugfs;
static struct dentry *rme_zip_debugfs;

struct dentry *get_rme_root_debugfs(struct pci_dev *pdev)
{
	if (!pdev)
		return NULL;

	switch (pdev->device) {
	case PCI_DEVICE_ID_HUAWEI_SEC_PF:
		return rme_sec_debugfs;
	case PCI_DEVICE_ID_HUAWEI_HPRE_PF:
		return rme_hpre_debugfs;
	case PCI_DEVICE_ID_HUAWEI_ZIP_PF:
		return rme_zip_debugfs;
	default:
		return NULL;
	}
}

static int rme_acc_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	switch (pdev->device) {
	case PCI_DEVICE_ID_HUAWEI_SEC_PF:
		return sec_probe(pdev);
	case PCI_DEVICE_ID_HUAWEI_HPRE_PF:
		return hpre_probe(pdev);
	case PCI_DEVICE_ID_HUAWEI_ZIP_PF:
		return zip_probe(pdev);
	default:
		return -ENODEV;
	}
}

static void rme_acc_remove(struct pci_dev *pdev)
{
	switch (pdev->device) {
	case PCI_DEVICE_ID_HUAWEI_SEC_PF:
		return sec_remove(pdev);
	case PCI_DEVICE_ID_HUAWEI_HPRE_PF:
		return hpre_remove(pdev);
	case PCI_DEVICE_ID_HUAWEI_ZIP_PF:
		return zip_remove(pdev);
	default:
		return;
	}
}

static struct pci_driver rme_acc_driver = {
	.name			= "rme_acc",
	.id_table		= rme_acc_ids,
	.probe			= rme_acc_probe,
	.remove			= rme_acc_remove,
	.sriov_configure = IS_ENABLED(CONFIG_PCI_IOV) ?
				hisi_qm_sriov_configure : NULL,
};

static void rme_devices_list_init(void)
{
	hisi_qm_init_list(get_rme_sec_devices());
	hisi_qm_init_list(get_rme_hpre_devices());
	hisi_qm_init_list(get_rme_zip_devices());
}

static void rme_register_debugfs(void)
{
	if (!debugfs_initialized())
		return;

	rme_debugfs_root = debugfs_create_dir("rme_acc", NULL);
	rme_sec_debugfs = debugfs_create_dir("rme_sec", rme_debugfs_root);
	rme_hpre_debugfs = debugfs_create_dir("rme_hpre", rme_debugfs_root);
	rme_zip_debugfs = debugfs_create_dir("rme_zip", rme_debugfs_root);
}

static void rme_unregister_debugfs(void)
{
	debugfs_remove_recursive(rme_debugfs_root);
}

static int __init rme_acc_init(void)
{
	int ret;

#ifdef CONFIG_HISI_CCADA_HOST
	hisi_pcipc_ns_add(rme_acc_ids);
#else
	pr_err("RME ACC Driver requires HISI CCA DA to be enabled, check the kernel config.\n");
	return -EOPNOTSUPP;
#endif

	rme_devices_list_init();
	rme_register_debugfs();

	ret = pci_register_driver(&rme_acc_driver);
	if (ret < 0) {
		pr_err("Failed to register pci driver.\n");
		goto err;
	}

	return ret;
err:
	rme_unregister_debugfs();
#ifdef CONFIG_HISI_CCADA_HOST
	hisi_pcipc_ns_remove(rme_acc_ids);
#endif
	return ret;
}

static void __exit rme_acc_exit(void)
{
	pci_unregister_driver(&rme_acc_driver);
	rme_unregister_debugfs();
#ifdef CONFIG_HISI_CCADA_HOST
	hisi_pcipc_ns_remove(rme_acc_ids);
#endif
}

module_init(rme_acc_init);
module_exit(rme_acc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiajing He <hejiajing2@h-partners.com>");
MODULE_DESCRIPTION("Driver for HiSilicon accelerator RME support");
