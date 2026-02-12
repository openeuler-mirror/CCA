// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 HiSilicon Limited. */

#include "rme_sec.h"
#include "rme_acc.h"

#define SEC_SQE_SIZE			128

#define SEC_CTRL_CNT_CLR_CE		0x301120
#define SEC_CTRL_CNT_CLR_CE_BIT		BIT(0)
#define SEC_CORE_INT_SOURCE		0x301010
#define SEC_CORE_INT_MASK		0x301000
#define SEC_CORE_INT_STATUS		0x301008
#define SEC_CORE_SRAM_ECC_ERR_INFO	0x301C14
#define SEC_ECC_NUM			16
#define SEC_ECC_MASH			0xFF
#define SEC_CORE_INT_DISABLE		0x0

#define SEC_RAS_CE_REG			0x301050
#define SEC_RAS_FE_REG			0x301054
#define SEC_RAS_NFE_REG			0x301058
#define SEC_RAS_FE_ENB_MSK		0x0
#define SEC_OOO_SHUTDOWN_SEL		0x301014
#define SEC_RAS_DISABLE			0x0
#define SEC_AXI_ERROR_MASK		(BIT(0) | BIT(1))

#define SEC_MEM_START_INIT_REG		0x301100
#define SEC_MEM_INIT_DONE_REG		0x301104

/* clock gating */
#define SEC_CONTROL_REG			0x301200
#define SEC_DYNAMIC_GATE_REG		0x30121c
#define SEC_CORE_AUTO_GATE		0x30212c
#define SEC_DYNAMIC_GATE_EN		0x7fff
#define SEC_CORE_AUTO_GATE_EN		GENMASK(3, 0)
#define SEC_CLK_GATE_ENABLE		BIT(3)
#define SEC_CLK_GATE_DISABLE		(~BIT(3))

#define SEC_TRNG_EN_SHIFT		8
#define SEC_SAA_EN_REG			0x301270
#define SEC_AXI_SHUTDOWN_ENABLE		BIT(12)
#define SEC_AXI_SHUTDOWN_DISABLE	0xFFFFEFFF
#define SEC_CORE_INT_STATUS_M_ECC	BIT(2)

#define SEC_PREFETCH_CFG		0x301130
#define SEC_SVA_TRANS			0x301EC4
#define SEC_PREFETCH_ENABLE		(~(BIT(0) | BIT(1) | BIT(11)))
#define SEC_PREFETCH_DISABLE		BIT(1)
#define SEC_SVA_DISABLE_READY		(BIT(7) | BIT(11))
#define SEC_SVA_PREFETCH_INFO		0x301ED4
#define SEC_SVA_STALL_NUM		GENMASK(23, 8)
#define SEC_SVA_PREFETCH_NUM		GENMASK(2, 0)
#define SEC_WAIT_SVA_READY		500000
#define SEC_READ_SVA_STATUS_TIMES	3
#define SEC_WAIT_US_MIN			10
#define SEC_WAIT_US_MAX			20

#define SEC_DELAY_10_US			10
#define SEC_POLL_TIMEOUT_US		1000
#define SEC_DBGFS_VAL_MAX_LEN		20
#define SEC_SINGLE_PORT_MAX_TRANS	0x2060

#define SEC_SQE_MASK_OFFSET		16
#define SEC_SQE_MASK_LEN		108

#define SEC_DFX_BASE			0x301000
#define SEC_DFX_CORE			0x302100
#define SEC_DFX_COMMON1			0x301600
#define SEC_DFX_COMMON2			0x301C00
#define SEC_DFX_BASE_LEN		0x9D
#define SEC_DFX_CORE_LEN		0x32B
#define SEC_DFX_COMMON1_LEN		0x45
#define SEC_DFX_COMMON2_LEN		0xBA

struct sec_hw_error {
	u32 int_msk;
	const char *msg;
};

static const char rme_sec_name[] = "rme_sec";

static const struct hisi_qm_cap_info sec_basic_info[] = {
	{SEC_QM_NFE_MASK_CAP,   0x3124, 0, GENMASK(31, 0), 0x0, 0x1C77, 0x7C77},
	{SEC_QM_RESET_MASK_CAP, 0x3128, 0, GENMASK(31, 0), 0x0, 0xC77, 0x6C77},
	{SEC_QM_OOO_SHUTDOWN_MASK_CAP, 0x3128, 0, GENMASK(31, 0), 0x0, 0x4, 0x6C77},
	{SEC_QM_CE_MASK_CAP,    0x312C, 0, GENMASK(31, 0), 0x0, 0x8, 0x8},
	{SEC_NFE_MASK_CAP,      0x3130, 0, GENMASK(31, 0), 0x0, 0x177, 0x60177},
	{SEC_RESET_MASK_CAP,    0x3134, 0, GENMASK(31, 0), 0x0, 0x177, 0x177},
	{SEC_OOO_SHUTDOWN_MASK_CAP, 0x3134, 0, GENMASK(31, 0), 0x0, 0x4, 0x177},
	{SEC_CE_MASK_CAP,       0x3138, 0, GENMASK(31, 0), 0x0, 0x88, 0xC088},
	{SEC_CORE_ENABLE_BITMAP, 0x3140, 0, GENMASK(31, 0), 0x17F, 0x17F, 0xF},
};

