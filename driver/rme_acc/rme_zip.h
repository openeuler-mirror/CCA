/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 HiSilicon Limited. */
#ifndef __HISI_RME_ZIP_H
#define __HISI_RME_ZIP_H

#include <linux/hisi_acc_qm.h>

struct hisi_zip_ctrl;

struct hisi_zip {
	struct hisi_qm qm;
	struct hisi_zip_ctrl *ctrl;
};

enum zip_cap_table_type {
	ZIP_QM_RAS_NFE_TYPE,
	ZIP_QM_RAS_NFE_RESET,
	ZIP_QM_RAS_CE_TYPE,
	ZIP_RAS_NFE_TYPE,
	ZIP_RAS_NFE_RESET,
	ZIP_RAS_CE_TYPE,
	ZIP_CORE_INFO,
	ZIP_CORE_EN,
};

int zip_probe(struct pci_dev *pdev);
void zip_remove(struct pci_dev *pdev);
struct hisi_qm_list *get_rme_zip_devices(void);
/* dae */
int hisi_dae_set_user_domain(struct hisi_qm *qm);
void hisi_dae_hw_error_disable(struct hisi_qm *qm);
void hisi_dae_hw_error_enable(struct hisi_qm *qm);
void hisi_dae_open_axi_master_ooo(struct hisi_qm *qm);
int hisi_dae_close_axi_master_ooo(struct hisi_qm *qm);
bool hisi_dae_dev_is_abnormal(struct hisi_qm *qm);
enum acc_err_result hisi_dae_get_err_result(struct hisi_qm *qm);
#endif
