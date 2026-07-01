/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "btmtk_define.h"
#include "btmtk_uart.h"
#include "btmtk_main.h"

#define LOG TRUE

/*============================================================================*/
/* Global Variable */
/*============================================================================*/
static struct btmtk_dev *g_bdev;
static struct tty_struct *g_tty_backup;

/*============================================================================*/
/* Function Prototype */
/*============================================================================*/
static int btmtk_uart_allocate_memory(void);

unsigned long flagss;

/* Allocate Uart-Related memory */
static int btmtk_uart_allocate_memory(void)
{
	if (g_bdev == NULL) {
		g_bdev = kzalloc(sizeof(*g_bdev), GFP_KERNEL);
		if (!g_bdev) {
			BTMTK_ERR("%s: alloc memory fail (g_data)", __func__);
			return -1;
		}
	}
	return 0;
}

/* Fixed: Argument modified to match its declaration in include/uart/btmtk_uart.h */
int btmtk_cif_send_calibration(struct btmtk_dev *bdev)
{
	return 0;
}

/* Send cmd and Receive evt */
int btmtk_cif_send_cmd(struct hci_dev *hdev, const uint8_t *cmd,
		const int cmd_len, int retry, int endpoint, unsigned long tx_state)
{
	int ret = -1, len = 0;
	if (!g_tty_backup) {
		BTMTK_ERR("%s: Backup TTY is null", __func__);
		return -1;
	}

	BTMTK_DBG_RAW(cmd, cmd_len, "%s, len = %d Send CMD : ", __func__, cmd_len);
	while (len != cmd_len) {
		ret = g_tty_backup->ops->write(g_tty_backup, cmd, cmd_len);
		len += ret;
		BTMTK_DBG("%s, len = %d", __func__, len);
	}

	return ret;
}

static int btmtk_uart_send_query_uart_cmd(struct hci_dev *hdev)
{
	u8 cmd[] = { 0x01, 0x6F, 0xFC, 0x05, 0x01, 0x04, 0x01, 0x00, 0x02};
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);

	/* Fixed: Adjusted to supply all 8 parameters expected by btmtk_main_send_cmd */
	btmtk_main_send_cmd(bdev, cmd, sizeof(cmd), 0, 0, 0, 0, BTMTK_TX_WAIT_VND_EVT);
	BTMTK_INFO("%s done", __func__);
	return 0;
}


/* ------ LDISC part ------ */
static int btmtk_uart_tty_open(struct tty_struct *tty)
{
	BTMTK_INFO("%s: tty %p\n", __func__, tty);

	tty->receive_room = 65536;
	tty->port->low_latency = 1;

	btmtk_uart_allocate_memory();

	tty->disc_data = g_bdev;
	g_tty_backup = tty;

	btmtk_allocate_hci_device(g_bdev, HCI_UART);

	if (tty->ldisc->ops->flush_buffer)
		tty->ldisc->ops->flush_buffer(tty);

	tty_driver_flush_buffer(tty);

	BTMTK_INFO("%s: tty done %p\n", __func__, tty);

	return 0;
}

static void btmtk_uart_tty_close(struct tty_struct *tty)
{
	btmtk_free_hci_device(g_bdev, HCI_UART);
	g_tty_backup = NULL;
	BTMTK_INFO("%s: tty %p", __func__, tty);
}

static ssize_t btmtk_uart_tty_read(struct tty_struct *tty, struct file *file,
				 unsigned char *buf, size_t count)
{
	BTMTK_INFO("%s: tty %p", __func__, tty);
	return 0;
}

static ssize_t btmtk_uart_tty_write(struct tty_struct *tty, struct file *file,
				 const unsigned char *data, size_t count)
{
	BTMTK_INFO("%s: tty %p", __func__, tty);
	return 0;
}

static unsigned int btmtk_uart_tty_poll(struct tty_struct *tty, struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	if (g_bdev && g_bdev->subsys_reset == 1) {
		mask |= POLLIN | POLLRDNORM;
		BTMTK_INFO("%s: tty %p", __func__, tty);
	}
	return mask;
}

