/*
 *  Copyright (C) 2011 Xilinx
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/spi/eeprom.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/mtd/physmap.h>
#include <linux/spi/flash.h>
#include <linux/ctype.h>
#include <linux/random.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk/zynq.h>
#include <linux/clocksource.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/memblock.h>
#include <linux/irqchip.h>
#include <linux/sys_soc.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/mach-types.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/smp_scu.h>
#include <asm/system_info.h>
#include <asm/hardware/cache-l2x0.h>

#include "board-iv-atlas-i-z7e.h"

#include <linux/proc_fs.h>

#include "common.h"

#define PROC_PERM	(S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define MODNAME "board-iv-atlas-i-z7e"

#define ZYNQ_DEVCFG_MCTRL		0x80
#define ZYNQ_DEVCFG_PS_VERSION_SHIFT	28
#define ZYNQ_DEVCFG_PS_VERSION_MASK	0xF

static struct platform_device zynq_cpuidle_device = {
	.name = "cpuidle-zynq",
};

/**
 * zynq_get_revision - Get Zynq silicon revision
 *
 * Return: Silicon version or -1 otherwise
 */
static int __init zynq_get_revision(void)
{
	struct device_node *np;
	void __iomem *zynq_devcfg_base;
	u32 revision;

	np = of_find_compatible_node(NULL, NULL, "xlnx,zynq-devcfg-1.0");
	if (!np) {
		pr_err("%s: no devcfg node found\n", __func__);
		return -1;
	}

	zynq_devcfg_base = of_iomap(np, 0);
	if (!zynq_devcfg_base) {
		pr_err("%s: Unable to map I/O memory\n", __func__);
		return -1;
	}

	revision = readl(zynq_devcfg_base + ZYNQ_DEVCFG_MCTRL);
	revision >>= ZYNQ_DEVCFG_PS_VERSION_SHIFT;
	revision &= ZYNQ_DEVCFG_PS_VERSION_MASK;

	iounmap(zynq_devcfg_base);

	return revision;
}


extern void platform_device_init(void);
extern void xusbps_init(void);

void __init zynq_map_io(void);
void __init zynq_init_late(void);
void __init zynq_timer_init(void);
void __init zynq_irq_init(void);
void __init zynq_memory_init(void);
void zynq_system_reset(char mode, const char *cmd);
void __init zynq_data_prefetch_enable(void *info);

#define ATLAS_II_GF_CARRIOR_ORD		"00034"
#define IV_IO_MAX_BOARDS 16

struct iv_io_board_info _iv_io_board_info_default = {};
struct iv_io_board_info * iv_io_board_info_default = &_iv_io_board_info_default;
struct iv_io_board_info * iv_io_board_info;
struct iv_io_board_info * iv_io_board_info_registered[IV_IO_MAX_BOARDS];
int iv_io_register_board_info(struct iv_io_board_info * info)
{
	int i;
	if (! info)
		return -EINVAL;

	for (i = 0; i < IV_IO_MAX_BOARDS; i++) {
		if (! iv_io_board_info_registered[i]) break;
	}
	if (i > IV_IO_MAX_BOARDS)
		return -ENOMEM;

	iv_io_board_info_registered[i] = info;

	return 0;
}
EXPORT_SYMBOL_GPL(iv_io_register_board_info);

static char iv_io[MAX_KERN_CMDLINE_FIELD_LEN];
static int __init iv_io_setup(char *options)
{
	strncpy(iv_io, options, sizeof(iv_io));
	iv_io[sizeof(iv_io) - 1] = '\0';
	return 0;
}
__setup("iv_io=", iv_io_setup);

static char iv_mb[MAX_KERN_CMDLINE_FIELD_LEN];
static int __init iv_mb_setup(char *options)
{
	strncpy(iv_mb, options, sizeof(iv_mb));
	iv_mb[sizeof(iv_mb) - 1] = '\0';
	return 0;
}
__setup("iv_mb=", iv_mb_setup);

