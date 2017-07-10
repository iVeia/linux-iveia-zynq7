/*
 * iVeia HH1 LCD Framebuffer driver
 *
 * (C) Copyright 2008, iVeia, LLC
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pm.h>

/*********************************************************************
 *
 * Globals
 *
 *********************************************************************/

#define MODNAME "iveia_fb"
#define IV_SCREEN_WIDTH (800)
#define IV_SCREEN_HEIGHT (480)
#define IV_BYTES_PER_PIXEL (2)
#define ANDROID_BYTES_PER_PIXEL 2
#if defined(CONFIG_MACH_IV_ATLAS_I_Z7E)
    #define VIDEOMEMSTART	(0x0F000000)
    #define VIDEOMEMFINISH	(0x0FFFFFFF)
#elif defined(CONFIG_ARCH_IV_ATLAS_II_Z8)
    #define VIDEOMEMSTART	(0x7F000000)
    #define VIDEOMEMFINISH	(0x7FFFFFFF)
#else  //OZZY
    #define VIDEOMEMSTART	(0x7F000000)
    #define VIDEOMEMFINISH	(0x7FFFFFFF)
#endif

//#define VIDEOMEMSIZE	(IV_BYTES_PER_PIXEL*IV_SCREEN_WIDTH*IV_SCREEN_HEIGHT)
#define VIDEOMEMSIZE   VIDEOMEMFINISH-VIDEOMEMSTART

static void * fb_addr = NULL;

static struct fb_var_screeninfo iveia_fb_default = {
	.xres           = IV_SCREEN_WIDTH,
	.yres           = IV_SCREEN_HEIGHT,
	.xres_virtual   = IV_SCREEN_WIDTH,
	.yres_virtual   = IV_SCREEN_HEIGHT,
	.bits_per_pixel = IV_BYTES_PER_PIXEL * 8,
//    .transp.offset  = 0,
//    .transp.length  = 8,
    .blue.offset    = 0,
    .blue.length    = 5,
    .green.offset   = 5,
    .green.length   = 6,
    .red.offset     = 11,
    .red.length     = 5,
};

static struct fb_fix_screeninfo iveia_fb_fix = {
	.id             = "iVeia HH1 FB",
	.type           = FB_TYPE_PACKED_PIXELS,
	.visual         = FB_VISUAL_TRUECOLOR,
    .line_length    = IV_SCREEN_WIDTH * IV_BYTES_PER_PIXEL,
	.smem_start     = VIDEOMEMSTART,
	.smem_len       = VIDEOMEMSIZE,
};

static struct fb_ops iveia_fb_ops = {
	.fb_read        = fb_sys_read,
	.fb_write       = fb_sys_write,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
};

struct iveia_fb_par {
    unsigned long * misc_reg;
};

/*********************************************************************
 *
 * Module probe/remove
 *
 *********************************************************************/

static int iveia_fb_probe(struct platform_device *dev)
{
	struct fb_info * info = NULL;
    struct iveia_fb_par * par = NULL;
	int ret;

    fb_addr = ioremap(iveia_fb_fix.smem_start, iveia_fb_fix.smem_len);
	if ( ! fb_addr ) return -ENOMEM;

    /*
     * Don't erase the screen - u-boot already put up a splash screen.
     */
    if (0) {
        memset(fb_addr, 0, iveia_fb_fix.smem_len);
    }

	info = framebuffer_alloc(sizeof(struct iveia_fb_par), &dev->dev);
	if (!info) goto out;

	info->screen_base = (char __iomem *) fb_addr;
	info->fbops = &iveia_fb_ops;

    info->var = iveia_fb_default;
	info->fix = iveia_fb_fix;
	info->flags = FBINFO_FLAG_DEFAULT;

    par = info->par;

	ret = register_framebuffer(info);
	if (ret < 0) goto out;

	platform_set_drvdata(dev, info);

	printk(KERN_INFO
	       "fb%d: iVeia frame buffer device, using %ldK of video memory\n",
	       info->node, (unsigned long) (iveia_fb_fix.smem_len >> 10));
	return 0;

out:
	printk(KERN_WARNING "fb: Could not intitalize fb device (err %d)\n", ret);
    if (info)
        framebuffer_release(info);
    if (fb_addr)
        iounmap(fb_addr);

	return ret;
}

static int iveia_fb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);
    struct iveia_fb_par * par;

	if (info) {
		unregister_framebuffer(info);
        if (fb_addr)
            iounmap(fb_addr);
		framebuffer_release(info);

        par = info->par;
	}
	return 0;
}

/*********************************************************************
 *
 * Module init/exit
 *
 *********************************************************************/

static struct platform_driver iveia_fb_driver = {
	.probe	    = iveia_fb_probe,
	.remove     = iveia_fb_remove,
	.driver     = {
		.name	= MODNAME,
	},
};

static struct platform_device *iveia_fb_device;

static int __init iveia_fb_init(void)
{
    int ret;

    ret = platform_driver_register(&iveia_fb_driver);

    if (!ret) {
        iveia_fb_device = platform_device_alloc(MODNAME, 0);

        if (iveia_fb_device)
            ret = platform_device_add(iveia_fb_device);
        else
            ret = -ENOMEM;

        if (ret) {
            platform_device_put(iveia_fb_device);
            platform_driver_unregister(&iveia_fb_driver);
        }
    }
    return 0;
}


static void __exit iveia_fb_exit(void)
{
	platform_device_unregister(iveia_fb_device);
	platform_driver_unregister(&iveia_fb_driver);
}

module_init(iveia_fb_init);
module_exit(iveia_fb_exit);
MODULE_LICENSE("GPL");

