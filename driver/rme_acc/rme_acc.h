/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 HiSilicon Limited. */
#ifndef __HISI_RME_ACC_H
#define __HISI_RME_ACC_H

#include <linux/hisi_acc_qm.h>

#define PCI_DEVICE_ID_HUAWEI_ZIP_PF	0xa250
#define PCI_DEVICE_ID_HUAWEI_SEC_PF	0xa255
#define PCI_DEVICE_ID_HUAWEI_HPRE_PF	0xa258
/* Assign 1 qp for PF just in case of calling hisi_qm_start() in reset procedure */
#define RME_ACC_PF_DEF_Q_NUM		1
#define RME_ACC_PF_DEF_Q_BASE		0

struct dentry *get_rme_root_debugfs(struct pci_dev *pdev);
void rme_acc_disable_ifc(struct hisi_qm *qm);

#endif