static char iv_master[MAX_KERN_CMDLINE_FIELD_LEN];
static int __init iv_master_setup(char *options)
{
	strncpy(iv_master, options, sizeof(iv_master));
	iv_master[sizeof(iv_master) - 1] = '\0';
	return 0;
}
__setup("master=", iv_master_setup);

int is_atlas_master(void){
    //return atoi(iv_master);
    return simple_strtol(iv_master,NULL,10);
}

/*
 * iv_board_get_field() - return a static pointer to the given field or subfield.
 *
 * buf must be a buffer of at least MAX_KERN_CMDLINE_FIELD_LEN bytes.
 *
 * Returns field (stored in buf), or NULL on no match.
 */
static char * iv_board_get_field(char * buf, IV_BOARD_CLASS class, IV_BOARD_FIELD field,
					IV_BOARD_SUBFIELD subfield)
{
	char * s = buf;
	char * pfield;
	char * board;

	if (class == IV_BOARD_CLASS_MB)
		board = iv_mb;
	else if (class == IV_BOARD_CLASS_IO)
		board = iv_io;
	else
		return NULL;

	if (!board) {
		s[0] = '\0';
	} else {
		strncpy(s, board, MAX_KERN_CMDLINE_FIELD_LEN);
		s[MAX_KERN_CMDLINE_FIELD_LEN - 1] = '\0';
	}

	if (field == IV_BOARD_FIELD_NONE)
		return s;

	pfield = strsep(&s, ",");
	if (field == IV_BOARD_FIELD_PN) {
		s = pfield;
		if (subfield == IV_BOARD_SUBFIELD_NONE)
			return s;

		pfield = strsep(&s, "-"); // Skip first subfield
		pfield = strsep(&s, "-");
		if (subfield == IV_BOARD_SUBFIELD_PN_ORDINAL)
			return pfield;
		pfield = strsep(&s, "-");
		if (subfield == IV_BOARD_SUBFIELD_PN_VARIANT)
			return pfield;
		pfield = strsep(&s, "-");
		if (subfield == IV_BOARD_SUBFIELD_PN_REVISION)
			return pfield;
	}
	pfield = strsep(&s, ",");
	if (field == IV_BOARD_FIELD_SN)
		return pfield;
	pfield = strsep(&s, ",");
	if (field == IV_BOARD_FIELD_NAME)
		return pfield;

	return NULL;
}


/*
 * iv_board_match() - match board field against string
 *
 * Returns non-zero on match
 */
static int iv_board_match(IV_BOARD_CLASS class, IV_BOARD_FIELD field,
					IV_BOARD_SUBFIELD subfield, char * s)
{
	char buf[MAX_KERN_CMDLINE_FIELD_LEN];
	char * p = iv_board_get_field(buf, class, field, subfield);
	return p && s && (strcmp(p, s) == 0);
}

/*
 * iv_board_ord_match() - match board ordinal against string
 *
 * Returns non-zero on match
 */
static int iv_board_ord_match(char * s)
{
	return iv_board_match(IV_BOARD_CLASS_IO, IV_BOARD_FIELD_PN,
					IV_BOARD_SUBFIELD_PN_ORDINAL, s);
}

/*
 * Return board info struct.  Don't call before board is initialized!
 */
const struct iv_io_board_info * iv_io_get_board_info(void)
{
	BUG_ON(iv_io_board_info == NULL);
	return iv_io_board_info;
}

#define SN_LEN 5

