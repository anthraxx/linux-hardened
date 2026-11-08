// SPDX-License-Identifier: GPL-2.0-only
/*
 * cppc_msr.c:  MSR Interface for CPPC
 * Copyright (c) 2016, Intel Corporation.
 */

#include <linux/limits.h>

#include <acpi/cppc_acpi.h>
#include <asm/msr.h>

/* Refer to drivers/acpi/cppc_acpi.c for the description of functions */

bool cpc_ffh_supported(void)
{
	return true;
}

static bool cpc_ffh_reg_valid(const struct cpc_reg *reg)
{
	return reg->address <= U32_MAX && reg->bit_width &&
	       reg->bit_width <= 64 &&
	       reg->bit_offset <= 64 - reg->bit_width;
}

int cpc_read_ffh(int cpunum, struct cpc_reg *reg, u64 *val)
{
	int err;

	if (!cpc_ffh_reg_valid(reg))
		return -EINVAL;

	err = rdmsrl_safe_on_cpu(cpunum, reg->address, val);
	if (!err) {
		u64 mask = GENMASK_ULL(reg->bit_offset + reg->bit_width - 1,
				       reg->bit_offset);

		*val &= mask;
		*val >>= reg->bit_offset;
	}
	return err;
}

int cpc_write_ffh(int cpunum, struct cpc_reg *reg, u64 val)
{
	u64 rd_val;
	int err;

	if (!cpc_ffh_reg_valid(reg))
		return -EINVAL;

	err = rdmsrl_safe_on_cpu(cpunum, reg->address, &rd_val);
	if (!err) {
		u64 mask = GENMASK_ULL(reg->bit_offset + reg->bit_width - 1,
				       reg->bit_offset);

		val <<= reg->bit_offset;
		val &= mask;
		rd_val &= ~mask;
		rd_val |= val;
		err = wrmsrl_safe_on_cpu(cpunum, reg->address, rd_val);
	}
	return err;
}
