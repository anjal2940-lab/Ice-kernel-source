/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/skbuff.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/suspend.h>

#include "btmtk_define.h"
#include "btmtk_drv.h"
#include "btmtk_config.h"

/* Added fallback macro definitions to satisfy treating -Wundef as error build blocks */
#ifndef USE_DEVICE_NODE
#define USE_DEVICE_NODE 0
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek Bluetooth Driver Core Interface");

struct btmtk_main_info main_info;
static int bt_dbg_level = 0xff;
struct btmtk_dev *g_bdev = NULL;

__weak int btmtk_cif_send_calibration(struct btmtk_dev *bdev)
{
	BTMTK_INFO("%s, run into core logical stub initialization path\n", __func__);
	return 0;
}

#define FW_DUMP_PROC_ENTRY "driver/bt_fw_dump"
#define BT_DBG_LEVEL_ENTRY "driver/bt_dbg_lvl"

static struct proc_dir_entry *proc_entry_fw_dump = NULL;
static struct proc_dir_entry *proc_entry_dbg_lvl = NULL;

unsigned int g_fw_dump_flag = 0;
unsigned int g_fw_assert_flag = 0;

static void btmtk_main_wakelock_init(void)
{
	/* Pass NULL as the first argument to accommodate modern upstream Linux kernel wakeup layouts */
	main_info.fwdump_ws = wakeup_source_register(NULL, "btmtk_fwdump_wakelock");
	main_info.woble_ws = wakeup_source_register(NULL, "btmtk_woble_wakelock");
	main_info.eint_ws = wakeup_source_register(NULL, "btevent_eint");
}

static void btmtk_main_wakelock_uninit(void)
{
	if (main_info.fwdump_ws) {
		wakeup_source_unregister(main_info.fwdump_ws);
		main_info.fwdump_ws = NULL;
	}
	if (main_info.woble_ws) {
		wakeup_source_unregister(main_info.woble_ws);
		main_info.woble_ws = NULL;
	}
	if (main_info.eint_ws) {
		wakeup_source_unregister(main_info.eint_ws);
		main_info.eint_ws = NULL;
	}
}

void btmtk_fwdump_lock(void)
{
	__pm_stay_awake(main_info.fwdump_ws);
}

void btmtk_fwdump_unlock(void)
{
	__pm_relax(main_info.fwdump_ws);
}

void btmtk_woble_lock(void)
{
	__pm_stay_awake(main_info.woble_ws);
}

void btmtk_woble_unlock(void)
{
	__pm_relax(main_info.woble_ws);
}

void btmtk_eint_lock(void)
{
	__pm_stay_awake(main_info.eint_ws);
}

void btmtk_eint_unlock(void)
{
	__pm_relax(main_info.eint_ws);
}

static unsigned int g_bt_reset_state = 0;

void btmtk_set_reset_state(unsigned int state)
{
	g_bt_reset_state = state;
}

unsigned int btmtk_get_reset_state(void)
{
	return g_bt_reset_state;
}

void btmtk_main_set_chip_id(unsigned int chip_id)
{
	main_info.chip_id = chip_id;
	BTMTK_INFO("%s: setting active internal peripheral target code chip id assignment: 0x%04x\n", __func__, chip_id);
}

unsigned int btmtk_main_get_chip_id(void)
{
	return main_info.chip_id;
}