static const char sn_char_map[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

/* iVeia's Organizational Unique Identifier (OUI) */
static const unsigned char iv_oui[3] = {0x00, 0x21, 0x68};

/*
 * Get the base MAC address assigned to the given board class.
 *
 * Each iVeia board has two MAC addresses allocated.  These are
 * based off the board's serial number.  This routine will derive
 * both addresses, and return the 6 byte result in "mac".
 *
 * This function will ALWAYS return an iVeia MAC address.  If the board SN is
 * invalid or not available, this func will use a random iVeia MAC addr, and
 * printk a warning.
 *
 * This function is not reentrant, and because it can return different numbers
 * on each call (if a random MAC is used) it should only be called once per
 * class.
 *
 * Format of returned MAC address data in a buffer is:
 *  MAC address = 00:21:68:AB:CD:EF
 *  mac = {0x00, 0x21, 0x68, 0xAB, 0xCD, 0xEF}
 */
static void get_base_macaddr_call_only_once(char * mac, IV_BOARD_CLASS class)
{
	unsigned long numeric_sn = 0;
	char buf[MAX_KERN_CMDLINE_FIELD_LEN];
	char * alpha_sn;
	char * classname;
	int i;
	
	alpha_sn = iv_board_get_field(buf, class, IV_BOARD_FIELD_SN, IV_BOARD_SUBFIELD_NONE);
	if (!alpha_sn)
		goto process_numeric_sn;

	if (strlen(alpha_sn) != SN_LEN)
		goto process_numeric_sn;

	/*
	 * An SN is a 5 character string that converts to a 25-bit number,
	 * where each char represents 5-bits, according to the sn_char_map.
	 */
	for (i = 0; i < SN_LEN; i++) {
		unsigned long val_5bit;
		char * p;
		p = strchr(sn_char_map, toupper(alpha_sn[i]));
		if (!p) {
			numeric_sn = 0;
			goto process_numeric_sn;
		}
		val_5bit = p - sn_char_map;
		numeric_sn = (numeric_sn << 5) | val_5bit;
	}

process_numeric_sn:
	switch (class){
		case IV_BOARD_CLASS_IO: classname = "IO board"; break;
		case IV_BOARD_CLASS_MB: classname = "mainboard "; break;
		case IV_BOARD_CLASS_BP: classname = "backplane"; break;
		default: classname = "unknown board type"; break;
	}

	if (!numeric_sn) {
		printk(KERN_WARNING MODNAME 
			": Invalid or non-existent %s SN (%s).  Using random MAC addr.\n", 
			classname, alpha_sn);
		numeric_sn = get_random_int();
	}

	numeric_sn <<= 1;
	memcpy(mac, iv_oui, 3);
	mac[3] = (numeric_sn >> 16) & 0xFF;
	mac[4] = (numeric_sn >> 8) & 0xFF;
	mac[5] = (numeric_sn >> 0) & 0xFE;

	printk(KERN_INFO MODNAME 
		": %s SN (%s) is using base MAC addr %02X:%02X:%02X:%02X:%02X:%02X.\n", 
		classname, alpha_sn, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static char * get_base_macaddr(IV_BOARD_CLASS class)
{
	static char iv_io_mac[6];
	static char iv_mb_mac[6];
	static char iv_bp_mac[6];
	/* bad_mac, just in case - should never be used */
	static char bad_mac[] = "\x00\x21\x68\xFF\xFF\xFE"; 
	static atomic_t firsttime = ATOMIC_INIT(-1);

	if (atomic_inc_and_test(&firsttime)) {
		get_base_macaddr_call_only_once(iv_io_mac, IV_BOARD_CLASS_IO);
		get_base_macaddr_call_only_once(iv_mb_mac, IV_BOARD_CLASS_MB);
		get_base_macaddr_call_only_once(iv_bp_mac, IV_BOARD_CLASS_BP);
	}

	switch (class){
		case IV_BOARD_CLASS_IO: return iv_io_mac;
		case IV_BOARD_CLASS_MB: return iv_mb_mac;
		case IV_BOARD_CLASS_BP: return iv_bp_mac;
		default: return bad_mac;
	}
}

/*
 * /proc filesystem support
 */
#define MACADDR_INDEX_MASK	0x000000FF
#define MACADDR_0		0x00000000
#define MACADDR_1		0x00000001
#define MACADDR_IV_IO		0x00000100
#define MACADDR_IV_MB		0x00000200
#define MACADDR_IV_BP		0x00000400
#define MACADDR_AS_VAR		0x00010000

#define MACADDR_IV_IO_0		(MACADDR_IV_IO | MACADDR_0)
#define MACADDR_AS_VAR_IV_IO_0	(MACADDR_IV_IO | MACADDR_0 | MACADDR_AS_VAR)
#define MACADDR_IV_IO_1		(MACADDR_IV_IO | MACADDR_1)
#define MACADDR_AS_VAR_IV_IO_1	(MACADDR_IV_IO | MACADDR_1 | MACADDR_AS_VAR)
#define MACADDR_IV_MB_0		(MACADDR_IV_MB | MACADDR_0)
#define MACADDR_AS_VAR_IV_MB_0	(MACADDR_IV_MB | MACADDR_0 | MACADDR_AS_VAR)
#define MACADDR_IV_MB_1		(MACADDR_IV_MB | MACADDR_1)
#define MACADDR_AS_VAR_IV_MB_1	(MACADDR_IV_MB | MACADDR_1 | MACADDR_AS_VAR)
#define MACADDR_IV_BP_0		(MACADDR_IV_BP | MACADDR_0)
#define MACADDR_AS_VAR_IV_BP_0	(MACADDR_IV_BP | MACADDR_0 | MACADDR_AS_VAR)
#define MACADDR_IV_BP_1		(MACADDR_IV_BP | MACADDR_1)
#define MACADDR_AS_VAR_IV_BP_1	(MACADDR_IV_BP | MACADDR_1 | MACADDR_AS_VAR)

#define BOARD_PN		0x00000001
#define BOARD_ORD		0x00000002
#define BOARD_VARIANT		0x00000004
#define BOARD_REV		0x00000008
#define BOARD_NAME		0x00000010
#define BOARD_IV_MB		0x00000020
#define BOARD_IV_IO		0x00000040
#define BOARD_IV_BP		0x00000080

#define PROC_PERM	(S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

#define SENSORS_ENABLED_TAG     0x01
#define SENSORS_WAKE_TAG        0x02
#define SENSORS_DELAY_TAG       0x03

static int proc_read_board(struct seq_file *m, void *data)
{
	unsigned long flags = (unsigned long) m->private;
	IV_BOARD_CLASS class = flags & IV_BOARD_CLASS_MASK;
	IV_BOARD_FIELD field = flags & IV_BOARD_FIELD_MASK;
	IV_BOARD_SUBFIELD subfield = flags & IV_BOARD_SUBFIELD_MASK;

	char buf[MAX_KERN_CMDLINE_FIELD_LEN];
	char * p = iv_board_get_field(buf, class, field, subfield);

	seq_printf(m, "%s\n", p);

	return 0;
}

static int proc_read_macaddr(struct seq_file *m, void *data)
{
	unsigned long flags = (unsigned long) m->private;
	unsigned long last_mac_byte;
	char * mac;
	IV_BOARD_CLASS class;

	if (flags & MACADDR_IV_IO) {
		class = IV_BOARD_CLASS_IO;
	} else if (flags & MACADDR_IV_MB) {
		class = IV_BOARD_CLASS_MB;
	} else if (flags & MACADDR_IV_BP) {
		class = IV_BOARD_CLASS_BP;
	} else {
		printk(KERN_ERR MODNAME ": Invalid iVeia class (flags=%08X)\n",
			(unsigned int) flags);
		class = IV_BOARD_CLASS_NONE;
	}
	mac = get_base_macaddr(class);

	last_mac_byte = mac[5] + ((flags & MACADDR_INDEX_MASK) & 1);

	seq_printf(m, "%s%02X%02X%02X%02X%02X%02X\n", 
		flags & MACADDR_AS_VAR ? "macaddr=" : "",
		mac[0], mac[1], mac[2], mac[3], mac[4], last_mac_byte);
	return 0;
}

struct i2c_board_info __initdata iv_atlas_i_z7e_i2c1_boardinfo[] = {
    {
                I2C_BOARD_INFO("lm75", 0x48),
    }
/*
    {
        I2C_BOARD_INFO("m41t62", 0x68),  // rtc
    },
*/
};

static int __init iv_atlas_i_z7e_i2c_init(void)
{
	struct i2c_board_info * i2c0_boardinfo;
	unsigned int i2c0_boardinfo_size;

    i2c_register_board_info(1, iv_atlas_i_z7e_i2c1_boardinfo, ARRAY_SIZE(iv_atlas_i_z7e_i2c1_boardinfo));

	i2c0_boardinfo = iv_io_board_info->i2c0_boardinfo;
	i2c0_boardinfo_size = iv_io_board_info->i2c0_boardinfo_size;

    if (i2c0_boardinfo)
        i2c_register_board_info(0, i2c0_boardinfo, i2c0_boardinfo_size);

    return 0;
}

//#if 0
struct proc_func_entry {
	struct proc_func_entry * proc_dir;
	int len;
	char * name;
	struct file_operations * fops;
	void * data;
};

static int board_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, proc_read_board, PDE_DATA(inode));
}

static const struct file_operations board_ops = {
        .open           = board_proc_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};

static int macaddr_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, proc_read_macaddr, PDE_DATA(inode));
}

