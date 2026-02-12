// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 HiSilicon Limited. */

#include "rme_zip.h"
#include "rme_acc.h"

#define HZIP_CLOCK_GATE_CTRL		0x301004
#define HZIP_DECOMP_CHECK_ENABLE	BIT(16)

#define HZIP_PORT_ARCA_CHE_0		0x301040
#define HZIP_PORT_ARCA_CHE_1		0x301044
#define HZIP_PORT_AWCA_CHE_0		0x301060
#define HZIP_PORT_AWCA_CHE_1		0x301064
#define HZIP_CACHE_ALL_EN		0xffffffff

#define HZIP_BD_RUSER_32_63		0x301110
#define HZIP_SGL_RUSER_32_63		0x30111c
#define HZIP_DATA_RUSER_32_63		0x301128
#define HZIP_DATA_WUSER_32_63		0x301134
#define HZIP_BD_WUSER_32_63		0x301140

#define HZIP_CORE_DFX_BASE		0x301000
#define HZIP_CORE_DFX_DECOMP_BASE	0x304000
#define HZIP_CORE_DFX_COMP_0		0x302000
#define HZIP_CORE_DFX_COMP_1		0x303000
#define HZIP_CORE_DFX_DECOMP_0		0x304000
#define HZIP_CORE_DFX_DECOMP_1		0x305000
#define HZIP_CORE_DFX_DECOMP_2		0x306000
#define HZIP_CORE_DFX_DECOMP_3		0x307000
#define HZIP_CORE_DFX_DECOMP_4		0x308000
#define HZIP_CORE_DFX_DECOMP_5		0x309000
#define HZIP_CORE_REGS_BASE_LEN		0xB0
#define HZIP_CORE_REGS_DFX_LEN		0x28
#define HZIP_CORE_ADDR_INTRVL		0x1000

#define HZIP_CORE_INT_SOURCE		0x3010A0
#define HZIP_CORE_INT_MASK_REG		0x3010A4
#define HZIP_CORE_INT_SET		0x3010A8
#define HZIP_CORE_INT_STATUS		0x3010AC
#define HZIP_CORE_INT_STATUS_M_ECC	BIT(1)
#define HZIP_CORE_SRAM_ECC_ERR_INFO	0x301148
#define HZIP_CORE_INT_RAS_CE_ENB	0x301160
#define HZIP_CORE_INT_RAS_NFE_ENB	0x301164
#define HZIP_CORE_INT_RAS_FE_ENB	0x301168
#define HZIP_CORE_INT_RAS_FE_ENB_MASK	0x0
#define HZIP_OOO_SHUTDOWN_SEL		0x30120C
#define HZIP_SRAM_ECC_ERR_NUM_SHIFT	16
#define HZIP_SRAM_ECC_ERR_ADDR_SHIFT	24
#define HZIP_CORE_INT_MASK_ALL		GENMASK(12, 0)
#define HZIP_AXI_ERROR_MASK		(BIT(2) | BIT(3))
#define HZIP_SQE_SIZE			128

#define HZIP_SOFT_CTRL_CNT_CLR_CE	0x301000
#define HZIP_SOFT_CTRL_CNT_CLR_CE_BIT	BIT(0)
#define HZIP_SOFT_CTRL_ZIP_CONTROL	0x30100C
#define HZIP_AXI_SHUTDOWN_ENABLE	BIT(14)
#define HZIP_WR_PORT			BIT(11)

#define HZIP_BUF_SIZE			22
#define HZIP_SQE_MASK_OFFSET		64
#define HZIP_SQE_MASK_LEN		48

#define HZIP_CNT_CLR_CE_EN		BIT(0)
#define HZIP_RO_CNT_CLR_CE_EN		BIT(2)
#define HZIP_RD_CNT_CLR_CE_EN		(HZIP_CNT_CLR_CE_EN | \
					 HZIP_RO_CNT_CLR_CE_EN)

#define HZIP_PREFETCH_CFG		0x3011B0
#define HZIP_SVA_TRANS			0x3011C4
#define HZIP_PREFETCH_ENABLE		(~(BIT(26) | BIT(17) | BIT(0)))
#define HZIP_SVA_PREFETCH_DISABLE	BIT(26)
#define HZIP_SVA_DISABLE_READY		(BIT(26) | BIT(30))
#define HZIP_SVA_PREFETCH_NUM		GENMASK(18, 16)
#define HZIP_SVA_STALL_NUM		GENMASK(15, 0)
#define HZIP_DELAY_1_US			1
#define HZIP_POLL_TIMEOUT_US		1000
#define HZIP_WAIT_SVA_READY		500000
#define HZIP_READ_SVA_STATUS_TIMES	3
#define HZIP_WAIT_US_MIN		10
#define HZIP_WAIT_US_MAX		20

/* clock gating */
#define HZIP_PEH_CFG_AUTO_GATE		0x3011A8
#define HZIP_PEH_CFG_AUTO_GATE_EN	BIT(0)
#define HZIP_CORE_GATED_EN		GENMASK(15, 8)
#define HZIP_CORE_GATED_OOO_EN		BIT(29)
#define HZIP_CLOCK_GATED_EN		(HZIP_CORE_GATED_EN | \
					 HZIP_CORE_GATED_OOO_EN)

/* zip comp high performance */
#define HZIP_HIGH_PERF_OFFSET		0x301208

enum {
	HZIP_HIGH_COMP_RATE,
	HZIP_HIGH_COMP_PERF,
};

static const char rme_zip_name[] = "rme_zip";

struct hisi_zip_hw_error {
	u32 int_msk;
	const char *msg;
};

static const struct hisi_zip_hw_error zip_hw_error[] = {
	{ .int_msk = BIT(0), .msg = "zip_ecc_1bitt_err" },
	{ .int_msk = BIT(1), .msg = "zip_ecc_2bit_err" },
	{ .int_msk = BIT(2), .msg = "zip_axi_rresp_err" },
	{ .int_msk = BIT(3), .msg = "zip_axi_bresp_err" },
	{ .int_msk = BIT(4), .msg = "zip_src_addr_parse_err" },
	{ .int_msk = BIT(5), .msg = "zip_dst_addr_parse_err" },
	{ .int_msk = BIT(6), .msg = "zip_pre_in_addr_err" },
	{ .int_msk = BIT(7), .msg = "zip_pre_in_data_err" },
	{ .int_msk = BIT(8), .msg = "zip_com_inf_err" },
	{ .int_msk = BIT(9), .msg = "zip_enc_inf_err" },
	{ .int_msk = BIT(10), .msg = "zip_pre_out_err" },
	{ .int_msk = BIT(11), .msg = "zip_axi_poison_err" },
	{ .int_msk = BIT(12), .msg = "zip_sva_err" },
	{ /* sentinel */ }
};

enum ctrl_debug_file_index {
	HZIP_CLEAR_ENABLE,
	HZIP_DEBUG_FILE_NUM,
};

static const char * const ctrl_debug_file_name[] = {
	[HZIP_CLEAR_ENABLE] = "clear_enable",
};

