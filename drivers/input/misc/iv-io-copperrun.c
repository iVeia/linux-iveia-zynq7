/**
 * iv-io-copperun.c - iVeia IO Copperrun Carrier Button Input Driver
 *
 * Copyright (C) 2012 iVeia, LLC
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <mach/board-iv-atlas-i-lpe.h>

#define MODNAME "iv_io_copperrun"

#define WAKE_GPIO_IRQ			161 + IH_GPIO_BASE

#define BUTTON_OFFSET			8
#define IRQ_BASE				OMAP_GPIO_IRQ_BASE
#define GPIO_BASE				TWL4030_GPIO_BASE + TWL4030_GPIO_MAX
#define COPPERRUN_BOARD_ORD		"00059"

static struct keymap_t {
	int gpio;
	int irq;
	int key;
} keymap[] = {
	{ GPIO_BASE + BUTTON_OFFSET + 0, IRQ_BASE + BUTTON_OFFSET + 0, KEY_SEARCH },
	{ GPIO_BASE + BUTTON_OFFSET + 1, IRQ_BASE + BUTTON_OFFSET + 1, KEY_BACK },
	{ GPIO_BASE + BUTTON_OFFSET + 2, IRQ_BASE + BUTTON_OFFSET + 2, KEY_HOMEPAGE },
	{ GPIO_BASE + BUTTON_OFFSET + 3, IRQ_BASE + BUTTON_OFFSET + 3, KEY_MENU },
	{ GPIO_BASE + 24 + 24 + 1, IRQ_BASE + 15, KEY_POWER },
};

static struct keymap_t keymap_init_state[ARRAY_SIZE(keymap)];

static struct input_dev * buttons_input_dev;

static irqreturn_t buttons_irq(int irq, void *_dev)
{
	struct input_dev *dev = _dev;
	struct input_dev *drvdata = input_get_drvdata(dev);
	int i;

	/*
	 * There's a little funkiness in the pca953x irq handler in that it
	 * appears IRQs are enabled during this IRQ, whereas the expectaion is
	 * that they are already disabled.  If not disabled, the following
	 * warning occurs: WARNING: at kernel/irq/handle.c:130
	 *
	 * Therefore, we force the disable here, and all is happy.
	 */
	local_irq_disable();

	/* 
	 * Its possible to get an intr before input device is registered - just
	 * ignore 
	 */
	if (!drvdata)
		return IRQ_HANDLED;

	/*
	 * Find the key corresponding to this irq, and report it
	 */
	for (i = 0; i < ARRAY_SIZE(keymap); i++) {
		if (keymap[i].irq == irq) break;
	}
	if (i >= ARRAY_SIZE(keymap)) {
		printk( KERN_ERR MODNAME ": Invalid keypress\n");
	} else {
		input_report_key(dev, keymap[i].key, !gpio_get_value_cansleep(keymap[i].gpio));
		input_sync(dev);
	}

	return IRQ_HANDLED;
}

static void iv_io_copperrun_buttons_cleanup(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(keymap_init_state); i++) {
		if (keymap_init_state[i].irq)
			free_irq(keymap_init_state[i].irq, buttons_input_dev);
		if (keymap_init_state[i].gpio)
			gpio_free(keymap_init_state[i].gpio);
	}
	input_unregister_device(buttons_input_dev);
}

static int __init iv_io_copperrun_buttons_init(void)
{
	int err;
	int i;

	if (strcmp(iv_io_get_board_info()->board_ord, COPPERRUN_BOARD_ORD) != 0)
		return 0;

	buttons_input_dev = input_allocate_device();
	if (!buttons_input_dev) {
		printk( KERN_ERR MODNAME ": Can't allocate input device\n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(keymap); i++) {
		err = gpio_request(keymap[i].gpio, NULL);
		if (err < 0) {
			printk(KERN_ERR MODNAME
				": Can't get GPIO %d for IRQ\n", keymap[i].gpio);
			goto out;
		}
		keymap_init_state[i].gpio = keymap[i].gpio;

		err = gpio_direction_input(keymap[i].gpio);
		if (err < 0) {
			printk(KERN_ERR MODNAME ": Can't set power key GPIO as input\n");
			goto out;
		}

		err = request_irq(keymap[i].irq, buttons_irq, 
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
				MODNAME, buttons_input_dev);
		if (err < 0) {
			printk( KERN_ERR MODNAME ": Can't get IRQ for key: %d\n", err);
			goto out;
		}
		keymap_init_state[i].irq = keymap[i].irq;

		input_set_capability(buttons_input_dev, EV_KEY, keymap[i].key);
	}

	err = enable_irq_wake(WAKE_GPIO_IRQ);
	if (err < 0) {
		printk( KERN_ERR MODNAME ": Can't get IRQ for key: %d\n", err);
		goto out;
	}

	buttons_input_dev->name = MODNAME;
	buttons_input_dev->phys = MODNAME "/input0";
	input_set_drvdata(buttons_input_dev, buttons_input_dev);

	err = input_register_device(buttons_input_dev);
	if (err) {
		printk( KERN_ERR MODNAME ": Can't register key: %d\n", err);
		goto out;
	}

	return 0;

out:
	iv_io_copperrun_buttons_cleanup();
	return err;
}

static void __exit iv_io_copperrun_buttons_exit(void)
{
	iv_io_copperrun_buttons_cleanup();
}

module_init(iv_io_copperrun_buttons_init);
module_exit(iv_io_copperrun_buttons_exit);

MODULE_AUTHOR("Brian Silverman <bsilverman@iveia.com>");
MODULE_DESCRIPTION("iVeia IO Copperrun Carrier Buttons");
MODULE_LICENSE("GPL");

