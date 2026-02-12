/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 HiSilicon Limited. */

#ifndef __HISI_RME_SEC_H
#define __HISI_RME_SEC_H

#include <linux/hisi_acc_qm.h>

enum sec_debug_file_index {
	SEC_CLEAR_ENABLE,
	SEC_DEBUG_FILE_NUM,
};

struct sec_debug_file {
	enum sec_debug_file_index index;
	spinlock_t lock;
	struct hisi_qm *qm;
};

struct sec_debug {
	struct sec_debug_file files[SEC_DEBUG_FILE_NUM];
};

struct sec_dev {
	struct hisi_qm qm;
	struct sec_debug debug;
};

enum sec_cap_type {
	SEC_QM_NFE_MASK_CAP = 0x0,
	SEC_QM_RESET_MASK_CAP,
	SEC_QM_OOO_SHUTDOWN_MASK_CAP,
	SEC_QM_CE_MASK_CAP,
	SEC_NFE_MASK_CAP,
	SEC_RESET_MASK_CAP,
	SEC_OOO_SHUTDOWN_MASK_CAP,
	SEC_CE_MASK_CAP,
	SEC_CORE_ENABLE_BITMAP,
};

enum sec_cap_table_type {
	SEC_QM_RAS_NFE_TYPE = 0x0,
	SEC_QM_RAS_NFE_RESET,
	SEC_QM_RAS_CE_TYPE,
	SEC_RAS_NFE_TYPE,
	SEC_RAS_NFE_RESET,
	SEC_RAS_CE_TYPE,
	SEC_CORE_INFO,
	SEC_CORE_EN,
};

int sec_probe(struct pci_dev *pdev);
void sec_remove(struct pci_dev *pdev);
struct hisi_qm_list *get_rme_sec_devices(void);
#endif
