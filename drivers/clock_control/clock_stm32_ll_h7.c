/*
 *
 * Copyright (c) 2019 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <drivers/clock_control.h>
#include <sys/util.h>
#include <drivers/clock_control/stm32_clock_control.h>
#include "stm32_hsem.h"

/* Macros to fill up prescaler values */
#define z_sysclk_prescaler(v) LL_RCC_SYSCLK_DIV_ ## v
#define sysclk_prescaler(v) z_sysclk_prescaler(v)

#define z_ahb_prescaler(v) LL_RCC_AHB_DIV_ ## v
#define ahb_prescaler(v) z_ahb_prescaler(v)

#define z_apb1_prescaler(v) LL_RCC_APB1_DIV_ ## v
#define apb1_prescaler(v) z_apb1_prescaler(v)

#define z_apb2_prescaler(v) LL_RCC_APB2_DIV_ ## v
#define apb2_prescaler(v) z_apb2_prescaler(v)

#define z_apb3_prescaler(v) LL_RCC_APB3_DIV_ ## v
#define apb3_prescaler(v) z_apb3_prescaler(v)

#define z_apb4_prescaler(v) LL_RCC_APB4_DIV_ ## v
#define apb4_prescaler(v) z_apb4_prescaler(v)

#if defined(CONFIG_CPU_CORTEX_M7)
#if CONFIG_CLOCK_STM32_D1CPRE > 1
/*
 * D1CPRE prescaler allows to set a HCLK frequency lower than SYSCLK frequency.
 * Though, zephyr doesn't make a difference today between these two clocks.
 * So, changing this prescaler is not allowed until it is made possible to
 * use them independently in zephyr clock subsystem.
 */
#error "D1CPRE presacler can't be higher than 1"
#endif
#endif /* CONFIG_CPU_CORTEX_M7 */

static uint32_t get_bus_clock(uint32_t clock, uint32_t prescaler)
{
	return clock / prescaler;
}

static inline int stm32_clock_control_on(struct device *dev,
					 clock_control_subsys_t sub_system)
{
	struct stm32_pclken *pclken = (struct stm32_pclken *)(sub_system);
	int rc = 0;

	ARG_UNUSED(dev);

	/* Both cores can access bansk by following LL API */
	/* Using "_Cn_" LL API would restrict access to one or the other */
	z_stm32_hsem_lock(CFG_HW_RCC_SEMID, HSEM_LOCK_DEFAULT_RETRY);
	switch (pclken->bus) {
	case STM32_CLOCK_BUS_AHB1:
		LL_AHB1_GRP1_EnableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_AHB2:
		LL_AHB2_GRP1_EnableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_AHB3:
		LL_AHB3_GRP1_EnableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_AHB4:
		LL_AHB4_GRP1_EnableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_APB1:
		LL_APB1_GRP1_EnableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_APB1_2:
		LL_APB1_GRP2_EnableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_APB2:
		LL_APB2_GRP1_EnableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_APB3:
		LL_APB3_GRP1_EnableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_APB4:
		LL_APB4_GRP1_EnableClock(pclken->enr);
		break;
	default:
		rc = -ENOTSUP;
		break;
	}

	z_stm32_hsem_unlock(CFG_HW_RCC_SEMID);

	return rc;
}

static inline int stm32_clock_control_off(struct device *dev,
					  clock_control_subsys_t sub_system)
{
	struct stm32_pclken *pclken = (struct stm32_pclken *)(sub_system);
	int rc = 0;

	ARG_UNUSED(dev);

	/* Both cores can access bansk by following LL API */
	/* Using "_Cn_" LL API would restrict access to one or the other */
	z_stm32_hsem_lock(CFG_HW_RCC_SEMID, HSEM_LOCK_DEFAULT_RETRY);
	switch (pclken->bus) {
	case STM32_CLOCK_BUS_AHB1:
		LL_AHB1_GRP1_DisableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_AHB2:
		LL_AHB2_GRP1_DisableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_AHB3:
		LL_AHB3_GRP1_DisableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_AHB4:
		LL_AHB4_GRP1_DisableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_APB1:
		LL_APB1_GRP1_DisableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_APB1_2:
		LL_APB1_GRP2_DisableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_APB2:
		LL_APB2_GRP1_DisableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_APB3:
		LL_APB3_GRP1_DisableClock(pclken->enr);
		break;
	case STM32_CLOCK_BUS_APB4:
		LL_APB4_GRP1_DisableClock(pclken->enr);
		break;
	default:
		rc = -ENOTSUP;
		break;
	}
	z_stm32_hsem_unlock(CFG_HW_RCC_SEMID);

	return rc;
}