static const struct file_operations macaddr_ops = {
        .open           = macaddr_proc_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};


static struct proc_func_entry proc_macaddr_iv_io_dir[] = {
	{ NULL, 0, "0", &macaddr_ops, (void *) MACADDR_IV_IO_0 },
	{ NULL, 0, "0_as_macaddr_var",  &macaddr_ops, (void *) MACADDR_AS_VAR_IV_IO_0 },
	{ NULL, 0, "1",  &macaddr_ops, (void *) MACADDR_IV_IO_1 },
	{ NULL, 0, "1_as_macaddr_var",  &macaddr_ops, (void *) MACADDR_AS_VAR_IV_IO_1 },
};

static struct proc_func_entry proc_macaddr_iv_mb_dir[] = {
	{ NULL, 0, "0", &macaddr_ops, (void *) MACADDR_IV_MB_0 },
	{ NULL, 0, "0_as_macaddr_var",  &macaddr_ops, (void *) MACADDR_AS_VAR_IV_MB_0 },
	{ NULL, 0, "1", &macaddr_ops, (void *) MACADDR_IV_MB_1 },
	{ NULL, 0, "1_as_macaddr_var",  &macaddr_ops, (void *) MACADDR_AS_VAR_IV_MB_1 },
};

static struct proc_func_entry proc_macaddr_iv_bp_dir[] = {
	{ NULL, 0, "0", &macaddr_ops, (void *) MACADDR_IV_BP_0 },
	{ NULL, 0, "0_as_macaddr_var",  &macaddr_ops, (void *) MACADDR_AS_VAR_IV_BP_0 },
	{ NULL, 0, "1", &macaddr_ops, (void *) MACADDR_IV_BP_1 },
	{ NULL, 0, "1_as_macaddr_var",  &macaddr_ops, (void *) MACADDR_AS_VAR_IV_BP_1 },
};