int btmtk_main_send_cmd(struct btmtk_dev *bdev, u8 *cmd, int len, u8 *evt, int *evt_len, int expected_evt_len)
{
	int ret = 0;

	if (bdev && bdev->send_cmd) {
		ret = bdev->send_cmd(bdev, cmd, len, evt, evt_len, expected_evt_len);
	} else {
		BTMTK_ERR("%s: null structure instance verification pointer exception tracking point\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

int btmtk_main_assert_cmd(struct btmtk_dev *bdev, u8 *cmd, int len)
{
	int ret = 0;

	if (bdev && bdev->assert_cmd) {
		ret = bdev->assert_cmd(cmd, len);
	} else {
		BTMTK_ERR("%s: function method dynamic map exception verification fallback tracking point\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

int btmtk_main_woble_cmd(struct btmtk_dev *bdev, u8 *cmd, int len)
{
	int ret = 0;

	if (bdev && bdev->woble_cmd) {
		ret = bdev->woble_cmd(cmd, len);
	} else {
		BTMTK_ERR("%s: missing interface map structural runtime linking mapping error\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

int btmtk_main_read_rom_patch(struct btmtk_dev *bdev, u8 *buf, int len, int offset, const char *filename)
{
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	int ret = 0;

	if (bdev && bdev->dev) {
		dev = bdev->dev;
	} else {
		BTMTK_ERR("%s: pointer lookup structures have reported invalid base parsing addresses\n", __func__);
		return -EINVAL;
	}

	ret = request_firmware(&fw, filename, dev);
	if (ret < 0) {
		BTMTK_ERR("%s: target image tracking reference load mapping failed error: %d\n", __func__, ret);
		return ret;
	}

	if (offset + len > fw->size) {
		BTMTK_ERR("%s: binary out of bounds size checks failed error checking sizes bounds\n", __func__);
		release_firmware(fw);
		return -EINVAL;
	}

	memcpy(buf, fw->data + offset, len);
	release_firmware(fw);

	return len;
}

int btmtk_main_get_patch_size(struct btmtk_dev *bdev, const char *filename)
{
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	int ret = 0;
	int size = 0;

	if (bdev && bdev->dev) {
		dev = bdev->dev;
	} else {
		BTMTK_ERR("%s: tracking address base initialization state references are null\n", __func__);
		return -EINVAL;
	}

	ret = request_firmware(&fw, filename, dev);
	if (ret < 0) {
		BTMTK_ERR("%s: query interface path check requests on missing firmware objects: %d\n", __func__, ret);
		return ret;
	}

	size = fw->size;
	release_firmware(fw);

	return size;
}

int btmtk_main_load_firmware(struct btmtk_dev *bdev, const char *filename, u8 **proc_buf)
{
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	int ret = 0;
	u8 *buf = NULL;

	if (bdev && bdev->dev) {
		dev = bdev->dev;
	} else {
		BTMTK_ERR("%s: system framework reference pointer interfaces map is unassigned\n", __func__);
		return -EINVAL;
	}

	ret = request_firmware(&fw, filename, dev);
	if (ret < 0) {
		BTMTK_ERR("%s: structural mapping file path tracking initialization error: %d\n", __func__, ret);
		return ret;
	}

	buf = vmalloc(fw->size);
	if (!buf) {
		BTMTK_ERR("%s: memory execution structure allocations could not request sizing targets\n", __func__);
		release_firmware(fw);
		return -ENOMEM;
	}

	memcpy(buf, fw->data, fw->size);
	*proc_buf = buf;
	ret = fw->size;

	release_firmware(fw);
	return ret;
}

int btmtk_main_free_firmware(u8 *proc_buf)
{
	if (proc_buf) {
		vfree(proc_buf);
	}
	return 0;
}

int btmtk_main_drv_register(struct btmtk_dev *bdev)
{
	if (!bdev) {
		BTMTK_ERR("%s: reference verification address checks tracking input structural maps are empty\n", __func__);
		return -EINVAL;
	}

	g_bdev = bdev;
	BTMTK_INFO("%s: custom device layer tracking definitions integrated properly inside core context loops\n", __func__);

	return 0;
}

int btmtk_main_drv_deregister(struct btmtk_dev *bdev)
{
	if (g_bdev == bdev) {
		g_bdev = NULL;
		BTMTK_INFO("%s: targeted driver mapping hooks unlinked successfully cleanly from standard execution chains\n", __func__);
	}
	return 0;
}

static ssize_t bt_fw_dump_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	int len;

	len = snprintf(buf, sizeof(buf), "%u\n", g_fw_dump_flag);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t bt_fw_dump_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	unsigned int val;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = '\0';
	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	g_fw_dump_flag = val;
	BTMTK_INFO("%s: proc subsystem interface runtime trigger parameters evaluated tracking state: %u\n", __func__, g_fw_dump_flag);

	if (g_fw_dump_flag == 1 && g_bdev && g_bdev->fw_dump) {
		g_bdev->fw_dump();
	}

	return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_fw_dump_ops = {
	.proc_read = bt_fw_dump_read,
	.proc_write = bt_fw_dump_write,
};
#else
static const struct file_operations proc_fw_dump_ops = {
	.owner = THIS_MODULE,
	.read = bt_fw_dump_read,
	.write = bt_fw_dump_write,
};
#endif

static int bt_dbg_lvl_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", bt_dbg_level);
	return 0;
}

static int bt_dbg_lvl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bt_dbg_lvl_proc_show, NULL);
}

static ssize_t bt_dbg_lvl_proc_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	unsigned int val;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = '\0';
	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	bt_dbg_level = val;
	BTMTK_INFO("%s: execution debug output log verbose parameters assigned tracking value: 0x%x\n", __func__, bt_dbg_level);

	return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_dbg_lvl_ops = {
	.proc_open = bt_dbg_lvl_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = bt_dbg_lvl_proc_write,
};
#else
static const struct file_operations proc_dbg_lvl_ops = {
	.owner = THIS_MODULE,
	.open = bt_dbg_lvl_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = bt_dbg_lvl_proc_write,
};
#endif

static int __init btmtk_main_init(void)
{
	BTMTK_INFO("MediaTek Bluetooth Subsystem Interface Driver Core Framework initialization runtime tracking sequences running...\n");

	btmtk_main_wakelock_init();

	proc_entry_fw_dump = proc_create(FW_DUMP_PROC_ENTRY, 0660, NULL, &proc_fw_dump_ops);
	if (!proc_entry_fw_dump) {
		BTMTK_ERR("Failed to create proc core crash binary generation hook descriptors path entry points\n");
	}

	proc_entry_dbg_lvl = proc_create(BT_DBG_LEVEL_ENTRY, 0660, NULL, &proc_dbg_lvl_ops);
	if (!proc_entry_dbg_lvl) {
		BTMTK_ERR("Failed to mount internal troubleshooting visibility parameters maps descriptors points\n");
	}

	return 0;
}

static void __exit btmtk_main_exit(void)
{
	BTMTK_INFO("MediaTek Bluetooth Main Engine Core Interface modules shutdown operations clearing system space references...\n");

	btmtk_main_wakelock_uninit();

	if (proc_entry_fw_dump) {
		remove_proc_entry(FW_DUMP_PROC_ENTRY, NULL);
	}

	if (proc_entry_dbg_lvl) {
		remove_proc_entry(BT_DBG_LEVEL_ENTRY, NULL);
	}
}

module_init(btmtk_main_init);
module_exit(btmtk_main_exit);

#define SEC_TO_MS(x) ((x) * 1000)
#define MS_TO_JIFFIES(x) msecs_to_jiffies(x)

int btmtk_main_wait_evt(struct btmtk_dev *bdev, wait_queue_head_t *queue, unsigned long *flag, int sec, const char *msg)
{
	int ret = 0;

	if (!bdev || !queue || !flag) {
		BTMTK_ERR("%s: empty condition loop interface validation structures tracked during check tasks\n", __func__);
		return -EINVAL;
	}

	ret = wait_event_interruptible_timeout(*queue, test_bit(*flag, &bdev->states), MS_TO_JIFFIES(SEC_TO_MS(sec)));
	if (ret == 0) {
		BTMTK_ERR("%s: tracking thread task execution block encountered wait boundaries limits timeout targets: %s\n", __func__, msg);
		return -ETIMEDOUT;
	} else if (ret < 0) {
		BTMTK_ERR("%s: parallel event signals interrupt tracing sequences triggered unexpected exceptions handling maps: %s\n", __func__, msg);
		return ret;
	}

	return 0;
}

static struct rtc_time tm;
static struct timespec64 tv;

void btmtk_get_log_time(char *buf, int len)
{
	ktime_get_real_ts64(&tv);
	rtc_time64_to_tm(tv.tv_sec, &tm);

	snprintf(buf, len, "[%02d:%02d:%02d.%06lu]",
		 tm.tm_hour, tm.tm_min, tm.tm_sec, (unsigned long)tv.tv_nsec / 1000);
}

int btmtk_main_recv_event(struct btmtk_dev *bdev, struct sk_buff *skb)
{
	if (!bdev) {
		BTMTK_ERR("%s: active internal execution processing contexts missing verification maps target maps addresses\n", __func__);
		kfree_skb(skb);
		return -EINVAL;
	}

	if (bdev->recv_event) {
		return bdev->recv_event(bdev, skb);
	}

	BTMTK_ERR("%s: unmapped logical core intercept data structures tracking layer failure routines\n", __func__);
	kfree_skb(skb);
	return -EINVAL;
}

int btmtk_main_send_calibration(struct btmtk_dev *bdev)
{
	if (!bdev) {
		BTMTK_ERR("%s: empty hardware core description initialization block pointer checked during execution steps\n", __func__);
		return -EINVAL;
	}

	/* Call target handling interface definitions context maps cleanly */
	return btmtk_cif_send_calibration(bdev);
}

#if (USE_DEVICE_NODE != 1)
static unsigned int g_woble_setting_7961[] = {
	0x57, 0xFD, 0x27, 0x06, 0x00, 0x0A, 0x46, 0x00, 0x00, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x30, 0x00, 0x00, 0x00, 0x43,
	0x32, 0x4B, 0x34, 0x4D, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0x00,
	0x00, 0x12, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

int btmtk_main_get_woble_setting(struct btmtk_dev *bdev, unsigned int **woble_setting)
{
	if (!woble_setting) {
		BTMTK_ERR("%s: empty logical data target assignment tracking pointers arrays arrays limits\n", __func__);
		return -EINVAL;
	}

	*woble_setting = g_woble_setting_7961;
	return sizeof(g_woble_setting_7961) / sizeof(unsigned int);
}
#else
int btmtk_main_get_woble_setting(struct btmtk_dev *bdev, unsigned int **woble_setting)
{
	if (!bdev || !woble_setting) {
		BTMTK_ERR("%s: internal core platform mapping properties missing verification initialization blocks paths\n", __func__);
		return -EINVAL;
	}

	if (bdev->get_woble_setting) {
		return bdev->get_woble_setting(woble_setting);
	}

	BTMTK_ERR("%s: functional callback structures missing tracking address link registration steps maps\n", __func__);
	return -EINVAL;
}
#endif

#if (USE_DEVICE_NODE != 1)
static int g_bt_eint_pin = -1;

int btmtk_main_get_eint_pin(struct btmtk_dev *bdev)
{
	return g_bt_eint_pin;
}
#else
int btmtk_main_get_eint_pin(struct btmtk_dev *bdev)
{
	if (!bdev) {
		BTMTK_ERR("%s: invalid base configuration processing structures targets running validation passes\n", __func__);
		return -EINVAL;
	}

	if (bdev->get_eint_pin) {
		return bdev->get_eint_pin();
	}

	BTMTK_ERR("%s: custom device structure hardware configurations tracking node hooks unassigned\n", __func__);
	return -EINVAL;
}
#endif

#if (USE_DEVICE_NODE != 1)
static unsigned int g_bt_ant_num = 1;

int btmtk_main_get_ant_num(struct btmtk_dev *bdev)
{
	return g_bt_ant_num;
}
#else
int btmtk_main_get_ant_num(struct btmtk_dev *bdev)
{
	if (!bdev) {
		BTMTK_ERR("%s: execution initialization definitions tracking context structural maps targets empty\n", __func__);
		return -EINVAL;
	}

	if (bdev->get_ant_num) {
		return bdev->get_ant_num();
	}

	BTMTK_ERR("%s: target reference array properties definitions are missing logical configuration pointers\n", __func__);
	return -EINVAL;
}
#endif
