/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 HiSilicon Limited. */
#ifndef __HISI_RME_HPRE_H
#define __HISI_RME_HPRE_H

#include <linux/hisi_acc_qm.h>

enum {
	HPRE_CLUSTER0,
	HPRE_CLUSTER1,
	HPRE_CLUSTER2,
	HPRE_CLUSTER3,
	HPRE_CLUSTERS_NUM_MAX
};

enum hpre_ctrl_dbgfs_file {
	HPRE_CLEAR_ENABLE,
	HPRE_CLUSTER_CTRL,
	HPRE_DEBUG_FILE_NUM,
};


#define HPRE_DEBUGFS_FILE_NUM (HPRE_DEBUG_FILE_NUM + HPRE_CLUSTERS_NUM_MAX - 1)

struct hpre_debugfs_file {
	int index;
	enum hpre_ctrl_dbgfs_file type;
	spinlock_t lock;
	struct hpre_debug *debug;
};

/*
 * One HPRE controller has one PF and multiple VFs, some global configurations
 * which PF has need this structure.
 * Just relevant for PF.
 */
struct hpre_debug {
	struct hpre_debugfs_file files[HPRE_DEBUGFS_FILE_NUM];
};

struct hpre {
	struct hisi_qm qm;
	struct hpre_debug debug;
};


enum hpre_cap_table_type {
	HPRE_QM_RAS_NFE_TYPE = 0x0,
	HPRE_QM_RAS_NFE_RESET,
	HPRE_QM_RAS_CE_TYPE,
	HPRE_RAS_NFE_TYPE,
	HPRE_RAS_NFE_RESET,
	HPRE_RAS_CE_TYPE,
	HPRE_CORE_INFO,
	HPRE_CORE_EN,
};

int hpre_probe(struct pci_dev *pdev);
void hpre_remove(struct pci_dev *pdev);
struct hisi_qm_list *get_rme_hpre_devices(void);
#endif