static struct proc_func_entry proc_macaddr_dir[] = {
	{ proc_macaddr_iv_io_dir, ARRAY_SIZE(proc_macaddr_iv_io_dir), "iv_io" },
	{ proc_macaddr_iv_mb_dir, ARRAY_SIZE(proc_macaddr_iv_mb_dir), "iv_mb" },
	{ proc_macaddr_iv_bp_dir, ARRAY_SIZE(proc_macaddr_iv_bp_dir), "iv_bp" },
};

#define INFO(c,f,sf) ((void *) (IV_BOARD_CLASS_##c | IV_BOARD_FIELD_##f | IV_BOARD_SUBFIELD_##sf))
static struct proc_func_entry proc_board_iv_io_dir[] = {
	{ NULL, 0, "pn",	&board_ops, INFO(IO, PN, NONE) },
	{ NULL, 0, "ord",	&board_ops, INFO(IO, PN, PN_ORDINAL) },
	{ NULL, 0, "variant",	&board_ops, INFO(IO, PN, PN_VARIANT) },
	{ NULL, 0, "rev",	&board_ops, INFO(IO, PN, PN_REVISION) },
	{ NULL, 0, "sn",	&board_ops, INFO(IO, SN, NONE) },
	{ NULL, 0, "name",	&board_ops, INFO(IO, NAME, NONE) },
};

