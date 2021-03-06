#ifndef _ASM_GPIO_H
#define _ASM_GPIO_H

#if CONFIG_ARCH_NR_GPIO > 0
#define ARCH_NR_GPIOS CONFIG_ARCH_NR_GPIO
#endif

#ifndef __ARM_GPIOLIB_COMPLEX
#include <asm-generic/gpio.h>

/* The trivial gpiolib dispatchers */
#define gpio_get_value  __gpio_get_value
#define gpio_set_value  __gpio_set_value
#define gpio_cansleep   __gpio_cansleep
#endif

/*
 * Provide a default gpio_to_irq() which should satisfy every case.
 * However, some platforms want to do this differently, so allow them
 * to override it.
 */
#ifndef gpio_to_irq
#define gpio_to_irq	__gpio_to_irq
#endif

#endif /* _ASM_GPIO_H */