struct ctrl_debug_file {
	enum ctrl_debug_file_index index;
	spinlock_t lock;
	struct hisi_zip_ctrl *ctrl;
};

/*
 * One ZIP controller has one PF and multiple VFs, some global configurations
 * which PF has need this structure.
 *
 * Just relevant for PF.
 */
struct hisi_zip_ctrl {
	struct hisi_zip *hisi_zip;
	struct ctrl_debug_file files[HZIP_DEBUG_FILE_NUM];
};

enum zip_cap_type {
	ZIP_QM_NFE_MASK_CAP = 0x0,
	ZIP_QM_RESET_MASK_CAP,
	ZIP_QM_OOO_SHUTDOWN_MASK_CAP,
	ZIP_QM_CE_MASK_CAP,
	ZIP_NFE_MASK_CAP,
	ZIP_RESET_MASK_CAP,
	ZIP_OOO_SHUTDOWN_MASK_CAP,
	ZIP_CE_MASK_CAP,
	ZIP_CORE_NUM_CAP,
	ZIP_CLUSTER_COMP_NUM_CAP,
	ZIP_DECOMP_ENABLE_BITMAP,
	ZIP_COMP_ENABLE_BITMAP
};

/* For PF device management, not support register to crypto */
static struct hisi_qm_list rme_zip_devices;
struct hisi_qm_list *get_rme_zip_devices(void)
{
	return &rme_zip_devices;
}

static struct hisi_qm_cap_info zip_basic_cap_info[] = {
	{ZIP_QM_NFE_MASK_CAP, 0x3124, 0, GENMASK(31, 0), 0x0, 0x1C57, 0x7C77},
	{ZIP_QM_RESET_MASK_CAP, 0x3128, 0, GENMASK(31, 0), 0x0, 0xC57, 0x6C77},
	{ZIP_QM_OOO_SHUTDOWN_MASK_CAP, 0x3128, 0, GENMASK(31, 0), 0x0, 0x4, 0x6C77},
	{ZIP_QM_CE_MASK_CAP, 0x312C, 0, GENMASK(31, 0), 0x0, 0x8, 0x8},
	{ZIP_NFE_MASK_CAP, 0x3130, 0, GENMASK(31, 0), 0x0, 0x7FE, 0x1FFE},
	{ZIP_RESET_MASK_CAP, 0x3134, 0, GENMASK(31, 0), 0x0, 0x7FE, 0x7FE},
	{ZIP_OOO_SHUTDOWN_MASK_CAP, 0x3134, 0, GENMASK(31, 0), 0x0, 0x2, 0x7FE},
	{ZIP_CE_MASK_CAP, 0x3138, 0, GENMASK(31, 0), 0x0, 0x1, 0x1},
	{ZIP_CORE_NUM_CAP, 0x313C, 16, GENMASK(7, 0), 0x8, 0x8, 0x5},
	{ZIP_CLUSTER_COMP_NUM_CAP, 0x313C, 8, GENMASK(7, 0), 0x2, 0x2, 0x2},
	{ZIP_DECOMP_ENABLE_BITMAP, 0x3140, 16, GENMASK(15, 0), 0xFC, 0xFC, 0x1C},
	{ZIP_COMP_ENABLE_BITMAP, 0x3140, 0, GENMASK(15, 0), 0x3, 0x3, 0x3},
};

static const struct hisi_qm_cap_query_info zip_cap_query_info[] = {
	{ZIP_QM_RAS_NFE_TYPE,  "ZIP_QM_RAS_NFE_TYPE  ", 0x3124, 0x0, 0x1C57, 0x7C77},
	{ZIP_QM_RAS_NFE_RESET, "ZIP_QM_RAS_NFE_RESET ", 0x3128, 0x0, 0xC57, 0x6C77},
	{ZIP_QM_RAS_CE_TYPE,   "ZIP_QM_RAS_CE_TYPE   ", 0x312C, 0x0, 0x8, 0x8},
	{ZIP_RAS_NFE_TYPE,     "ZIP_RAS_NFE_TYPE     ", 0x3130, 0x0, 0x7FE, 0x1FFE},
	{ZIP_RAS_NFE_RESET,    "ZIP_RAS_NFE_RESET    ", 0x3134, 0x0, 0x7FE, 0x7FE},
	{ZIP_RAS_CE_TYPE,      "ZIP_RAS_CE_TYPE      ", 0x3138, 0x0, 0x1, 0x1},
	{ZIP_CORE_INFO,        "ZIP_CORE_INFO        ", 0x313C, 0x12080206, 0x12080206, 0x12050203},
	{ZIP_CORE_EN,          "ZIP_CORE_EN          ", 0x3140, 0xFC0003, 0xFC0003, 0x1C0003},
};

static const struct debugfs_reg32 hzip_dfx_regs[] = {
	{"HZIP_GET_BD_NUM                ",  0x00},
	{"HZIP_GET_RIGHT_BD              ",  0x04},
	{"HZIP_GET_ERROR_BD              ",  0x08},
	{"HZIP_DONE_BD_NUM               ",  0x0c},
	{"HZIP_WORK_CYCLE                ",  0x10},
	{"HZIP_IDLE_CYCLE                ",  0x18},
	{"HZIP_MAX_DELAY                 ",  0x20},
	{"HZIP_MIN_DELAY                 ",  0x24},
	{"HZIP_AVG_DELAY                 ",  0x28},
	{"HZIP_MEM_VISIBLE_DATA          ",  0x30},
	{"HZIP_MEM_VISIBLE_ADDR          ",  0x34},
	{"HZIP_CONSUMED_BYTE             ",  0x38},
	{"HZIP_PRODUCED_BYTE             ",  0x40},
	{"HZIP_COMP_INF                  ",  0x70},
	{"HZIP_PRE_OUT                   ",  0x78},
	{"HZIP_BD_RD                     ",  0x7c},
	{"HZIP_BD_WR                     ",  0x80},
	{"HZIP_GET_BD_AXI_ERR_NUM        ",  0x84},
	{"HZIP_GET_BD_PARSE_ERR_NUM      ",  0x88},
	{"HZIP_ADD_BD_AXI_ERR_NUM        ",  0x8c},
	{"HZIP_DECOMP_STF_RELOAD_CURR_ST ",  0x94},
	{"HZIP_DECOMP_LZ77_CURR_ST       ",  0x9c},
};

static const struct debugfs_reg32 hzip_com_dfx_regs[] = {
	{"HZIP_CLOCK_GATE_CTRL           ",  0x301004},
	{"HZIP_CORE_INT_RAS_CE_ENB       ",  0x301160},
	{"HZIP_CORE_INT_RAS_NFE_ENB      ",  0x301164},
	{"HZIP_CORE_INT_RAS_FE_ENB       ",  0x301168},
	{"HZIP_UNCOM_ERR_RAS_CTRL        ",  0x30116C},
};