static int stm32_clock_control_get_subsys_rate(struct device *clock,
					clock_control_subsys_t sub_system,
						uint32_t *rate)
{
	struct stm32_pclken *pclken = (struct stm32_pclken *)(sub_system);
	/*
	 * Get AHB Clock (= SystemCoreClock = SYSCLK/prescaler)
	 * SystemCoreClock is preferred to CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC
	 * since it will be updated after clock configuration and hence
	 * more likely to contain actual clock speed
	 */
#if defined(CONFIG_CPU_CORTEX_M4)
	uint32_t ahb_clock = SystemCoreClock;
#else
	uint32_t ahb_clock = get_bus_clock(SystemCoreClock,
				CONFIG_CLOCK_STM32_HPRE);
#endif
	uint32_t apb1_clock = get_bus_clock(ahb_clock,
				CONFIG_CLOCK_STM32_D2PPRE1);
	uint32_t apb2_clock = get_bus_clock(ahb_clock,
				CONFIG_CLOCK_STM32_D2PPRE2);
	uint32_t apb3_clock = get_bus_clock(ahb_clock,
				CONFIG_CLOCK_STM32_D1PPRE);
	uint32_t apb4_clock = get_bus_clock(ahb_clock,
				CONFIG_CLOCK_STM32_D3PPRE);

	ARG_UNUSED(clock);

	switch (pclken->bus) {
	case STM32_CLOCK_BUS_AHB1:
	case STM32_CLOCK_BUS_AHB2:
	case STM32_CLOCK_BUS_AHB3:
	case STM32_CLOCK_BUS_AHB4:
		*rate = ahb_clock;
		break;
	case STM32_CLOCK_BUS_APB1:
	case STM32_CLOCK_BUS_APB1_2:
		*rate = apb1_clock;
		break;
	case STM32_CLOCK_BUS_APB2:
		*rate = apb2_clock;
		break;
	case STM32_CLOCK_BUS_APB3:
		*rate = apb3_clock;
		break;
	case STM32_CLOCK_BUS_APB4:
		*rate = apb4_clock;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static struct clock_control_driver_api stm32_clock_control_api = {
	.on = stm32_clock_control_on,
	.off = stm32_clock_control_off,
	.get_rate = stm32_clock_control_get_subsys_rate,
};

static int stm32_clock_control_init(struct device *dev)
{
	ARG_UNUSED(dev);

#if !defined(CONFIG_CPU_CORTEX_M4)

#ifdef CONFIG_CLOCK_STM32_SYSCLK_SRC_PLL

	/* HW semaphore Clock enable */
	LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_HSEM);

	z_stm32_hsem_lock(CFG_HW_RCC_SEMID, HSEM_LOCK_DEFAULT_RETRY);

#ifdef CONFIG_CLOCK_STM32_PLL_SRC_HSE

	if (IS_ENABLED(CONFIG_CLOCK_STM32_HSE_BYPASS)) {
		LL_RCC_HSE_EnableBypass();
	} else {
		LL_RCC_HSE_DisableBypass();
	}

	/* Enable HSE oscillator */
	LL_RCC_HSE_Enable();
	while (LL_RCC_HSE_IsReady() != 1) {
	}

	/* Set FLASH latency */
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);

	/* Main PLL configuration and activation */
	LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_HSE);
#else
	#error "CONFIG_CLOCK_STM32_PLL_SRC_HSE not selected"
#endif /* CONFIG_CLOCK_STM32_PLL_SRC_HSE */

	/* Configure PLL1 */
	LL_RCC_PLL1P_Enable();
	LL_RCC_PLL1Q_Enable();
	LL_RCC_PLL1R_Enable();
	LL_RCC_PLL1FRACN_Disable();
	LL_RCC_PLL1_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_2_4);
	LL_RCC_PLL1_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);
	LL_RCC_PLL1_SetM(CONFIG_CLOCK_STM32_PLL_M_DIVISOR);
	LL_RCC_PLL1_SetN(CONFIG_CLOCK_STM32_PLL_N_MULTIPLIER);
	LL_RCC_PLL1_SetP(CONFIG_CLOCK_STM32_PLL_P_DIVISOR);
	LL_RCC_PLL1_SetQ(CONFIG_CLOCK_STM32_PLL_Q_DIVISOR);
	LL_RCC_PLL1_SetR(CONFIG_CLOCK_STM32_PLL_R_DIVISOR);

	LL_RCC_PLL1_Enable();
	while (LL_RCC_PLL1_IsReady() != 1) {
	}

	/* Set buses (Sys,AHB, APB1, APB2 & APB4) prescalers */
	LL_RCC_SetSysPrescaler(sysclk_prescaler(CONFIG_CLOCK_STM32_D1CPRE));
	LL_RCC_SetAHBPrescaler(ahb_prescaler(CONFIG_CLOCK_STM32_HPRE));
	LL_RCC_SetAPB1Prescaler(apb1_prescaler(CONFIG_CLOCK_STM32_D2PPRE1));
	LL_RCC_SetAPB2Prescaler(apb2_prescaler(CONFIG_CLOCK_STM32_D2PPRE2));
	LL_RCC_SetAPB3Prescaler(apb3_prescaler(CONFIG_CLOCK_STM32_D1PPRE));
	LL_RCC_SetAPB4Prescaler(apb4_prescaler(CONFIG_CLOCK_STM32_D3PPRE));

	/* Set PLL1 as System Clock Source */
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL1);
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL1) {
	}

	z_stm32_hsem_unlock(CFG_HW_RCC_SEMID);

#else
	#error "CONFIG_CLOCK_STM32_SYSCLK_SRC_PLL not selected"
#endif /* CLOCK_STM32_SYSCLK_SRC_PLL */

#endif /* CONFIG_CPU_CORTEX_M4 */

	/* Set systick to 1ms */
	SysTick_Config(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / 1000);
	/* Update CMSIS variable */
	SystemCoreClock = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;

	return 0;
}

/**
 * @brief RCC device, note that priority is intentionally set to 1 so
 * that the device init runs just after SOC init
 */
DEVICE_AND_API_INIT(rcc_stm32, STM32_CLOCK_CONTROL_NAME,
		    &stm32_clock_control_init,
		    NULL, NULL,
		    PRE_KERNEL_1,
		    CONFIG_CLOCK_CONTROL_STM32_DEVICE_INIT_PRIORITY,
		    &stm32_clock_control_api);