static int btmtk_uart_tty_ioctl(struct tty_struct *tty, struct file *file,
			      unsigned int cmd, unsigned long arg)
{
	u32 err = 0;

	BTMTK_INFO("%s: tty %p", __func__, tty);

	switch (cmd) {
	case HCIUARTSETPROTO:
		pr_info("<!!> Set low_latency to TRUE <!!>\n");
		tty->port->low_latency = 1;
		break;
	case HCIUARTSETBAUD:
		pr_info("<!!> Set BAUDRATE bypass <!!>\n");
		msleep(100);
		return 1;
	case HCIUARTSETWAKEUP:
		pr_info("<!!> Send Wakeup bypass <!!>\n");
		msleep(200);
		return 1;
	case HCIUARTGETBAUD:
		pr_info("<!!> Get BAUDRATE <!!>\n");
		if (g_bdev && g_bdev->hdev)
			btmtk_uart_send_query_uart_cmd(g_bdev->hdev);
		return 1;
	case HCIUARTSETSTP:
		pr_info("<!!> Set STP mandatory command <!!>\n");
		return 1;
	case HCIUARTLOADPATCH:
		pr_info("<!!> Set HCIUARTLOADPATCH command <!!>\n");
		btmtk_load_rom_patch_766x(g_bdev);
		return 1;
	default:
		err = n_tty_ioctl_helper(tty, file, cmd, arg);
		break;
	};

	return err;
}

static void btmtk_uart_tty_receive(struct tty_struct *tty, const u8 *data, char *flags, int count)
{
	int ret = -1;
	struct btmtk_dev *bdev = tty->disc_data;

	BTMTK_DBG_RAW(data, count, "Receive");

	if (bdev && bdev->hdev) {
		ret = btmtk_recv(bdev->hdev, data, count);
		if (test_and_clear_bit(BTMTKUART_TX_SKIP_VENDOR_EVT, &bdev->tx_state)) {
			BTMTK_DBG("%s clear bit BTMTKUART_TX_SKIP_VENDOR_EVT", __func__);
			wake_up(&bdev->p_wait_event_q);
			BTMTK_DBG("%s wake_up p_wait_event_q", __func__);
		} else if (ret < 0) {
			BTMTK_ERR("%s, ret = %d", __func__, ret);
		}
	}
}

static void btmtk_uart_tty_wakeup(struct tty_struct *tty)
{
	BTMTK_INFO("%s: tty %p", __func__, tty);
}

static int uart_register(void)
{
	static struct tty_ldisc_ops btmtk_uart_ldisc;
	u32 err = 0;

	BTMTK_INFO("%s", __func__);

	memset(&btmtk_uart_ldisc, 0, sizeof(btmtk_uart_ldisc));
	btmtk_uart_ldisc.magic = TTY_LDISC_MAGIC;
	btmtk_uart_ldisc.name = "n_mtk";
	btmtk_uart_ldisc.open = btmtk_uart_tty_open;
	btmtk_uart_ldisc.close = btmtk_uart_tty_close;
	btmtk_uart_ldisc.read = btmtk_uart_tty_read;
	btmtk_uart_ldisc.write = btmtk_uart_tty_write;
	btmtk_uart_ldisc.ioctl = btmtk_uart_tty_ioctl;
	btmtk_uart_ldisc.poll = btmtk_uart_tty_poll;
	btmtk_uart_ldisc.receive_buf = btmtk_uart_tty_receive;
	btmtk_uart_ldisc.write_wakeup = btmtk_uart_tty_wakeup;
	btmtk_uart_ldisc.owner = THIS_MODULE;

	err = tty_register_ldisc(N_MTK, &btmtk_uart_ldisc);
	if (err) {
		BTMTK_ERR("MTK line discipline registration failed. (%d)", err);
		return err;
	}

	BTMTK_INFO("%s done", __func__);
	return err;
}

static int uart_deregister(void)
{
	u32 err = 0;

	err = tty_unregister_ldisc(N_MTK);
	if (err) {
		BTMTK_ERR("line discipline registration failed. (%d)", err);
		return err;
	}
	return 0;
}

int btmtk_cif_register(void)
{
	int ret = -1;

	BTMTK_INFO("%s", __func__);
	ret = uart_register();
	if (ret < 0) {
		BTMTK_ERR("*** UART registration fail(%d)! ***", ret);
		return ret;
	}
	BTMTK_INFO("%s: Done", __func__);
	return 0;
}

int btmtk_cif_deregister(void)
{
	int ret = -1;

	BTMTK_INFO("%s", __func__);
	ret = uart_deregister();
	if (ret < 0) {
		BTMTK_ERR("*** UART deregistration fail(%d)! ***", ret);
		return ret;
	}
	BTMTK_INFO("%s: Done", __func__);
	return 0;
}