static const struct debugfs_reg32 hzip_dump_dfx_regs[] = {
	{"HZIP_GET_BD_NUM                ",  0x00},
	{"HZIP_GET_RIGHT_BD              ",  0x04},
	{"HZIP_GET_ERROR_BD              ",  0x08},
	{"HZIP_DONE_BD_NUM               ",  0x0c},
	{"HZIP_MAX_DELAY                 ",  0x20},
};

/* define the ZIP's dfx regs region and region length */
static struct dfx_diff_registers hzip_diff_regs[] = {
	{
		.reg_offset = HZIP_CORE_DFX_BASE,
		.reg_len = HZIP_CORE_REGS_BASE_LEN,
	}, {
		.reg_offset = HZIP_CORE_DFX_COMP_0,
		.reg_len = HZIP_CORE_REGS_DFX_LEN,
	}, {
		.reg_offset = HZIP_CORE_DFX_COMP_1,
		.reg_len = HZIP_CORE_REGS_DFX_LEN,
	}, {
		.reg_offset = HZIP_CORE_DFX_DECOMP_0,
		.reg_len = HZIP_CORE_REGS_DFX_LEN,
	}, {
		.reg_offset = HZIP_CORE_DFX_DECOMP_1,
		.reg_len = HZIP_CORE_REGS_DFX_LEN,
	}, {
		.reg_offset = HZIP_CORE_DFX_DECOMP_2,
		.reg_len = HZIP_CORE_REGS_DFX_LEN,
	}, {
		.reg_offset = HZIP_CORE_DFX_DECOMP_3,
		.reg_len = HZIP_CORE_REGS_DFX_LEN,
	}, {
		.reg_offset = HZIP_CORE_DFX_DECOMP_4,
		.reg_len = HZIP_CORE_REGS_DFX_LEN,
	}, {
		.reg_offset = HZIP_CORE_DFX_DECOMP_5,
		.reg_len = HZIP_CORE_REGS_DFX_LEN,
	},
};