static const struct hisi_qm_cap_query_info sec_cap_query_info[] = {
	{SEC_QM_RAS_NFE_TYPE,  "SEC_QM_RAS_NFE_TYPE  ", 0x3124, 0x0, 0x1C77, 0x7C77},
	{SEC_QM_RAS_NFE_RESET, "SEC_QM_RAS_NFE_RESET ", 0x3128, 0x0, 0xC77, 0x6C77},
	{SEC_QM_RAS_CE_TYPE,   "SEC_QM_RAS_CE_TYPE   ", 0x312C, 0x0, 0x8, 0x8},
	{SEC_RAS_NFE_TYPE,     "SEC_RAS_NFE_TYPE     ", 0x3130, 0x0, 0x177, 0x60177},
	{SEC_RAS_NFE_RESET,    "SEC_RAS_NFE_RESET    ", 0x3134, 0x0, 0x177, 0x177},
	{SEC_RAS_CE_TYPE,      "SEC_RAS_CE_TYPE      ", 0x3138, 0x0, 0x88, 0xC088},
	{SEC_CORE_INFO,        "SEC_CORE_INFO        ", 0x313c, 0x110404, 0x110404, 0x110404},
	{SEC_CORE_EN,          "SEC_CORE_EN          ", 0x3140, 0x17F, 0x17F, 0xF},
};

static const struct sec_hw_error sec_hw_errors[] = {
	{
		.int_msk = BIT(0),
		.msg = "sec_axi_rresp_err_rint"
	},
	{
		.int_msk = BIT(1),
		.msg = "sec_axi_bresp_err_rint"
	},
	{
		.int_msk = BIT(2),
		.msg = "sec_ecc_2bit_err_rint"
	},
	{
		.int_msk = BIT(3),
		.msg = "sec_ecc_1bit_err_rint"
	},
	{
		.int_msk = BIT(4),
		.msg = "sec_req_trng_timeout_rint"
	},
	{
		.int_msk = BIT(5),
		.msg = "sec_fsm_hbeat_rint"
	},
	{
		.int_msk = BIT(6),
		.msg = "sec_channel_req_rng_timeout_rint"
	},
	{
		.int_msk = BIT(7),
		.msg = "sec_bd_err_rint"
	},
	{
		.int_msk = BIT(8),
		.msg = "sec_chain_buff_err_rint"
	},
	{
		.int_msk = BIT(14),
		.msg = "sec_no_secure_access"
	},
	{
		.int_msk = BIT(15),
		.msg = "sec_wrapping_key_auth_err"
	},
	{
		.int_msk = BIT(16),
		.msg = "sec_km_key_crc_fail"
	},
	{
		.int_msk = BIT(17),
		.msg = "sec_axi_poison_err"
	},
	{
		.int_msk = BIT(18),
		.msg = "sec_sva_err"
	},
	{}
};

static const char * const sec_dbg_file_name[] = {
	[SEC_CLEAR_ENABLE] = "clear_enable",
};

static const struct debugfs_reg32 sec_dfx_regs[] = {
	{"SEC_PF_ABNORMAL_INT_SOURCE    ",  0x301010},
	{"SEC_SAA_EN                    ",  0x301270},
	{"SEC_BD_LATENCY_MIN            ",  0x301600},
	{"SEC_BD_LATENCY_MAX            ",  0x301608},
	{"SEC_BD_LATENCY_AVG            ",  0x30160C},
	{"SEC_BD_NUM_IN_SAA0            ",  0x301670},
	{"SEC_BD_NUM_IN_SAA1            ",  0x301674},
	{"SEC_BD_NUM_IN_SEC             ",  0x301680},
	{"SEC_ECC_1BIT_CNT              ",  0x301C00},
	{"SEC_ECC_1BIT_INFO             ",  0x301C04},
	{"SEC_ECC_2BIT_CNT              ",  0x301C10},
	{"SEC_ECC_2BIT_INFO             ",  0x301C14},
	{"SEC_BD_SAA0                   ",  0x301C20},
	{"SEC_BD_SAA1                   ",  0x301C24},
	{"SEC_BD_SAA2                   ",  0x301C28},
	{"SEC_BD_SAA3                   ",  0x301C2C},
	{"SEC_BD_SAA4                   ",  0x301C30},
	{"SEC_BD_SAA5                   ",  0x301C34},
	{"SEC_BD_SAA6                   ",  0x301C38},
	{"SEC_BD_SAA7                   ",  0x301C3C},
	{"SEC_BD_SAA8                   ",  0x301C40},
	{"SEC_RAS_CE_ENABLE             ",  0x301050},
	{"SEC_RAS_FE_ENABLE             ",  0x301054},
	{"SEC_RAS_NFE_ENABLE            ",  0x301058},
	{"SEC_REQ_TRNG_TIME_TH          ",  0x30112C},
	{"SEC_CHANNEL_RNG_REQ_THLD      ",  0x302110},
};

/* define the SEC's dfx regs region and region length */
static struct dfx_diff_registers sec_diff_regs[] = {
	{
		.reg_offset = SEC_DFX_BASE,
		.reg_len = SEC_DFX_BASE_LEN,
	}, {
		.reg_offset = SEC_DFX_COMMON1,
		.reg_len = SEC_DFX_COMMON1_LEN,
	}, {
		.reg_offset = SEC_DFX_COMMON2,
		.reg_len = SEC_DFX_COMMON2_LEN,
	}, {
		.reg_offset = SEC_DFX_CORE,
		.reg_len = SEC_DFX_CORE_LEN,
	},
};