static struct proc_func_entry proc_board_iv_mb_dir[] = {
	{ NULL, 0, "pn",	&board_ops, INFO(MB, PN, NONE) },
	{ NULL, 0, "ord",	&board_ops, INFO(MB, PN, PN_ORDINAL) },
	{ NULL, 0, "variant",	&board_ops, INFO(MB, PN, PN_VARIANT) },
	{ NULL, 0, "rev",	&board_ops, INFO(MB, PN, PN_REVISION) },
	{ NULL, 0, "sn",	&board_ops, INFO(MB, SN, NONE) },
	{ NULL, 0, "name",	&board_ops, INFO(MB, NAME, NONE) },
};

static struct proc_func_entry proc_board_iv_bp_dir[] = {
	{ NULL, 0, "pn",	&board_ops, INFO(BP, PN, NONE) },
	{ NULL, 0, "ord",	&board_ops, INFO(BP, PN, PN_ORDINAL) },
	{ NULL, 0, "variant",	&board_ops, INFO(BP, PN, PN_VARIANT) },
	{ NULL, 0, "rev",	&board_ops, INFO(BP, PN, PN_REVISION) },
	{ NULL, 0, "sn",	&board_ops, INFO(BP, SN, NONE) },
	{ NULL, 0, "name",	&board_ops, INFO(BP, NAME, NONE) },
};

static struct proc_func_entry proc_board_dir[] = {
	{ proc_board_iv_io_dir, ARRAY_SIZE(proc_board_iv_io_dir), "iv_io" },
	{ proc_board_iv_mb_dir, ARRAY_SIZE(proc_board_iv_mb_dir), "iv_mb" },
	{ proc_board_iv_bp_dir, ARRAY_SIZE(proc_board_iv_bp_dir), "iv_bp" },
};
/*
static struct proc_func_entry proc_sensors_dir[] = {
	{ NULL, 0, "enabled", proc_read_sensor, proc_write_sensor, &sensors_enabled_data },
	{ NULL, 0, "wake", proc_read_sensor, proc_write_sensor, &sensors_wake_data },
	{ NULL, 0, "delay", proc_read_sensor, proc_write_sensor, &sensors_delay_data },
};
*/
static struct proc_func_entry proc_iveia_dir[] = {
	//{ NULL, 0, "backlight", proc_read_backlight, proc_write_backlight, NULL },
	//{ proc_sensors_dir, ARRAY_SIZE(proc_sensors_dir), "sensors" },
	{ proc_macaddr_dir, ARRAY_SIZE(proc_macaddr_dir), "macaddr" },
	{ proc_board_dir, ARRAY_SIZE(proc_board_dir), "board" },
};

static struct proc_func_entry proc_root = {
	proc_iveia_dir,	ARRAY_SIZE(proc_iveia_dir), "iveia"
};
/*
 * Recursive function to create a proc entry or dir.  Should be given the proc
 * dir to create the entry in (or NULL if the top (/proc)).  The entry can
 * either be a single proc entry or a dir (that points to an array of
 * proc_func_entrys)
 * 
 * If its a dir, you must define entry's fields proc_dir and len.
 * If its an entry, you must define entry's fields read_proc, write_proc, and data.
 * And, entry->name must always be defined.
 */
static void create_proc_dir_or_entry(
	struct proc_dir_entry * dir, struct proc_func_entry * entry)
{
	int i;

	if (entry->proc_dir) {
		dir = proc_mkdir_mode(entry->name, S_IFDIR, dir);
		if (!dir)
			return;
		for (i = 0; i < entry->len; i++) {
			create_proc_dir_or_entry(
				dir, &entry->proc_dir[i]);
		}

	} else {
		struct proc_dir_entry * p;
		p = proc_create_data(entry->name, 0, dir, entry->fops, entry->data);
		if (!p) {
			printk(KERN_ERR MODNAME 
				": Can't create proc entry '%s'\n", 
				entry->name);
		}
	}
}
//#endif

