// SPDX-License-Identifier: GPL-2.0-only
/*
 * Marvell Fibre Channel HBA Driver
 * Copyright (c)  2023     Marvell
 */

#include <linux/module.h>

static int __init tcm_qla2xxx_init(void)
{
	return 0;
}

static void __exit tcm_qla2xxx_exit(void)
{
}
MODULE_DESCRIPTION("Stub driver to override inbox tcm_qla2xxx.ko driver");
MODULE_LICENSE("GPL");
module_init(tcm_qla2xxx_init);
module_exit(tcm_qla2xxx_exit);