static int sec_diff_regs_show(struct seq_file *s, void *unused)
{
	struct hisi_qm *qm = s->private;

	hisi_qm_acc_diff_regs_dump(qm, s, qm->debug.acc_diff_regs,
				   ARRAY_SIZE(sec_diff_regs));

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(sec_diff_regs);

/* For PF device management, not support register to crypto */
static struct hisi_qm_list rme_sec_devices;
struct hisi_qm_list *get_rme_sec_devices(void)
{
	return &rme_sec_devices;
}

static void sec_set_endian(struct hisi_qm *qm)
{
	u32 reg;

	reg = readl_relaxed(qm->io_base + SEC_CONTROL_REG);
	reg &= ~(BIT(1) | BIT(0));
	if (!IS_ENABLED(CONFIG_64BIT))
		reg |= BIT(1);

	if (!IS_ENABLED(CONFIG_CPU_LITTLE_ENDIAN))
		reg |= BIT(0);

	writel_relaxed(reg, qm->io_base + SEC_CONTROL_REG);
}

static int sec_wait_sva_ready(struct hisi_qm *qm, __u32 offset, __u32 mask)
{
	u32 val, try_times = 0;
	u8 count = 0;

	/*
	 * Read the register value every 10-20us. If the value is 0 for three
	 * consecutive times, the SVA module is ready.
	 */
	do {
		val = readl(qm->io_base + offset);
		if (val & mask)
			count = 0;
		else if (++count == SEC_READ_SVA_STATUS_TIMES)
			break;

		usleep_range(SEC_WAIT_US_MIN, SEC_WAIT_US_MAX);
	} while (++try_times < SEC_WAIT_SVA_READY);

	if (try_times == SEC_WAIT_SVA_READY) {
		pci_err(qm->pdev, "Failed to wait sva prefetch ready\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static void sec_close_sva_prefetch(struct hisi_qm *qm)
{
	u32 val;
	int ret;

	if (!test_bit(QM_SUPPORT_SVA_PREFETCH, &qm->caps))
		return;

	val = readl_relaxed(qm->io_base + SEC_PREFETCH_CFG);
	val |= SEC_PREFETCH_DISABLE;
	writel(val, qm->io_base + SEC_PREFETCH_CFG);

	ret = readl_relaxed_poll_timeout(qm->io_base + SEC_SVA_TRANS,
					 val, !(val & SEC_SVA_DISABLE_READY),
					 SEC_DELAY_10_US, SEC_POLL_TIMEOUT_US);
	if (ret)
		pci_err(qm->pdev, "Failed to close sva prefetch\n");

	(void)sec_wait_sva_ready(qm, SEC_SVA_PREFETCH_INFO, SEC_SVA_STALL_NUM);
}

static void sec_open_sva_prefetch(struct hisi_qm *qm)
{
	u32 val;
	int ret;

	if (!test_bit(QM_SUPPORT_SVA_PREFETCH, &qm->caps))
		return;

	/* Enable prefetch */
	val = readl_relaxed(qm->io_base + SEC_PREFETCH_CFG);
	val &= SEC_PREFETCH_ENABLE;
	writel(val, qm->io_base + SEC_PREFETCH_CFG);

	ret = readl_relaxed_poll_timeout(qm->io_base + SEC_PREFETCH_CFG,
					 val, !(val & SEC_PREFETCH_DISABLE),
					 SEC_DELAY_10_US, SEC_POLL_TIMEOUT_US);
	if (ret) {
		pci_err(qm->pdev, "Failed to open sva prefetch\n");
		sec_close_sva_prefetch(qm);
		return;
	}

	ret = sec_wait_sva_ready(qm, SEC_SVA_TRANS, SEC_SVA_PREFETCH_NUM);
	if (ret)
		sec_close_sva_prefetch(qm);
}

static void sec_enable_clock_gate(struct hisi_qm *qm)
{
	u32 val;

	val = readl_relaxed(qm->io_base + SEC_CONTROL_REG);
	val |= SEC_CLK_GATE_ENABLE;
	writel_relaxed(val, qm->io_base + SEC_CONTROL_REG);

	val = readl(qm->io_base + SEC_DYNAMIC_GATE_REG);
	val |= SEC_DYNAMIC_GATE_EN;
	writel(val, qm->io_base + SEC_DYNAMIC_GATE_REG);

	val = readl(qm->io_base + SEC_CORE_AUTO_GATE);
	val |= SEC_CORE_AUTO_GATE_EN;
	writel(val, qm->io_base + SEC_CORE_AUTO_GATE);
}

static void sec_disable_clock_gate(struct hisi_qm *qm)
{
	u32 val;

	val = readl_relaxed(qm->io_base + SEC_CONTROL_REG);
	val &= SEC_CLK_GATE_DISABLE;
	writel_relaxed(val, qm->io_base + SEC_CONTROL_REG);
}

static int sec_engine_init(struct hisi_qm *qm)
{
	int ret;
	u32 reg;

	/* disable clock gate control before mem init */
	sec_disable_clock_gate(qm);

	writel_relaxed(0x1, qm->io_base + SEC_MEM_START_INIT_REG);

	ret = readl_relaxed_poll_timeout(qm->io_base + SEC_MEM_INIT_DONE_REG,
					 reg, reg & 0x1, SEC_DELAY_10_US,
					 SEC_POLL_TIMEOUT_US);
	if (ret) {
		pci_err(qm->pdev, "Failed to init sec mem!\n");
		return ret;
	}

	reg = readl_relaxed(qm->io_base + SEC_CONTROL_REG);
	reg |= (0x1 << SEC_TRNG_EN_SHIFT);
	writel_relaxed(reg, qm->io_base + SEC_CONTROL_REG);

	sec_open_sva_prefetch(qm);

	writel(SEC_SINGLE_PORT_MAX_TRANS,
	       qm->io_base + AM_CFG_SINGLE_PORT_MAX_TRANS);

	reg = hisi_qm_get_hw_info(qm, sec_basic_info, SEC_CORE_ENABLE_BITMAP,
				  qm->cap_ver);
	writel(reg, qm->io_base + SEC_SAA_EN_REG);

	/* config endian */
	sec_set_endian(qm);

	sec_enable_clock_gate(qm);

	return 0;
}

static int sec_set_user_domain_and_cache(struct hisi_qm *qm)
{
	/* qm user domain */
	writel(AXUSER_BASE, qm->io_base + QM_ARUSER_M_CFG_1);
	writel(ARUSER_M_CFG_ENABLE, qm->io_base + QM_ARUSER_M_CFG_ENABLE);
	writel(AXUSER_BASE, qm->io_base + QM_AWUSER_M_CFG_1);
	writel(AWUSER_M_CFG_ENABLE, qm->io_base + QM_AWUSER_M_CFG_ENABLE);
	writel(WUSER_M_CFG_ENABLE, qm->io_base + QM_WUSER_M_CFG_ENABLE);

	/* qm cache */
	writel(AXI_M_CFG, qm->io_base + QM_AXI_M_CFG);
	writel(AXI_M_CFG_ENABLE, qm->io_base + QM_AXI_M_CFG_ENABLE);

	/* disable FLR triggered by BME(bus master enable) */
	writel(PEH_AXUSER_CFG, qm->io_base + QM_PEH_AXUSER_CFG);
	writel(PEH_AXUSER_CFG_ENABLE, qm->io_base + QM_PEH_AXUSER_CFG_ENABLE);

	/* enable sqc,cqc writeback */
	writel(SQC_CACHE_ENABLE | CQC_CACHE_ENABLE | SQC_CACHE_WB_ENABLE |
	       CQC_CACHE_WB_ENABLE | FIELD_PREP(SQC_CACHE_WB_THRD, 1) |
	       FIELD_PREP(CQC_CACHE_WB_THRD, 1), qm->io_base + QM_CACHE_CTL);

	return sec_engine_init(qm);
}

/* sec_debug_regs_clear() - clear the sec debug regs */
static void sec_debug_regs_clear(struct hisi_qm *qm)
{
	int i;

	/* clear sec dfx regs */
	writel(0x1, qm->io_base + SEC_CTRL_CNT_CLR_CE);
	for (i = 0; i < ARRAY_SIZE(sec_dfx_regs); i++)
		readl(qm->io_base + sec_dfx_regs[i].offset);

	/* clear rdclr_en */
	writel(0x0, qm->io_base + SEC_CTRL_CNT_CLR_CE);

	hisi_qm_debug_regs_clear(qm);
}

static void sec_master_ooo_ctrl(struct hisi_qm *qm, bool enable)
{
	u32 val1, val2;

	val1 = readl(qm->io_base + SEC_CONTROL_REG);
	if (enable) {
		val1 |= SEC_AXI_SHUTDOWN_ENABLE;
		val2 = qm->err_info.dev_err.shutdown_mask;
	} else {
		val1 &= SEC_AXI_SHUTDOWN_DISABLE;
		val2 = 0x0;
	}

	writel(val2, qm->io_base + SEC_OOO_SHUTDOWN_SEL);
	writel(val1, qm->io_base + SEC_CONTROL_REG);
}

static void sec_hw_error_enable(struct hisi_qm *qm)
{
	struct hisi_qm_err_mask *dev_err = &qm->err_info.dev_err;
	u32 err_mask = dev_err->ce | dev_err->nfe | dev_err->fe;

	/* clear SEC hw error source if having */
	writel(err_mask, qm->io_base + SEC_CORE_INT_SOURCE);

	/* enable RAS int */
	writel(dev_err->ce, qm->io_base + SEC_RAS_CE_REG);
	writel(dev_err->fe, qm->io_base + SEC_RAS_FE_REG);
	writel(dev_err->nfe, qm->io_base + SEC_RAS_NFE_REG);

	/* enable SEC block master OOO when nfe occurs */
	sec_master_ooo_ctrl(qm, true);

	/* enable SEC hw error interrupts */
	writel(err_mask, qm->io_base + SEC_CORE_INT_MASK);
}

static void sec_hw_error_disable(struct hisi_qm *qm)
{
	/* disable SEC hw error interrupts */
	writel(SEC_CORE_INT_DISABLE, qm->io_base + SEC_CORE_INT_MASK);

	/* disable SEC block master OOO when nfe occurs */
	sec_master_ooo_ctrl(qm, false);

	/* disable RAS int */
	writel(SEC_RAS_DISABLE, qm->io_base + SEC_RAS_CE_REG);
	writel(SEC_RAS_DISABLE, qm->io_base + SEC_RAS_FE_REG);
	writel(SEC_RAS_DISABLE, qm->io_base + SEC_RAS_NFE_REG);
}

static u32 sec_clear_enable_read(struct hisi_qm *qm)
{
	return readl(qm->io_base + SEC_CTRL_CNT_CLR_CE) &
	       SEC_CTRL_CNT_CLR_CE_BIT;
}

static int sec_clear_enable_write(struct hisi_qm *qm, u32 val)
{
	u32 tmp;

	if (val != 1 && val)
		return -EINVAL;

	tmp = (readl(qm->io_base + SEC_CTRL_CNT_CLR_CE) &
	       ~SEC_CTRL_CNT_CLR_CE_BIT) | val;
	writel(tmp, qm->io_base + SEC_CTRL_CNT_CLR_CE);

	return 0;
}

static ssize_t sec_debug_read(struct file *filp, char __user *buf,
			      size_t count, loff_t *pos)
{
	struct sec_debug_file *file = filp->private_data;
	char tbuf[SEC_DBGFS_VAL_MAX_LEN];
	struct hisi_qm *qm = file->qm;
	u32 val;
	int ret;

	ret = hisi_qm_get_dfx_access(qm);
	if (ret)
		return ret;

	spin_lock_irq(&file->lock);

	switch (file->index) {
	case SEC_CLEAR_ENABLE:
		val = sec_clear_enable_read(qm);
		break;
	default:
		goto err_input;
	}

	spin_unlock_irq(&file->lock);

	hisi_qm_put_dfx_access(qm);
	ret = snprintf(tbuf, SEC_DBGFS_VAL_MAX_LEN, "%u\n", val);
	return simple_read_from_buffer(buf, count, pos, tbuf, ret);

err_input:
	spin_unlock_irq(&file->lock);
	hisi_qm_put_dfx_access(qm);
	return -EINVAL;
}

static ssize_t sec_debug_write(struct file *filp, const char __user *buf,
			       size_t count, loff_t *pos)
{
	struct sec_debug_file *file = filp->private_data;
	char tbuf[SEC_DBGFS_VAL_MAX_LEN];
	struct hisi_qm *qm = file->qm;
	unsigned long val;
	int len, ret;

	if (*pos != 0)
		return 0;

	if (count >= SEC_DBGFS_VAL_MAX_LEN)
		return -ENOSPC;

	len = simple_write_to_buffer(tbuf, SEC_DBGFS_VAL_MAX_LEN - 1,
				     pos, buf, count);
	if (len < 0)
		return len;

	tbuf[len] = '\0';
	if (kstrtoul(tbuf, 0, &val))
		return -EFAULT;

	ret = hisi_qm_get_dfx_access(qm);
	if (ret)
		return ret;

	spin_lock_irq(&file->lock);

	switch (file->index) {
	case SEC_CLEAR_ENABLE:
		ret = sec_clear_enable_write(qm, val);
		if (ret)
			goto err_input;
		break;
	default:
		ret = -EINVAL;
		goto err_input;
	}

	ret = count;

 err_input:
	spin_unlock_irq(&file->lock);
	hisi_qm_put_dfx_access(qm);
	return ret;
}

static const struct file_operations sec_dbg_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = sec_debug_read,
	.write = sec_debug_write,
};

static int sec_regs_show(struct seq_file *s, void *unused)
{
	hisi_qm_regs_dump(s, s->private);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(sec_regs);

static int sec_cap_regs_show(struct seq_file *s, void *unused)
{
	struct hisi_qm *qm = s->private;
	u32 i, size;

	size = qm->cap_tables.qm_cap_size;
	for (i = 0; i < size; i++)
		seq_printf(s, "%s= 0x%08x\n",
			   qm->cap_tables.qm_cap_table[i].name,
			   qm->cap_tables.qm_cap_table[i].cap_val);

	size = qm->cap_tables.dev_cap_size;
	for (i = 0; i < size; i++)
		seq_printf(s, "%s= 0x%08x\n",
			   qm->cap_tables.dev_cap_table[i].name,
			   qm->cap_tables.dev_cap_table[i].cap_val);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(sec_cap_regs);

static int sec_core_debug_init(struct hisi_qm *qm)
{
	struct dfx_diff_registers *sec_regs = qm->debug.acc_diff_regs;
	struct device *dev = &qm->pdev->dev;
	struct debugfs_regset32 *regset;
	struct dentry *tmp_d;

	tmp_d = debugfs_create_dir("sec_dfx", qm->debug.debug_root);

	regset = devm_kzalloc(dev, sizeof(*regset), GFP_KERNEL);
	if (!regset)
		return -ENOMEM;

	regset->regs = sec_dfx_regs;
	regset->nregs = ARRAY_SIZE(sec_dfx_regs);
	regset->base = qm->io_base;
	regset->dev = dev;

	debugfs_create_file("regs", 0444, tmp_d, regset, &sec_regs_fops);
	if (sec_regs)
		debugfs_create_file("diff_regs", 0444, tmp_d, qm,
				    &sec_diff_regs_fops);

	debugfs_create_file("cap_regs", 0444, qm->debug.debug_root, qm,
			    &sec_cap_regs_fops);

	return 0;
}

static int sec_debug_init(struct hisi_qm *qm)
{
	struct sec_dev *sec = container_of(qm, struct sec_dev, qm);
	int i;

	for (i = SEC_CLEAR_ENABLE; i < SEC_DEBUG_FILE_NUM; i++) {
		spin_lock_init(&sec->debug.files[i].lock);
		sec->debug.files[i].index = i;
		sec->debug.files[i].qm = qm;

		debugfs_create_file(sec_dbg_file_name[i], 0600,
				    qm->debug.debug_root,
				    sec->debug.files + i,
				    &sec_dbg_fops);
	}

	return sec_core_debug_init(qm);
}

static int sec_debugfs_init(struct hisi_qm *qm)
{
	struct device *dev = &qm->pdev->dev;
	struct dentry *root_debugfs;
	int ret;

	ret = hisi_qm_regs_debugfs_init(qm, sec_diff_regs,
					ARRAY_SIZE(sec_diff_regs));
	if (ret) {
		dev_warn(dev, "Failed to init SEC diff regs!\n");
		return ret;
	}

	root_debugfs = get_rme_root_debugfs(qm->pdev);
	if (!root_debugfs)
		dev_warn(dev, "Fail to get parent debugfs, dev_name replace!\n");
	qm->debug.debug_root = debugfs_create_dir(dev_name(dev), root_debugfs);
	qm->debug.sqe_mask_offset = SEC_SQE_MASK_OFFSET;
	qm->debug.sqe_mask_len = SEC_SQE_MASK_LEN;

	/* As we don't support qos yet, so don't need to create debug file */
	if (test_bit(QM_SUPPORT_FUNC_QOS, &qm->caps)) {
		clear_bit(QM_SUPPORT_FUNC_QOS, &qm->caps);
		hisi_qm_debug_init(qm);
		set_bit(QM_SUPPORT_FUNC_QOS, &qm->caps);
	} else {
		hisi_qm_debug_init(qm);
	}

	ret = sec_debug_init(qm);
	if (ret)
		goto debugfs_remove;

	return 0;

debugfs_remove:
	debugfs_remove_recursive(qm->debug.debug_root);
	hisi_qm_regs_debugfs_uninit(qm, ARRAY_SIZE(sec_diff_regs));
	return ret;
}

static void sec_debugfs_exit(struct hisi_qm *qm)
{
	debugfs_remove_recursive(qm->debug.debug_root);

	hisi_qm_regs_debugfs_uninit(qm, ARRAY_SIZE(sec_diff_regs));
}

static int sec_show_last_regs_init(struct hisi_qm *qm)
{
	struct qm_debug *debug = &qm->debug;
	int i;

	debug->last_words = kcalloc(ARRAY_SIZE(sec_dfx_regs),
				    sizeof(unsigned int), GFP_KERNEL);
	if (!debug->last_words)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(sec_dfx_regs); i++)
		debug->last_words[i] = readl_relaxed(qm->io_base +
						     sec_dfx_regs[i].offset);

	return 0;
}

static void sec_show_last_regs_uninit(struct hisi_qm *qm)
{
	struct qm_debug *debug = &qm->debug;

	if (!debug->last_words)
		return;

	kfree(debug->last_words);
	debug->last_words = NULL;
}

static void sec_show_last_dfx_regs(struct hisi_qm *qm)
{
	struct qm_debug *debug = &qm->debug;
	struct pci_dev *pdev = qm->pdev;
	u32 val;
	int i;

	if (!debug->last_words)
		return;

	/* dumps last word of the debugging registers during controller reset */
	for (i = 0; i < ARRAY_SIZE(sec_dfx_regs); i++) {
		val = readl_relaxed(qm->io_base + sec_dfx_regs[i].offset);
		if (val != debug->last_words[i])
			pci_info(pdev, "%s \t= 0x%08x => 0x%08x\n",
				sec_dfx_regs[i].name, debug->last_words[i], val);
	}
}

static void sec_log_hw_error(struct hisi_qm *qm, u32 err_sts)
{
	const struct sec_hw_error *errs = sec_hw_errors;
	struct device *dev = &qm->pdev->dev;
	u32 err_val;

	while (errs->msg) {
		if (errs->int_msk & err_sts) {
			dev_err(dev, "%s [error status=0x%x] found\n",
				errs->msg, errs->int_msk);

			if (SEC_CORE_INT_STATUS_M_ECC & errs->int_msk) {
				err_val = readl(qm->io_base +
						SEC_CORE_SRAM_ECC_ERR_INFO);
				dev_err(dev, "Multi ecc sram num=0x%x\n",
					((err_val) >> SEC_ECC_NUM) &
					SEC_ECC_MASH);
			}
		}
		errs++;
	}
}

static u32 sec_get_hw_err_status(struct hisi_qm *qm)
{
	return readl(qm->io_base + SEC_CORE_INT_STATUS);
}

static void sec_clear_hw_err_status(struct hisi_qm *qm, u32 err_sts)
{
	writel(err_sts, qm->io_base + SEC_CORE_INT_SOURCE);
}

static void sec_disable_error_report(struct hisi_qm *qm, u32 err_type)
{
	u32 nfe_mask = qm->err_info.dev_err.nfe;

	writel(nfe_mask & (~err_type), qm->io_base + SEC_RAS_NFE_REG);
}

static void sec_open_axi_master_ooo(struct hisi_qm *qm)
{
	u32 val;

	val = readl(qm->io_base + SEC_CONTROL_REG);
	writel(val & SEC_AXI_SHUTDOWN_DISABLE, qm->io_base + SEC_CONTROL_REG);
	writel(val | SEC_AXI_SHUTDOWN_ENABLE, qm->io_base + SEC_CONTROL_REG);
}

static enum acc_err_result sec_get_err_result(struct hisi_qm *qm)
{
	u32 err_status;

	err_status = sec_get_hw_err_status(qm);
	if (err_status) {
		if (err_status & qm->err_info.dev_err.ecc_2bits_mask)
			qm->err_status.is_dev_ecc_mbit = true;
		sec_log_hw_error(qm, err_status);

		if (err_status & qm->err_info.dev_err.reset_mask) {
			/* Disable the same error reporting until device is recovered. */
			sec_disable_error_report(qm, err_status);
			return ACC_ERR_NEED_RESET;
		}
		sec_clear_hw_err_status(qm, err_status);
	}

	return ACC_ERR_RECOVERED;
}

static bool sec_dev_is_abnormal(struct hisi_qm *qm)
{
	u32 err_status;

	err_status = sec_get_hw_err_status(qm);
	if (err_status & qm->err_info.dev_err.shutdown_mask)
		return true;

	return false;
}

static void sec_disable_axi_error(struct hisi_qm *qm)
{
	struct hisi_qm_err_mask *dev_err = &qm->err_info.dev_err;
	u32 err_mask = dev_err->ce | dev_err->nfe | dev_err->fe;

	writel(err_mask & ~SEC_AXI_ERROR_MASK, qm->io_base + SEC_CORE_INT_MASK);
	writel(dev_err->shutdown_mask & (~SEC_AXI_ERROR_MASK),
	       qm->io_base + SEC_OOO_SHUTDOWN_SEL);
}

static void sec_enable_axi_error(struct hisi_qm *qm)
{
	struct hisi_qm_err_mask *dev_err = &qm->err_info.dev_err;
	u32 err_mask = dev_err->ce | dev_err->nfe | dev_err->fe;

	/* clear axi error source */
	writel(SEC_AXI_ERROR_MASK, qm->io_base + SEC_CORE_INT_SOURCE);

	writel(err_mask, qm->io_base + SEC_CORE_INT_MASK);

	writel(dev_err->shutdown_mask, qm->io_base + SEC_OOO_SHUTDOWN_SEL);
}

static void sec_err_info_init(struct hisi_qm *qm)
{
	struct hisi_qm_err_info *err_info = &qm->err_info;
	struct hisi_qm_err_mask *qm_err = &err_info->qm_err;
	struct hisi_qm_err_mask *dev_err = &err_info->dev_err;

	qm_err->fe = SEC_RAS_FE_ENB_MSK;
	qm_err->ce = hisi_qm_get_hw_info(qm, sec_basic_info, SEC_QM_CE_MASK_CAP,
					 qm->cap_ver);
	qm_err->nfe = hisi_qm_get_hw_info(qm, sec_basic_info,
					  SEC_QM_NFE_MASK_CAP, qm->cap_ver);
	qm_err->shutdown_mask = hisi_qm_get_hw_info(qm, sec_basic_info,
						    SEC_QM_OOO_SHUTDOWN_MASK_CAP,
						    qm->cap_ver);
	qm_err->reset_mask = hisi_qm_get_hw_info(qm, sec_basic_info,
						 SEC_QM_RESET_MASK_CAP,
						 qm->cap_ver);
	qm_err->ecc_2bits_mask = QM_ECC_MBIT;

	dev_err->fe = SEC_RAS_FE_ENB_MSK;
	dev_err->ce = hisi_qm_get_hw_info(qm, sec_basic_info, SEC_CE_MASK_CAP,
					  qm->cap_ver);
	dev_err->nfe = hisi_qm_get_hw_info(qm, sec_basic_info, SEC_NFE_MASK_CAP,
					   qm->cap_ver);
	dev_err->shutdown_mask = hisi_qm_get_hw_info(qm, sec_basic_info,
						     SEC_OOO_SHUTDOWN_MASK_CAP,
						     qm->cap_ver);
	dev_err->reset_mask = hisi_qm_get_hw_info(qm, sec_basic_info,
						  SEC_RESET_MASK_CAP,
						  qm->cap_ver);
	dev_err->ecc_2bits_mask = SEC_CORE_INT_STATUS_M_ECC;

	err_info->msi_wr_port = BIT(0);
	err_info->acpi_rst = "SRST";
}

static const struct hisi_qm_err_ini sec_err_ini = {
	.hw_init		= sec_set_user_domain_and_cache,
	.hw_err_enable		= sec_hw_error_enable,
	.hw_err_disable		= sec_hw_error_disable,
	.get_dev_hw_err_status	= sec_get_hw_err_status,
	.clear_dev_hw_err_status = sec_clear_hw_err_status,
	.open_axi_master_ooo	= sec_open_axi_master_ooo,
	.open_sva_prefetch	= sec_open_sva_prefetch,
	.close_sva_prefetch	= sec_close_sva_prefetch,
	.show_last_dfx_regs	= sec_show_last_dfx_regs,
	.err_info_init		= sec_err_info_init,
	.get_err_result		= sec_get_err_result,
	.dev_is_abnormal	= sec_dev_is_abnormal,
	.disable_axi_error	= sec_disable_axi_error,
	.enable_axi_error	= sec_enable_axi_error,
};

static int sec_pf_probe_init(struct sec_dev *sec)
{
	struct hisi_qm *qm = &sec->qm;
	int ret;

	ret = sec_set_user_domain_and_cache(qm);
	if (ret)
		return ret;

	sec_debug_regs_clear(qm);
	ret = sec_show_last_regs_init(qm);
	if (ret)
		pci_err(qm->pdev, "Failed to init last word regs!\n");

	return ret;
}

static int sec_pre_store_cap_reg(struct hisi_qm *qm)
{
	struct hisi_qm_cap_record *sec_cap;
	struct pci_dev *pdev = qm->pdev;
	size_t i, size;

	size = ARRAY_SIZE(sec_cap_query_info);
	sec_cap = devm_kzalloc(&pdev->dev, sizeof(*sec_cap) * size, GFP_KERNEL);
	if (!sec_cap)
		return -ENOMEM;

	for (i = 0; i < size; i++) {
		sec_cap[i].type = sec_cap_query_info[i].type;
		sec_cap[i].name = sec_cap_query_info[i].name;
		sec_cap[i].cap_val = hisi_qm_get_cap_value(qm,
							   sec_cap_query_info,
							   i, qm->cap_ver);
	}

	qm->cap_tables.dev_cap_table = sec_cap;
	qm->cap_tables.dev_cap_size = size;

	return 0;
}

static int sec_qm_init(struct hisi_qm *qm, struct pci_dev *pdev)
{
	int ret;

	qm->pdev = pdev;
	qm->mode = UACCE_MODE_NOUACCE;
	qm->sqe_size = SEC_SQE_SIZE;
	qm->dev_name = rme_sec_name;
	qm->fun_type = QM_HW_PF;
	qm->qp_base = RME_ACC_PF_DEF_Q_BASE;
	qm->qp_num = RME_ACC_PF_DEF_Q_NUM;
	qm->qm_list = &rme_sec_devices;
	qm->err_ini = &sec_err_ini;

	ret = hisi_qm_init(qm);
	if (ret) {
		pci_err(qm->pdev, "Failed to init sec qm configures!\n");
		return ret;
	}

	if (qm->ver < QM_HW_V4) {
		pci_err(qm->pdev, "RME is not supported in V%#x!\n", qm->ver);
		hisi_qm_uninit(qm);
		return -EINVAL;
	}

	/* Fetch and save the value of capability registers */
	ret = sec_pre_store_cap_reg(qm);
	if (ret) {
		pci_err(qm->pdev, "Failed to pre-store capability registers!\n");
		hisi_qm_uninit(qm);
	}

	return ret;
}

static void sec_qm_uninit(struct hisi_qm *qm)
{
	hisi_qm_uninit(qm);
}

static void sec_probe_uninit(struct hisi_qm *qm)
{
	sec_debug_regs_clear(qm);
	sec_show_last_regs_uninit(qm);
	sec_close_sva_prefetch(qm);
}

int sec_probe(struct pci_dev *pdev)
{
	struct sec_dev *sec;
	struct hisi_qm *qm;
	int ret;

	if (pdev->vendor != PCI_VENDOR_ID_HUAWEI ||
	    pdev->device != PCI_DEVICE_ID_HUAWEI_SEC_PF)
		return -EINVAL;

	sec = devm_kzalloc(&pdev->dev, sizeof(*sec), GFP_KERNEL);
	if (!sec)
		return -ENOMEM;

	qm = &sec->qm;
	set_bit(QM_DRIVER_DOWN, &qm->misc_ctl);
	ret = sec_qm_init(qm, pdev);
	if (ret) {
		pci_err(pdev, "Failed to init SEC QM (%d)!\n", ret);
		return ret;
	}

	ret = sec_pf_probe_init(sec);
	if (ret) {
		pci_err(pdev, "Failed to probe!\n");
		sec_qm_uninit(qm);
		return ret;
	}

	hisi_qm_add_list(qm, &rme_sec_devices);
	/* Device is enabled, clear the flag. */
	clear_bit(QM_DRIVER_DOWN, &qm->misc_ctl);

	ret = sec_debugfs_init(qm);
	if (ret)
		pci_warn(pdev, "Failed to init debugfs!\n");

	pci_info(pdev, "Driver is in RME mode!\n");

	return 0;
}

void sec_remove(struct pci_dev *pdev)
{
	struct hisi_qm *qm;
	int num_vfs;

	if (pdev->vendor != PCI_VENDOR_ID_HUAWEI ||
	    pdev->device != PCI_DEVICE_ID_HUAWEI_SEC_PF)
		return;

	qm = pci_get_drvdata(pdev);
	set_bit(QM_DRIVER_DOWN, &qm->misc_ctl);

	num_vfs = pci_num_vf(pdev);
	if (num_vfs)
		hisi_qm_sriov_configure(pdev, 0);

	sec_debugfs_exit(qm);

	/* Delete device from list after qm_frozen() call in sriov_configure */
	hisi_qm_del_list(qm, &rme_sec_devices);

	sec_probe_uninit(qm);
	sec_qm_uninit(qm);
}