static void iv_atlas_i_z7e_proc_entries_init(void)
{
//#if 0
	create_proc_dir_or_entry(NULL, &proc_root);
//#endif

	/*
	 * First time only get_base_macaddr() is called, it gets the MAC
	 * addresses, and does some info printk()s.  Do it now, just so the
	 * printks print now.
	 */
	get_base_macaddr(IV_BOARD_CLASS_NONE);
}

static int iv_atlas_i_z7e_ioboard_init(void)
{
	struct iv_io_board_info * info = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(iv_io_board_info_registered); i++) {
		info = iv_io_board_info_registered[i];
		if (info && iv_board_ord_match(info->board_ord)) {
			printk(KERN_INFO MODNAME ": %s IO board.\n", info->name);
			break;
		}
	}
	if (i >= ARRAY_SIZE(iv_io_board_info_registered)) {
		//_iv_io_board_info_default.board_ord = kmalloc(5);
		_iv_io_board_info_default.board_ord = ATLAS_II_GF_CARRIOR_ORD;
		printk(KERN_INFO MODNAME ": Unrecognized IO board, using defaults.\n");
		iv_io_board_info = iv_io_board_info_default;
	} else {
		iv_io_board_info = info;
	}

	return 0;
}

static void __init board_atlas_i_z7e_init(void)
{
	struct platform_device_info devinfo = { .name = "cpufreq-dt", };
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	struct device *parent = NULL;

	iv_atlas_i_z7e_ioboard_init();
	iv_atlas_i_z7e_i2c_init();
	iv_atlas_i_z7e_proc_entries_init();


	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		goto out;

	system_rev = zynq_get_revision();

	soc_dev_attr->family = kasprintf(GFP_KERNEL, "Xilinx Zynq");
	soc_dev_attr->revision = kasprintf(GFP_KERNEL, "0x%x", system_rev);
	soc_dev_attr->soc_id = kasprintf(GFP_KERNEL, "0x%x",
					 zynq_slcr_get_device_id());

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev)) {
		kfree(soc_dev_attr->family);
		kfree(soc_dev_attr->revision);
		kfree(soc_dev_attr->soc_id);
		kfree(soc_dev_attr);
		goto out;
	}

	parent = soc_device_to_device(soc_dev);

out:
	/*
	 * Finished with the static registrations now; fill in the missing
	 * devices
	 */
	of_platform_populate(NULL, of_default_bus_match_table, NULL, parent);

	platform_device_register(&zynq_cpuidle_device);


	if (iv_io_board_info->proc_init)
		iv_io_board_info->proc_init(iv_io_board_info);

	if (iv_io_board_info->late_init)
		iv_io_board_info->late_init(iv_io_board_info);


}

static int __init iv_io_late_initcall(void)
{
	if (iv_board_ord_match(iv_io_board_info->board_ord))
		if (iv_io_board_info->really_late_init)
			iv_io_board_info->really_late_init(iv_io_board_info);

	platform_device_register_simple("cpufreq-dt", -1, NULL, 0);
	zynq_pm_late_init();

#ifdef CONFIG_XILINX_L1_PREFETCH
	on_each_cpu(zynq_data_prefetch_enable, NULL, 0);
#endif

	return 0;
}
//late_initcall(iv_io_late_initcall);

#define IV_Z7E_SWRESET_GPIO 929

void iv_z7e_system_reset(char mode, const char *cmd)
{
	gpio_request(IV_Z7E_SWRESET_GPIO, NULL);
	gpio_direction_output(IV_Z7E_SWRESET_GPIO, 0);

}

static const char *zynq_dt_match[] = {
	"iveia,atlas-i-z7e",
	NULL
};

MACHINE_START(IV_ATLAS_I_Z7E, "iVeia Atlas-I-Z7e")
	.smp		= smp_ops(zynq_smp_ops),
	.map_io		= zynq_map_io,
	.init_irq	= zynq_irq_init,
	.init_machine	= board_atlas_i_z7e_init,
	.init_late	= iv_io_late_initcall,//zynq_init_late,//TO BE REPLACED
	.init_time	= zynq_timer_init,
	.dt_compat	= zynq_dt_match,
	.reserve	= zynq_memory_init,
	.restart	= iv_z7e_system_reset,
MACHINE_END
