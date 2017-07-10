/**
 * iv-io-arcadia-pwrkey.c - iVeia IO Arcadia Carrier Power Key Input Driver
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

#define MODNAME "iv_io_arcadia_pwrkey"

#define ARCADIA_PWRKEY_GPIO 139

struct iv_io_arcadia_pwrkey_dev {
	struct input_dev *idev;
	int irq;
	int gpio;
};

static struct iv_io_arcadia_pwrkey_dev _pwrkey_dev;
static struct iv_io_arcadia_pwrkey_dev *pwrkey_dev = &_pwrkey_dev;

static struct input_dev *pwrkey_input_dev;

static irqreturn_t powerkey_irq(int irq, void *_pwr)
{
	struct input_dev *pwr = _pwr;
	struct iv_io_arcadia_pwrkey_dev *dev = input_get_drvdata(pwr);

	/* 
	 * Its possible to get an intr before input device is registered - just
	 * ignore 
	 */
	if (!dev)
		return IRQ_HANDLED;

	input_report_key(pwr, KEY_POWER, !gpio_get_value(dev->gpio));
	input_sync(pwr);

	return IRQ_HANDLED;
}

static int __init iv_io_arcadia_pwrkey_init(void)
{
	int gpio = ARCADIA_PWRKEY_GPIO;
	int irq;
	int err;

	pwrkey_input_dev = input_allocate_device();
	if (!pwrkey_input_dev) {
		printk( KERN_ERR MODNAME ": Can't allocate power key input device\n");
		return -ENOMEM;
	}

	err = gpio_request(gpio, NULL);
	if (err < 0) {
		printk(KERN_ERR MODNAME ": Can't get GPIO for IRQ\n");
		goto free_input_dev;
	}

	err = gpio_direction_input(gpio);
	if (err < 0) {
		printk(KERN_ERR MODNAME ": Can't set power key GPIO as input\n");
		goto free_gpio;
	}

	irq = gpio_to_irq(gpio);

	err = request_irq(irq, powerkey_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			MODNAME, pwrkey_input_dev);
	if (err < 0) {
		printk( KERN_ERR MODNAME ": Can't get IRQ for power key: %d\n", err);
		goto free_gpio;
	}

	pwrkey_input_dev->evbit[0] = BIT_MASK(EV_KEY);
	pwrkey_input_dev->keybit[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER);
	pwrkey_input_dev->name = MODNAME;
	pwrkey_input_dev->phys = MODNAME "/input0";

	pwrkey_dev->gpio = gpio;
	pwrkey_dev->irq = irq;
	pwrkey_dev->idev = pwrkey_input_dev;
	input_set_drvdata(pwrkey_input_dev, pwrkey_dev);

	err = input_register_device(pwrkey_input_dev);
	if (err) {
		printk( KERN_ERR MODNAME ": Can't register power key: %d\n", err);
		goto free_irq;
	}

	return 0;

free_irq:
	free_irq(irq, pwrkey_input_dev);
free_gpio:
	gpio_free(gpio);
free_input_dev:
	input_free_device(pwrkey_input_dev);
	return err;
}

static void __exit iv_io_arcadia_pwrkey_exit(void)
{
	struct iv_io_arcadia_pwrkey_dev *dev = input_get_drvdata(pwrkey_input_dev);

	free_irq(dev->irq, pwrkey_input_dev);
	gpio_free(dev->gpio);
	input_unregister_device(pwrkey_input_dev);
}

module_init(iv_io_arcadia_pwrkey_init);
module_exit(iv_io_arcadia_pwrkey_exit);

MODULE_AUTHOR("Brian Silverman <bsilverman@iveia.com>");
MODULE_DESCRIPTION("iVeia IO Arcadia Carrier Power Key");
MODULE_LICENSE("GPL");

