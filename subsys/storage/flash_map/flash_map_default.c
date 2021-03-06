/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT fixed_partitions

#include <zephyr.h>
#include <storage/flash_map.h>

#if USE_PARTITION_MANAGER

/**
 * Have flash_map_default use Partition Manager information instead of
 * DeviceTree information when we are using the Partition Manager.
 */

#include <pm_config.h>
#include <sys/util.h>

#define FLASH_MAP_OFFSET(i) UTIL_CAT(PM_, UTIL_CAT(PM_##i##_LABEL, _ADDRESS))
#define FLASH_MAP_DEV(i)    UTIL_CAT(PM_, UTIL_CAT(PM_##i##_LABEL, _DEV_NAME))
#define FLASH_MAP_SIZE(i)   UTIL_CAT(PM_, UTIL_CAT(PM_##i##_LABEL, _SIZE))
#define FLASH_MAP_NUM       PM_NUM

#define FLASH_AREA_FOO(i, _)                \
	{                                       \
		.fa_id       = i,                   \
		.fa_off      = FLASH_MAP_OFFSET(i), \
		.fa_dev_name = FLASH_MAP_DEV(i),    \
		.fa_size     = FLASH_MAP_SIZE(i)    \
	},

const struct flash_area default_flash_map[] = {
	UTIL_LISTIFY(FLASH_MAP_NUM, FLASH_AREA_FOO, ~)
};

#else

/* Get the grand parent of a node */
#define GPARENT(node_id) DT_PARENT(DT_PARENT(node_id))

/* if 'soc-nv-flash' return controller label or punt to _else_code */
#define IS_SOC_NV_FLASH(part, _else_code) \
	COND_CODE_1(DT_NODE_HAS_COMPAT(GPARENT(part), soc_nv_flash), \
		    (DT_LABEL(DT_PARENT(GPARENT(part)))), (_else_code))

/* if 'jedec,spi-nor' return controller label or punt to _else_code */
#define IS_JEDEC_SPI_NOR(part, _else_code) \
	COND_CODE_1(DT_NODE_HAS_COMPAT(GPARENT(part), jedec_spi_nor), \
		    (DT_LABEL(GPARENT(part))), (_else_code))

/* if 'nordic,qspi-nor' return controller label or punt to _else_code */
#define IS_NORDIC_QSPI_NOR(part, _else_code) \
	COND_CODE_1(DT_NODE_HAS_COMPAT(GPARENT(part), nordic_qspi_nor), \
		    (DT_LABEL(GPARENT(part))), (_else_code))

/* return flash controller label based on matching compatible or NULL */
#define DT_FLASH_DEV_FROM_PART(part) \
	IS_SOC_NV_FLASH(part, \
	IS_JEDEC_SPI_NOR(part, \
	IS_NORDIC_QSPI_NOR(part, NULL)))

#define FLASH_AREA_FOO(part)				\
	{.fa_id = DT_FIXED_PARTITION_ID(part),		\
	 .fa_off = DT_REG_ADDR(part),			\
	 .fa_dev_name = DT_FLASH_DEV_FROM_PART(part),	\
	 .fa_size = DT_REG_SIZE(part),},

#define FOREACH_PARTION(n) DT_FOREACH_CHILD(DT_DRV_INST(n), FLASH_AREA_FOO)

/* We iterate over all compatible 'fixed-partions' nodes and
 * use DT_FOREACH_CHILD to iterate over all the partitions for that
 * 'fixed-partions' node.  This way we build a global partition map
 */
const struct flash_area default_flash_map[] = {
	DT_INST_FOREACH_STATUS_OKAY(FOREACH_PARTION)
};

#endif /* USE_PARTITION_MANAGER */

const int flash_map_entries = ARRAY_SIZE(default_flash_map);
const struct flash_area *flash_map = default_flash_map;