static int hzip_diff_regs_show(struct seq_file *s, void *unused)
{
	struct hisi_qm *qm = s->private;

	hisi_qm_acc_diff_regs_dump(qm, s, qm->debug.acc_diff_regs,
				   ARRAY_SIZE(hzip_diff_regs));

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(hzip_diff_regs);

static int perf_mode_set(const char *val, const struct kernel_param *kp)
{
	int ret;
	u32 n;

	if (!val)
		return -EINVAL;

	ret = kstrtou32(val, 10, &n);
	if (ret != 0 || (n != HZIP_HIGH_COMP_PERF && n != HZIP_HIGH_COMP_RATE))
		return -EINVAL;

	return param_set_int(val, kp);
}

static const struct kernel_param_ops zip_com_perf_ops = {
	.set = perf_mode_set,
	.get = param_get_int,
};

/*
 * perf_mode = 0 means enable high compression rate mode,
 * perf_mode = 1 means enable high compression performance mode.
 * These two modes only apply to the compression direction.
 */
static u32 perf_mode = HZIP_HIGH_COMP_RATE;
module_param_cb(perf_mode, &zip_com_perf_ops, &perf_mode, 0444);
MODULE_PARM_DESC(perf_mode, "ZIP high perf mode 0(default), 1(enable)");

static void hisi_zip_set_high_perf(struct hisi_qm *qm)
{
	u32 val;

	val = readl_relaxed(qm->io_base + HZIP_HIGH_PERF_OFFSET);
	if (perf_mode == HZIP_HIGH_COMP_PERF)
		val |= HZIP_HIGH_COMP_PERF;
	else
		val &= ~HZIP_HIGH_COMP_PERF;

	/* Set perf mode */
	writel(val, qm->io_base + HZIP_HIGH_PERF_OFFSET);
}

static int hisi_zip_wait_sva_ready(struct hisi_qm *qm, __u32 offset, __u32 mask)
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
		else if (++count == HZIP_READ_SVA_STATUS_TIMES)
			break;

		usleep_range(HZIP_WAIT_US_MIN, HZIP_WAIT_US_MAX);
	} while (++try_times < HZIP_WAIT_SVA_READY);

	if (try_times == HZIP_WAIT_SVA_READY) {
		pci_err(qm->pdev, "Failed to wait sva prefetch ready!\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static void hisi_zip_close_sva_prefetch(struct hisi_qm *qm)
{
	u32 val;
	int ret;

	if (!test_bit(QM_SUPPORT_SVA_PREFETCH, &qm->caps))
		return;

	val = readl_relaxed(qm->io_base + HZIP_PREFETCH_CFG);
	val |= HZIP_SVA_PREFETCH_DISABLE;
	writel(val, qm->io_base + HZIP_PREFETCH_CFG);

	ret = readl_relaxed_poll_timeout(qm->io_base + HZIP_SVA_TRANS,
					 val, !(val & HZIP_SVA_DISABLE_READY),
					 HZIP_DELAY_1_US, HZIP_POLL_TIMEOUT_US);
	if (ret)
		pci_err(qm->pdev, "Failed to close sva prefetch!\n");

	(void)hisi_zip_wait_sva_ready(qm, HZIP_SVA_TRANS, HZIP_SVA_STALL_NUM);
}

static void hisi_zip_open_sva_prefetch(struct hisi_qm *qm)
{
	u32 val;
	int ret;

	if (!test_bit(QM_SUPPORT_SVA_PREFETCH, &qm->caps))
		return;

	/* Enable prefetch */
	val = readl_relaxed(qm->io_base + HZIP_PREFETCH_CFG);
	val &= HZIP_PREFETCH_ENABLE;
	writel(val, qm->io_base + HZIP_PREFETCH_CFG);

	ret = readl_relaxed_poll_timeout(qm->io_base + HZIP_PREFETCH_CFG,
					 val, !(val & HZIP_SVA_PREFETCH_DISABLE),
					 HZIP_DELAY_1_US, HZIP_POLL_TIMEOUT_US);
	if (ret) {
		pci_err(qm->pdev, "Failed to open sva prefetch!\n");
		hisi_zip_close_sva_prefetch(qm);
		return;
	}

	ret = hisi_zip_wait_sva_ready(qm, HZIP_SVA_TRANS, HZIP_SVA_PREFETCH_NUM);
	if (ret)
		hisi_zip_close_sva_prefetch(qm);
}

static void hisi_zip_enable_clock_gate(struct hisi_qm *qm)
{
	u32 val;

	val = readl(qm->io_base + HZIP_CLOCK_GATE_CTRL);
	val |= HZIP_CLOCK_GATED_EN;
	writel(val, qm->io_base + HZIP_CLOCK_GATE_CTRL);

	val = readl(qm->io_base + HZIP_PEH_CFG_AUTO_GATE);
	val |= HZIP_PEH_CFG_AUTO_GATE_EN;
	writel(val, qm->io_base + HZIP_PEH_CFG_AUTO_GATE);
}

static int hisi_zip_set_user_domain_and_cache(struct hisi_qm *qm)
{
	void __iomem *base = qm->io_base;
	u32 dcomp_bm, comp_bm;
	u32 zip_core_en;

	/* qm user domain */
	writel(AXUSER_BASE, base + QM_ARUSER_M_CFG_1);
	writel(ARUSER_M_CFG_ENABLE, base + QM_ARUSER_M_CFG_ENABLE);
	writel(AXUSER_BASE, base + QM_AWUSER_M_CFG_1);
	writel(AWUSER_M_CFG_ENABLE, base + QM_AWUSER_M_CFG_ENABLE);
	writel(WUSER_M_CFG_ENABLE, base + QM_WUSER_M_CFG_ENABLE);

	/* qm cache */
	writel(AXI_M_CFG, base + QM_AXI_M_CFG);
	writel(AXI_M_CFG_ENABLE, base + QM_AXI_M_CFG_ENABLE);

	/* disable FLR triggered by BME(bus master enable) */
	writel(PEH_AXUSER_CFG, base + QM_PEH_AXUSER_CFG);
	writel(PEH_AXUSER_CFG_ENABLE, base + QM_PEH_AXUSER_CFG_ENABLE);

	/* cache */
	writel(HZIP_CACHE_ALL_EN, base + HZIP_PORT_ARCA_CHE_0);
	writel(HZIP_CACHE_ALL_EN, base + HZIP_PORT_ARCA_CHE_1);
	writel(HZIP_CACHE_ALL_EN, base + HZIP_PORT_AWCA_CHE_0);
	writel(HZIP_CACHE_ALL_EN, base + HZIP_PORT_AWCA_CHE_1);

	/* user domain configurations */
	writel(AXUSER_BASE, base + HZIP_BD_RUSER_32_63);
	writel(AXUSER_BASE, base + HZIP_BD_WUSER_32_63);
	writel(AXUSER_BASE, base + HZIP_DATA_RUSER_32_63);
	writel(AXUSER_BASE, base + HZIP_DATA_WUSER_32_63);
	writel(AXUSER_BASE, base + HZIP_SGL_RUSER_32_63);

	hisi_zip_open_sva_prefetch(qm);

	/* let's open all compression/decompression cores */
	zip_core_en = qm->cap_tables.dev_cap_table[ZIP_CORE_EN].cap_val;
	dcomp_bm = (zip_core_en >>
		    zip_basic_cap_info[ZIP_DECOMP_ENABLE_BITMAP].shift) &
		    zip_basic_cap_info[ZIP_DECOMP_ENABLE_BITMAP].mask;
	comp_bm = (zip_core_en >>
		   zip_basic_cap_info[ZIP_COMP_ENABLE_BITMAP].shift) &
		   zip_basic_cap_info[ZIP_COMP_ENABLE_BITMAP].mask;
	writel(HZIP_DECOMP_CHECK_ENABLE | dcomp_bm | comp_bm,
	       base + HZIP_CLOCK_GATE_CTRL);

	/* enable sqc,cqc writeback */
	writel(SQC_CACHE_ENABLE | CQC_CACHE_ENABLE | SQC_CACHE_WB_ENABLE |
	       CQC_CACHE_WB_ENABLE | FIELD_PREP(SQC_CACHE_WB_THRD, 1) |
	       FIELD_PREP(CQC_CACHE_WB_THRD, 1), base + QM_CACHE_CTL);

	hisi_zip_set_high_perf(qm);

	hisi_zip_enable_clock_gate(qm);

	return hisi_dae_set_user_domain(qm);
}

static void hisi_zip_master_ooo_ctrl(struct hisi_qm *qm, bool enable)
{
	u32 val1, val2;

	val1 = readl(qm->io_base + HZIP_SOFT_CTRL_ZIP_CONTROL);
	if (enable) {
		val1 |= HZIP_AXI_SHUTDOWN_ENABLE;
		val2 = qm->err_info.dev_err.shutdown_mask;
	} else {
		val1 &= ~HZIP_AXI_SHUTDOWN_ENABLE;
		val2 = 0x0;
	}

	writel(val2, qm->io_base + HZIP_OOO_SHUTDOWN_SEL);

	writel(val1, qm->io_base + HZIP_SOFT_CTRL_ZIP_CONTROL);
}

static void hisi_zip_hw_error_enable(struct hisi_qm *qm)
{
	struct hisi_qm_err_mask *dev_err = &qm->err_info.dev_err;
	u32 err_mask = dev_err->ce | dev_err->nfe | dev_err->fe;

	/* clear ZIP hw error source if having */
	writel(err_mask, qm->io_base + HZIP_CORE_INT_SOURCE);

	/* configure error type */
	writel(dev_err->ce, qm->io_base + HZIP_CORE_INT_RAS_CE_ENB);
	writel(dev_err->fe, qm->io_base + HZIP_CORE_INT_RAS_FE_ENB);
	writel(dev_err->nfe, qm->io_base + HZIP_CORE_INT_RAS_NFE_ENB);

	hisi_zip_master_ooo_ctrl(qm, true);

	/* enable ZIP hw error interrupts */
	writel(~err_mask, qm->io_base + HZIP_CORE_INT_MASK_REG);

	hisi_dae_hw_error_enable(qm);
}

static void hisi_zip_hw_error_disable(struct hisi_qm *qm)
{
	struct hisi_qm_err_mask *dev_err = &qm->err_info.dev_err;
	u32 err_mask = dev_err->ce | dev_err->nfe | dev_err->fe;

	/* disable ZIP hw error interrupts */
	writel(err_mask, qm->io_base + HZIP_CORE_INT_MASK_REG);

	hisi_zip_master_ooo_ctrl(qm, false);

	hisi_dae_hw_error_disable(qm);
}

static inline struct hisi_qm *file_to_qm(struct ctrl_debug_file *file)
{
	struct hisi_zip *hisi_zip = file->ctrl->hisi_zip;

	return &hisi_zip->qm;
}

static u32 clear_enable_read(struct hisi_qm *qm)
{
	return readl(qm->io_base + HZIP_SOFT_CTRL_CNT_CLR_CE) &
		     HZIP_SOFT_CTRL_CNT_CLR_CE_BIT;
}

static int clear_enable_write(struct hisi_qm *qm, u32 val)
{
	u32 tmp;

	if (val != 1 && val != 0)
		return -EINVAL;

	tmp = (readl(qm->io_base + HZIP_SOFT_CTRL_CNT_CLR_CE) &
	       ~HZIP_SOFT_CTRL_CNT_CLR_CE_BIT) | val;
	writel(tmp, qm->io_base + HZIP_SOFT_CTRL_CNT_CLR_CE);

	return  0;
}

static ssize_t hisi_zip_ctrl_debug_read(struct file *filp, char __user *buf,
					size_t count, loff_t *pos)
{
	struct ctrl_debug_file *file = filp->private_data;
	struct hisi_qm *qm = file_to_qm(file);
	char tbuf[HZIP_BUF_SIZE];
	u32 val;
	int ret;

	ret = hisi_qm_get_dfx_access(qm);
	if (ret)
		return ret;

	spin_lock_irq(&file->lock);
	switch (file->index) {
	case HZIP_CLEAR_ENABLE:
		val = clear_enable_read(qm);
		break;
	default:
		goto err_input;
	}
	spin_unlock_irq(&file->lock);

	hisi_qm_put_dfx_access(qm);
	ret = scnprintf(tbuf, sizeof(tbuf), "%u\n", val);
	return simple_read_from_buffer(buf, count, pos, tbuf, ret);

err_input:
	spin_unlock_irq(&file->lock);
	hisi_qm_put_dfx_access(qm);
	return -EINVAL;
}

static ssize_t hisi_zip_ctrl_debug_write(struct file *filp,
					 const char __user *buf,
					 size_t count, loff_t *pos)
{
	struct ctrl_debug_file *file = filp->private_data;
	struct hisi_qm *qm = file_to_qm(file);
	char tbuf[HZIP_BUF_SIZE];
	unsigned long val;
	int len, ret;

	if (*pos != 0)
		return 0;

	if (count >= HZIP_BUF_SIZE)
		return -ENOSPC;

	len = simple_write_to_buffer(tbuf, HZIP_BUF_SIZE - 1, pos, buf, count);
	if (len < 0)
		return len;

	tbuf[len] = '\0';
	ret = kstrtoul(tbuf, 0, &val);
	if (ret)
		return -EFAULT;

	ret = hisi_qm_get_dfx_access(qm);
	if (ret)
		return ret;

	spin_lock_irq(&file->lock);
	switch (file->index) {
	case HZIP_CLEAR_ENABLE:
		ret = clear_enable_write(qm, val);
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

static const struct file_operations ctrl_debug_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = hisi_zip_ctrl_debug_read,
	.write = hisi_zip_ctrl_debug_write,
};

static int hisi_zip_regs_show(struct seq_file *s, void *unused)
{
	hisi_qm_regs_dump(s, s->private);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(hisi_zip_regs);

static void __iomem *get_zip_core_addr(struct hisi_qm *qm, int core_num)
{
	u8 zip_comp_core_num;
	u32 zip_core_info;

	zip_core_info = qm->cap_tables.dev_cap_table[ZIP_CORE_INFO].cap_val;
	zip_comp_core_num = (zip_core_info >>
			     zip_basic_cap_info[ZIP_CLUSTER_COMP_NUM_CAP].shift) &
			     zip_basic_cap_info[ZIP_CLUSTER_COMP_NUM_CAP].mask;

	if (core_num < zip_comp_core_num)
		return qm->io_base + HZIP_CORE_DFX_BASE +
		       (core_num + 1) * HZIP_CORE_ADDR_INTRVL;

	return qm->io_base + HZIP_CORE_DFX_DECOMP_BASE +
	       (core_num - zip_comp_core_num) * HZIP_CORE_ADDR_INTRVL;
}

static int hisi_zip_core_debug_init(struct hisi_qm *qm)
{
	u32 zip_core_num, zip_comp_core_num;
	struct device *dev = &qm->pdev->dev;
	struct debugfs_regset32 *regset;
	u32 zip_core_info;
	struct dentry *tmp_d;
	char buf[HZIP_BUF_SIZE];
	int i;

	zip_core_info =  qm->cap_tables.dev_cap_table[ZIP_CORE_INFO].cap_val;
	zip_core_num = (zip_core_info >>
			zip_basic_cap_info[ZIP_CORE_NUM_CAP].shift) &
			zip_basic_cap_info[ZIP_CORE_NUM_CAP].mask;
	zip_comp_core_num = (zip_core_info >>
			     zip_basic_cap_info[ZIP_CLUSTER_COMP_NUM_CAP].shift) &
			     zip_basic_cap_info[ZIP_CLUSTER_COMP_NUM_CAP].mask;

	for (i = 0; i < zip_core_num; i++) {
		if (i < zip_comp_core_num)
			scnprintf(buf, sizeof(buf), "comp_core%d", i);
		else
			scnprintf(buf, sizeof(buf), "decomp_core%d",
				  i - zip_comp_core_num);

		regset = devm_kzalloc(dev, sizeof(*regset), GFP_KERNEL);
		if (!regset)
			return -ENOMEM;

		regset->regs = hzip_dfx_regs;
		regset->nregs = ARRAY_SIZE(hzip_dfx_regs);
		regset->base = get_zip_core_addr(qm, i);
		regset->dev = dev;

		tmp_d = debugfs_create_dir(buf, qm->debug.debug_root);
		debugfs_create_file("regs", 0444, tmp_d, regset,
				    &hisi_zip_regs_fops);
	}

	return 0;
}

static int zip_cap_regs_show(struct seq_file *s, void *unused)
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

DEFINE_SHOW_ATTRIBUTE(zip_cap_regs);

static void hisi_zip_dfx_debug_init(struct hisi_qm *qm)
{
	struct dfx_diff_registers *hzip_regs = qm->debug.acc_diff_regs;
	struct dentry *tmp_dir;

	tmp_dir = debugfs_create_dir("zip_dfx", qm->debug.debug_root);

	if (hzip_regs)
		debugfs_create_file("diff_regs", 0444, tmp_dir, qm,
				    &hzip_diff_regs_fops);

	debugfs_create_file("cap_regs", 0444, qm->debug.debug_root, qm,
			    &zip_cap_regs_fops);
}

static int hisi_zip_ctrl_debug_init(struct hisi_qm *qm)
{
	struct hisi_zip *zip = container_of(qm, struct hisi_zip, qm);
	int i;

	for (i = HZIP_CLEAR_ENABLE; i < HZIP_DEBUG_FILE_NUM; i++) {
		spin_lock_init(&zip->ctrl->files[i].lock);
		zip->ctrl->files[i].ctrl = zip->ctrl;
		zip->ctrl->files[i].index = i;

		debugfs_create_file(ctrl_debug_file_name[i], 0600,
				    qm->debug.debug_root,
				    zip->ctrl->files + i,
				    &ctrl_debug_fops);
	}

	return hisi_zip_core_debug_init(qm);
}

static int hisi_zip_debugfs_init(struct hisi_qm *qm)
{
	struct device *dev = &qm->pdev->dev;
	struct dentry *root_debugfs;
	int ret;

	ret = hisi_qm_regs_debugfs_init(qm, hzip_diff_regs,
					ARRAY_SIZE(hzip_diff_regs));
	if (ret) {
		dev_warn(dev, "Failed to init ZIP diff regs!\n");
		return ret;
	}

	qm->debug.sqe_mask_offset = HZIP_SQE_MASK_OFFSET;
	qm->debug.sqe_mask_len = HZIP_SQE_MASK_LEN;

	root_debugfs = get_rme_root_debugfs(qm->pdev);
	if (!root_debugfs)
		dev_warn(dev, "Fail to get parent debugfs, dev_name replace!\n");
	qm->debug.debug_root = debugfs_create_dir(dev_name(dev), root_debugfs);

	/* As we don't support qos yet, so don't need to create debug file */
	if (test_bit(QM_SUPPORT_FUNC_QOS, &qm->caps)) {
		clear_bit(QM_SUPPORT_FUNC_QOS, &qm->caps);
		hisi_qm_debug_init(qm);
		set_bit(QM_SUPPORT_FUNC_QOS, &qm->caps);
	} else {
		hisi_qm_debug_init(qm);
	}

	ret = hisi_zip_ctrl_debug_init(qm);
	if (ret)
		goto debugfs_remove;

	hisi_zip_dfx_debug_init(qm);

	return 0;

debugfs_remove:
	debugfs_remove_recursive(qm->debug.debug_root);
	hisi_qm_regs_debugfs_uninit(qm, ARRAY_SIZE(hzip_diff_regs));
	return ret;
}

/* hisi_zip_debug_regs_clear() - clear the zip debug regs */
static void hisi_zip_debug_regs_clear(struct hisi_qm *qm)
{
	u32 zip_core_info;
	u8 zip_core_num;
	int i, j;

	zip_core_info = qm->cap_tables.dev_cap_table[ZIP_CORE_INFO].cap_val;
	zip_core_num = (zip_core_info >>
			zip_basic_cap_info[ZIP_CORE_NUM_CAP].shift) &
			zip_basic_cap_info[ZIP_CORE_NUM_CAP].mask;

	/* enable register read_clear bit */
	writel(HZIP_RD_CNT_CLR_CE_EN, qm->io_base + HZIP_SOFT_CTRL_CNT_CLR_CE);
	for (i = 0; i < zip_core_num; i++)
		for (j = 0; j < ARRAY_SIZE(hzip_dfx_regs); j++)
			readl(get_zip_core_addr(qm, i) +
			      hzip_dfx_regs[j].offset);

	/* disable register read_clear bit */
	writel(0x0, qm->io_base + HZIP_SOFT_CTRL_CNT_CLR_CE);

	hisi_qm_debug_regs_clear(qm);
}

static void hisi_zip_debugfs_exit(struct hisi_qm *qm)
{
	debugfs_remove_recursive(qm->debug.debug_root);

	hisi_qm_regs_debugfs_uninit(qm, ARRAY_SIZE(hzip_diff_regs));

	hisi_zip_debug_regs_clear(qm);
}


static int hisi_zip_show_last_regs_init(struct hisi_qm *qm)
{
	int core_dfx_regs_num =  ARRAY_SIZE(hzip_dump_dfx_regs);
	int com_dfx_regs_num = ARRAY_SIZE(hzip_com_dfx_regs);
	struct qm_debug *debug = &qm->debug;
	void __iomem *io_base;
	u32 zip_core_info;
	u32 zip_core_num;
	int i, j, idx;

	zip_core_info = qm->cap_tables.dev_cap_table[ZIP_CORE_INFO].cap_val;
	zip_core_num = (zip_core_info >> zip_basic_cap_info[ZIP_CORE_NUM_CAP].shift) &
			zip_basic_cap_info[ZIP_CORE_NUM_CAP].mask;

	debug->last_words = kcalloc(core_dfx_regs_num * zip_core_num + com_dfx_regs_num,
				    sizeof(unsigned int), GFP_KERNEL);
	if (!debug->last_words)
		return -ENOMEM;

	for (i = 0; i < com_dfx_regs_num; i++) {
		io_base = qm->io_base + hzip_com_dfx_regs[i].offset;
		debug->last_words[i] = readl_relaxed(io_base);
	}

	for (i = 0; i < zip_core_num; i++) {
		io_base = get_zip_core_addr(qm, i);
		for (j = 0; j < core_dfx_regs_num; j++) {
			idx = com_dfx_regs_num + i * core_dfx_regs_num + j;
			debug->last_words[idx] = readl_relaxed(io_base +
							       hzip_dump_dfx_regs[j].offset);
		}
	}

	return 0;
}

static void hisi_zip_show_last_regs_uninit(struct hisi_qm *qm)
{
	struct qm_debug *debug = &qm->debug;

	if (!debug->last_words)
		return;

	kfree(debug->last_words);
	debug->last_words = NULL;
}

static void hisi_zip_show_last_dfx_regs(struct hisi_qm *qm)
{
	int core_dfx_regs_num =  ARRAY_SIZE(hzip_dump_dfx_regs);
	int com_dfx_regs_num = ARRAY_SIZE(hzip_com_dfx_regs);
	u32 zip_core_num, zip_comp_core_num;
	struct qm_debug *debug = &qm->debug;
	char buf[HZIP_BUF_SIZE];
	u32 zip_core_info;
	void __iomem *base;
	int i, j, idx;
	u32 val;

	if (!debug->last_words)
		return;

	for (i = 0; i < com_dfx_regs_num; i++) {
		val = readl_relaxed(qm->io_base + hzip_com_dfx_regs[i].offset);
		if (debug->last_words[i] != val)
			pci_info(qm->pdev, "com_dfx: %s \t= 0x%08x => 0x%08x\n",
				 hzip_com_dfx_regs[i].name, debug->last_words[i], val);
	}

	zip_core_info = qm->cap_tables.dev_cap_table[ZIP_CORE_INFO].cap_val;
	zip_core_num = (zip_core_info >>
			zip_basic_cap_info[ZIP_CORE_NUM_CAP].shift) &
		       zip_basic_cap_info[ZIP_CORE_NUM_CAP].mask;
	zip_comp_core_num = (zip_core_info >>
			     zip_basic_cap_info[ZIP_CLUSTER_COMP_NUM_CAP].shift) &
			    zip_basic_cap_info[ZIP_CLUSTER_COMP_NUM_CAP].mask;

	for (i = 0; i < zip_core_num; i++) {
		if (i < zip_comp_core_num)
			scnprintf(buf, sizeof(buf), "Comp_core-%d", i);
		else
			scnprintf(buf, sizeof(buf), "Decomp_core-%d",
				  i - zip_comp_core_num);
		base = get_zip_core_addr(qm, i);

		pci_info(qm->pdev, "==>%s:\n", buf);
		/* dump last word for dfx regs during control resetting */
		for (j = 0; j < core_dfx_regs_num; j++) {
			idx = com_dfx_regs_num + i * core_dfx_regs_num + j;
			val = readl_relaxed(base + hzip_dump_dfx_regs[j].offset);
			if (debug->last_words[idx] != val)
				pci_info(qm->pdev, "%s \t= 0x%08x => 0x%08x\n",
					 hzip_dump_dfx_regs[j].name,
					 debug->last_words[idx], val);
		}
	}
}

static void hisi_zip_log_hw_error(struct hisi_qm *qm, u32 err_sts)
{
	const struct hisi_zip_hw_error *err = zip_hw_error;
	struct device *dev = &qm->pdev->dev;
	u32 err_val;

	while (err->msg) {
		if (err->int_msk & err_sts) {
			dev_err(dev, "%s [error status=0x%x] found\n",
				err->msg, err->int_msk);

			if (err->int_msk & HZIP_CORE_INT_STATUS_M_ECC) {
				err_val = readl(qm->io_base +
						HZIP_CORE_SRAM_ECC_ERR_INFO);
				dev_err(dev, "hisi-zip multi ecc sram num=0x%x\n",
					((err_val >>
					HZIP_SRAM_ECC_ERR_NUM_SHIFT) & 0xFF));
			}
		}
		err++;
	}
}

static u32 hisi_zip_get_hw_err_status(struct hisi_qm *qm)
{
	return readl(qm->io_base + HZIP_CORE_INT_STATUS);
}

static void hisi_zip_clear_hw_err_status(struct hisi_qm *qm, u32 err_sts)
{
	writel(err_sts, qm->io_base + HZIP_CORE_INT_SOURCE);
}

static void hisi_zip_disable_error_report(struct hisi_qm *qm, u32 err_type)
{
	u32 nfe_mask = qm->err_info.dev_err.nfe;

	writel(nfe_mask & (~err_type), qm->io_base + HZIP_CORE_INT_RAS_NFE_ENB);
}

static void hisi_zip_open_axi_master_ooo(struct hisi_qm *qm)
{
	u32 val;

	val = readl(qm->io_base + HZIP_SOFT_CTRL_ZIP_CONTROL);

	writel(val & ~HZIP_AXI_SHUTDOWN_ENABLE,
	       qm->io_base + HZIP_SOFT_CTRL_ZIP_CONTROL);

	writel(val | HZIP_AXI_SHUTDOWN_ENABLE,
	       qm->io_base + HZIP_SOFT_CTRL_ZIP_CONTROL);

	hisi_dae_open_axi_master_ooo(qm);
}

static void hisi_zip_close_axi_master_ooo(struct hisi_qm *qm)
{
	u32 nfe_enb;

	/* Disable ECC Mbit error report. */
	nfe_enb = readl(qm->io_base + HZIP_CORE_INT_RAS_NFE_ENB);
	writel(nfe_enb & ~HZIP_CORE_INT_STATUS_M_ECC,
	       qm->io_base + HZIP_CORE_INT_RAS_NFE_ENB);

	/* Inject zip ECC Mbit error to block master ooo. */
	writel(HZIP_CORE_INT_STATUS_M_ECC,
	       qm->io_base + HZIP_CORE_INT_SET);
}

static enum acc_err_result hisi_zip_get_err_result(struct hisi_qm *qm)
{
	enum acc_err_result zip_result = ACC_ERR_NONE;
	enum acc_err_result dae_result;
	u32 err_status;

	/* Get device hardware new error status */
	err_status = hisi_zip_get_hw_err_status(qm);
	if (err_status) {
		if (err_status & qm->err_info.dev_err.ecc_2bits_mask)
			qm->err_status.is_dev_ecc_mbit = true;
		hisi_zip_log_hw_error(qm, err_status);

		if (err_status & qm->err_info.dev_err.reset_mask) {
			/* Disable the same error reporting until device is recovered. */
			hisi_zip_disable_error_report(qm, err_status);
			zip_result = ACC_ERR_NEED_RESET;
		} else {
			hisi_zip_clear_hw_err_status(qm, err_status);
		}
	}

	dae_result = hisi_dae_get_err_result(qm);

	return (zip_result == ACC_ERR_NEED_RESET ||
		dae_result == ACC_ERR_NEED_RESET) ?
		ACC_ERR_NEED_RESET : ACC_ERR_RECOVERED;
}

static bool hisi_zip_dev_is_abnormal(struct hisi_qm *qm)
{
	u32 err_status;

	err_status = hisi_zip_get_hw_err_status(qm);
	if (err_status & qm->err_info.dev_err.shutdown_mask)
		return true;

	return hisi_dae_dev_is_abnormal(qm);
}

static int hisi_zip_set_priv_status(struct hisi_qm *qm)
{
	return hisi_dae_close_axi_master_ooo(qm);
}

static void hisi_zip_disable_axi_error(struct hisi_qm *qm)
{
	struct hisi_qm_err_mask *dev_err = &qm->err_info.dev_err;
	u32 err_mask = dev_err->ce | dev_err->nfe | dev_err->fe;
	u32 val;

	val = ~(err_mask & (~HZIP_AXI_ERROR_MASK));
	writel(val, qm->io_base + HZIP_CORE_INT_MASK_REG);

	writel(dev_err->shutdown_mask & (~HZIP_AXI_ERROR_MASK),
	       qm->io_base + HZIP_OOO_SHUTDOWN_SEL);
}

static void hisi_zip_enable_axi_error(struct hisi_qm *qm)
{
	struct hisi_qm_err_mask *dev_err = &qm->err_info.dev_err;
	u32 err_mask = dev_err->ce | dev_err->nfe | dev_err->fe;

	/* clear axi error source */
	writel(HZIP_AXI_ERROR_MASK, qm->io_base + HZIP_CORE_INT_SOURCE);

	writel(~err_mask, qm->io_base + HZIP_CORE_INT_MASK_REG);

	writel(dev_err->shutdown_mask, qm->io_base + HZIP_OOO_SHUTDOWN_SEL);
}

static void hisi_zip_err_info_init(struct hisi_qm *qm)
{
	struct hisi_qm_err_info *err_info = &qm->err_info;
	struct hisi_qm_err_mask *qm_err = &err_info->qm_err;
	struct hisi_qm_err_mask *dev_err = &err_info->dev_err;

	qm_err->fe = HZIP_CORE_INT_RAS_FE_ENB_MASK;
	qm_err->ce = hisi_qm_get_hw_info(qm, zip_basic_cap_info,
					 ZIP_QM_CE_MASK_CAP, qm->cap_ver);
	qm_err->nfe = hisi_qm_get_hw_info(qm, zip_basic_cap_info,
					  ZIP_QM_NFE_MASK_CAP, qm->cap_ver);
	qm_err->ecc_2bits_mask = QM_ECC_MBIT;
	qm_err->reset_mask = hisi_qm_get_hw_info(qm, zip_basic_cap_info,
						 ZIP_QM_RESET_MASK_CAP,
						 qm->cap_ver);
	qm_err->shutdown_mask = hisi_qm_get_hw_info(qm, zip_basic_cap_info,
						    ZIP_QM_OOO_SHUTDOWN_MASK_CAP,
						    qm->cap_ver);

	dev_err->fe = HZIP_CORE_INT_RAS_FE_ENB_MASK;
	dev_err->ce = hisi_qm_get_hw_info(qm, zip_basic_cap_info,
					  ZIP_CE_MASK_CAP, qm->cap_ver);
	dev_err->nfe = hisi_qm_get_hw_info(qm, zip_basic_cap_info,
					   ZIP_NFE_MASK_CAP, qm->cap_ver);
	dev_err->ecc_2bits_mask = HZIP_CORE_INT_STATUS_M_ECC;
	dev_err->shutdown_mask = hisi_qm_get_hw_info(qm, zip_basic_cap_info,
						     ZIP_OOO_SHUTDOWN_MASK_CAP,
						     qm->cap_ver);
	dev_err->reset_mask = hisi_qm_get_hw_info(qm, zip_basic_cap_info,
						  ZIP_RESET_MASK_CAP,
						  qm->cap_ver);

	err_info->msi_wr_port = HZIP_WR_PORT;
	err_info->acpi_rst = "ZRST";
}

static const struct hisi_qm_err_ini hisi_zip_err_ini = {
	.hw_init		= hisi_zip_set_user_domain_and_cache,
	.hw_err_enable		= hisi_zip_hw_error_enable,
	.hw_err_disable		= hisi_zip_hw_error_disable,
	.get_dev_hw_err_status	= hisi_zip_get_hw_err_status,
	.clear_dev_hw_err_status = hisi_zip_clear_hw_err_status,
	.open_axi_master_ooo	= hisi_zip_open_axi_master_ooo,
	.close_axi_master_ooo	= hisi_zip_close_axi_master_ooo,
	.open_sva_prefetch	= hisi_zip_open_sva_prefetch,
	.close_sva_prefetch	= hisi_zip_close_sva_prefetch,
	.show_last_dfx_regs	= hisi_zip_show_last_dfx_regs,
	.err_info_init		= hisi_zip_err_info_init,
	.get_err_result		= hisi_zip_get_err_result,
	.dev_is_abnormal	= hisi_zip_dev_is_abnormal,
	.set_priv_status	= hisi_zip_set_priv_status,
	.disable_axi_error	= hisi_zip_disable_axi_error,
	.enable_axi_error	= hisi_zip_enable_axi_error,
};

static int hisi_zip_pf_probe_init(struct hisi_zip *hisi_zip)
{
	struct hisi_qm *qm = &hisi_zip->qm;
	struct hisi_zip_ctrl *ctrl;
	int ret;

	ctrl = devm_kzalloc(&qm->pdev->dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	hisi_zip->ctrl = ctrl;
	ctrl->hisi_zip = hisi_zip;

	ret = hisi_zip_set_user_domain_and_cache(qm);
	if (ret)
		return ret;

	hisi_zip_debug_regs_clear(qm);

	ret = hisi_zip_show_last_regs_init(qm);
	if (ret)
		pci_err(qm->pdev, "Failed to init last word regs!\n");

	return ret;
}

static int zip_pre_store_cap_reg(struct hisi_qm *qm)
{
	struct hisi_qm_cap_record *zip_cap;
	struct pci_dev *pdev = qm->pdev;
	size_t i, size;

	size = ARRAY_SIZE(zip_cap_query_info);
	zip_cap = devm_kzalloc(&pdev->dev, sizeof(*zip_cap) * size, GFP_KERNEL);
	if (!zip_cap)
		return -ENOMEM;

	for (i = 0; i < size; i++) {
		zip_cap[i].type = zip_cap_query_info[i].type;
		zip_cap[i].name = zip_cap_query_info[i].name;
		zip_cap[i].cap_val = hisi_qm_get_cap_value(qm,
							   zip_cap_query_info,
							   i, qm->cap_ver);
	}

	qm->cap_tables.dev_cap_table = zip_cap;
	qm->cap_tables.dev_cap_size = size;

	return 0;
}

static int hisi_zip_qm_init(struct hisi_qm *qm, struct pci_dev *pdev)
{
	int ret;

	qm->pdev = pdev;
	qm->mode = UACCE_MODE_NOUACCE;
	qm->sqe_size = HZIP_SQE_SIZE;
	qm->dev_name = rme_zip_name;
	qm->fun_type = QM_HW_PF;
	qm->qp_base = RME_ACC_PF_DEF_Q_BASE;
	qm->qp_num = RME_ACC_PF_DEF_Q_NUM;
	qm->qm_list = &rme_zip_devices;
	qm->err_ini = &hisi_zip_err_ini;

	ret = hisi_qm_init(qm);
	if (ret) {
		pci_err(qm->pdev, "Failed to init zip qm configures!\n");
		return ret;
	}

	if (qm->ver < QM_HW_V4) {
		pci_err(qm->pdev, "RME is not supported in V%#x!\n", qm->ver);
		hisi_qm_uninit(qm);
		return -EINVAL;
	}

	/* Fetch and save the value of capability registers */
	ret = zip_pre_store_cap_reg(qm);
	if (ret) {
		pci_err(qm->pdev, "Failed to pre-store capability registers!\n");
		hisi_qm_uninit(qm);
		return ret;
	}

	return 0;
}

static void hisi_zip_qm_uninit(struct hisi_qm *qm)
{
	hisi_qm_uninit(qm);
}

static void hisi_zip_probe_uninit(struct hisi_qm *qm)
{
	hisi_zip_show_last_regs_uninit(qm);
	hisi_zip_close_sva_prefetch(qm);
}

int zip_probe(struct pci_dev *pdev)
{
	struct hisi_zip *hisi_zip;
	struct hisi_qm *qm;
	int ret;

	if (pdev->vendor != PCI_VENDOR_ID_HUAWEI ||
	    pdev->device != PCI_DEVICE_ID_HUAWEI_ZIP_PF)
		return -EINVAL;

	hisi_zip = devm_kzalloc(&pdev->dev, sizeof(*hisi_zip), GFP_KERNEL);
	if (!hisi_zip)
		return -ENOMEM;

	qm = &hisi_zip->qm;
	set_bit(QM_DRIVER_DOWN, &qm->misc_ctl);
	ret = hisi_zip_qm_init(qm, pdev);
	if (ret) {
		pci_err(pdev, "Failed to init ZIP QM (%d)!\n", ret);
		return ret;
	}

	ret = hisi_zip_pf_probe_init(hisi_zip);
	if (ret) {
		pci_err(pdev, "Failed to probe (%d)!\n", ret);
		hisi_zip_qm_uninit(qm);
		return ret;
	}

	hisi_qm_add_list(qm, &rme_zip_devices);
	/* Device is enabled, clear the flag. */
	clear_bit(QM_DRIVER_DOWN, &qm->misc_ctl);

	ret = hisi_zip_debugfs_init(qm);
	if (ret)
		pci_err(pdev, "Failed to init debugfs (%d)!\n", ret);

	pci_info(pdev, "Driver is in RME mode!\n");

	return 0;
}

void zip_remove(struct pci_dev *pdev)
{
	struct hisi_qm *qm;
	int num_vfs;

	if (pdev->vendor != PCI_VENDOR_ID_HUAWEI ||
	    pdev->device != PCI_DEVICE_ID_HUAWEI_ZIP_PF)
		return;

	qm = pci_get_drvdata(pdev);
	set_bit(QM_DRIVER_DOWN, &qm->misc_ctl);

	num_vfs = pci_num_vf(pdev);
	if (num_vfs)
		hisi_qm_sriov_configure(pdev, 0);

	hisi_zip_debugfs_exit(qm);

	/* Delete device from list after qm_frozen() call in sriov_configure */
	hisi_qm_del_list(qm, &rme_zip_devices);

	hisi_zip_probe_uninit(qm);
	hisi_zip_qm_uninit(qm);
}
