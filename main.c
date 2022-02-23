/*
 * Copyright (c) 2022 Balázs Triszka <balika011@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * Copyright (c) 2022 Balázs Triszka <balika011@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "glitch.pio.h"

#define CPU_RESET 11
#define PLL_BYPASS 12
#define DEBUG_LED 25

int main(void)
{
	vreg_set_voltage(VREG_VOLTAGE_1_30);
	set_sys_clock_khz(266000, true);

	uint32_t freq = clock_get_hz(clk_sys);
	clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, freq, freq);

	while (1)
	{
		gpio_init(CPU_RESET);
		gpio_set_dir(CPU_RESET, GPIO_IN);
		gpio_set_slew_rate(CPU_RESET, GPIO_SLEW_RATE_FAST);
		gpio_set_drive_strength(CPU_RESET, GPIO_DRIVE_STRENGTH_12MA);
		while (!gpio_get(CPU_RESET))
			;

		uint offset = pio_add_program(pio0, &glitch_program);
		pio_sm_config c = glitch_program_get_default_config(offset);
		sm_config_set_sideset_pins(&c, CPU_RESET);

		pio_gpio_init(pio0, CPU_RESET);
		gpio_set_slew_rate(CPU_RESET, GPIO_SLEW_RATE_FAST);
		gpio_set_drive_strength(CPU_RESET, GPIO_DRIVE_STRENGTH_12MA);

		pio_gpio_init(pio0, PLL_BYPASS);
		gpio_set_slew_rate(PLL_BYPASS, GPIO_SLEW_RATE_FAST);
		gpio_set_drive_strength(PLL_BYPASS, GPIO_DRIVE_STRENGTH_12MA);

		pio_sm_set_pins_with_mask(pio0, 0, (1u << CPU_RESET) | (0u << PLL_BYPASS), (1u << CPU_RESET) | (1u << PLL_BYPASS));
		pio_sm_set_pindirs_with_mask(pio0, 0, (1u << CPU_RESET) | (1u << PLL_BYPASS), (1u << CPU_RESET) | (1u << PLL_BYPASS));

		pio_sm_init(pio0, 0, offset, &c);
		pio_sm_set_enabled(pio0, 0, true);

		sleep_ms(200);

		gpio_init(DEBUG_LED);
		gpio_set_dir(DEBUG_LED, GPIO_OUT);
		gpio_put(DEBUG_LED, 1);

		pio_sm_put_blocking(pio0, 0, 3);		 // post bits
		pio_sm_put_blocking(pio0, 0, 144686585); // delay from 11 post to pll pull up
		pio_sm_put_blocking(pio0, 0, 9693040);	 // delay from post going up to rst pull down
		pio_sm_put_blocking(pio0, 0, 63);		 // delay from glitch start till glitch end
		pio_sm_put_blocking(pio0, 0, 66500);	 // delay from glitch end to pll down

		pio_sm_get_blocking(pio0, 0);

		pio_sm_set_enabled(pio0, 0, false);

		pio_sm_restart(pio0, 0);

		pio_remove_program(pio0, &glitch_program, offset);

		gpio_put(DEBUG_LED, 0);

		gpio_init(CPU_RESET);
		gpio_set_dir(CPU_RESET, GPIO_IN);
		while (gpio_get(CPU_RESET))
			;
	}

	return 0;
}