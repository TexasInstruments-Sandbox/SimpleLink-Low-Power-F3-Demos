/* main.c - Application main entry point */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>

#include <zephyr/settings/settings.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>

#include "board.h"

// For device reset
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_pmctl.h>
#include <driverlib/hapi.h>

#define BT_COMP_ID_TI            0x000D
#define CS_PROCEDURE_MODEL_ID    0x1234
#define CS_RESULT_MODEL_ID       0x1235

#define OP_CS_PROCEDURE_SET          BT_MESH_MODEL_OP_3(0x01, BT_COMP_ID_TI)
#define OP_CS_PROCEDURE_STATUS       BT_MESH_MODEL_OP_3(0x02, BT_COMP_ID_TI)

#define OP_CS_MEASURE_SET            BT_MESH_MODEL_OP_3(0x03, BT_COMP_ID_TI)
#define OP_CS_MEASURE_STATUS         BT_MESH_MODEL_OP_3(0x04, BT_COMP_ID_TI)

#define BT_MESH_OPCODE_SIZE 2  // 2 bytes for the opcode
#define BT_MESH_ONOFF_PAYLOAD_SIZE 1  // 1 byte for the OnOff value

// MCUBoot image swap

static struct k_work_delayable image_swap_work;
static struct k_work_delayable sender_image_swap_work;
static struct k_work_delayable measure_message_work;
static int measured_distance = 0;

// Receiver: set bit 0 to 1, bit 1 stays 0
static void image_swap(struct k_work *work){
	HWREG(PMCTL_BASE + PMCTL_O_AONRCLR1) |= PMCTL_AONRCLR1_FLAG_M; //set the CLR register
	HWREG(PMCTL_BASE + PMCTL_O_AONRSET1) |= 0x00000001U; //set the SET register (bit 0 = 1)
	HapiResetDevice();
}

// Sender: set bit 0 and bit 1 to 1
static void sender_image_swap(struct k_work *work){
	HWREG(PMCTL_BASE + PMCTL_O_AONRCLR1) |= PMCTL_AONRCLR1_FLAG_M; //set the CLR register
	HWREG(PMCTL_BASE + PMCTL_O_AONRSET1) |= 0x00000003U; //set the SET register (bit 0 = 1, bit 1 = 1)
	HapiResetDevice();
}

static int get_distance_measurement(void){
	int flag_var = HWREG(PMCTL_BASE + PMCTL_O_AONRSTA1); //set flag_var to the value of the register to determine image swap
	HWREG(PMCTL_BASE + PMCTL_O_AONRCLR1) |= PMCTL_AONRCLR1_FLAG_M; //set the CLR register
	return ((flag_var & 0x0003FFFF) >> 1);
}

/* Vendor CS measure Server message handlers */

static int cs_measure_receive(const struct bt_mesh_model *model,
                              struct bt_mesh_msg_ctx *ctx,
                              struct net_buf_simple *buf)
{
    printk("\n=== SERVER RECEIVED MESSAGE ===\n");
    printk("From node address: 0x%04x\n", ctx->addr);
    printk("To group address: 0x%04x\n", ctx->recv_dst);
    printk("App Index: 0x%04x\n", ctx->app_idx);

    if (buf->len != 4) {
        printk("ERROR: Invalid payload size (expected 4 bytes, got %d)\n", buf->len);
        return -EINVAL;
    }

    uint32_t value = net_buf_simple_pull_le32(buf);
    printk("Received CS measurement: %u\n", value);

    printk("===============================\n");

	return 0;
}

// Server operation: receive OP_CS_MEASURE_SET
static const struct bt_mesh_model_op vnd_cs_measure_server_op[] = {
    { OP_CS_MEASURE_SET, BT_MESH_LEN_EXACT(4), cs_measure_receive },
    BT_MESH_MODEL_OP_END,
};

static struct bt_mesh_model_pub vend_cs_measure_cli_pub = {
    .msg = NET_BUF_SIMPLE(BT_MESH_OPCODE_SIZE + 4),
};

/* Vendor CS measure Client message handlers */

static int cs_measure_status(const struct bt_mesh_model *model,
                             struct bt_mesh_msg_ctx *ctx,
                             struct net_buf_simple *buf)
{
    uint32_t value = net_buf_simple_pull_le32(buf);
    printk("Client: Received status for value %u\n", value);
	return 0;
}

// Client operation: receive OP_CS_MEASURE_STATUS
static struct bt_mesh_model_op vnd_cs_measure_client_op[] = {
    { OP_CS_MEASURE_STATUS, BT_MESH_LEN_MIN(4), cs_measure_status },
    BT_MESH_MODEL_OP_END,
};

/* Vendor CS procedure Server message handlers */

static int cs_procedure_request_receive(const struct bt_mesh_model *model,
                            struct bt_mesh_msg_ctx *ctx,
                            struct net_buf_simple *buf)
{
    printk("\n=== SERVER RECEIVED MESSAGE ===\n");
    printk("From node address: 0x%04x\n", ctx->addr);
    printk("To group address: 0x%04x\n", ctx->recv_dst);
    printk("App Index: 0x%04x\n", ctx->app_idx);

    if (buf->len != 4) {
        printk("ERROR: Invalid payload size (expected 4 bytes, got %d)\n", buf->len);
        return -EINVAL;
    }

    uint32_t value = net_buf_simple_pull_le32(buf);
    printk("Received value: %u\n", value);

    printk("===============================\n");

	k_work_schedule(&image_swap_work, K_MSEC(CONFIG_IMAGE_SWAP_DELAY));

	return 0;
}

// Server operation: receive OP_CS_PROCEDURE_SET
static const struct bt_mesh_model_op vnd_cs_procedure_server_op[] = {
    { OP_CS_PROCEDURE_SET, BT_MESH_LEN_EXACT(4), cs_procedure_request_receive },
    BT_MESH_MODEL_OP_END,
};

static struct bt_mesh_model_pub vend_cs_procedure_cli_pub = {
    .msg = NET_BUF_SIMPLE(BT_MESH_OPCODE_SIZE + 4),
};

/* Vendor CS procedure Client message handlers */

static int cs_procedure_status(const struct bt_mesh_model *model,
                               struct bt_mesh_msg_ctx *ctx,
                               struct net_buf_simple *buf)
{
    uint32_t value = net_buf_simple_pull_le32(buf);
    printk("Client: Received status for value %u\n", value);
	return 0;
}

// Client operation: receive OP_CS_PROCEDURE_STATUS
static struct bt_mesh_model_op vnd_cs_procedure_client_op[] = {
    { OP_CS_PROCEDURE_STATUS, BT_MESH_LEN_MIN(4), cs_procedure_status },
    BT_MESH_MODEL_OP_END,
};

/* Models */

/* This application only needs one element to contain its models */
static const struct bt_mesh_model models[] = {
	BT_MESH_MODEL_CFG_SRV
};

static const struct bt_mesh_model vnd_models[] = {
	BT_MESH_MODEL_VND(
		BT_COMP_ID_TI,
		CS_PROCEDURE_MODEL_ID,
		vnd_cs_procedure_server_op,
		&vend_cs_procedure_cli_pub,
		vnd_cs_procedure_client_op),
	BT_MESH_MODEL_VND(
		BT_COMP_ID_TI,
		CS_RESULT_MODEL_ID,
		vnd_cs_measure_server_op,
		&vend_cs_measure_cli_pub,
		vnd_cs_measure_client_op),
};

static const struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, models, vnd_models),
};

static const struct bt_mesh_comp comp = {
	.cid = BT_COMP_ID_TI,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

/* Provisioning */
static void prov_complete(uint16_t net_idx, uint16_t addr)
{
	printk("Device successfuly provisioned.\n");
}

static void prov_reset(void)
{
	printk("Device advertising as unprovisionned again.\n");
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
}

static uint8_t dev_uuid[16];

static const struct bt_mesh_prov prov = {
	.uuid = dev_uuid,
	.output_size = 4,
	.output_actions = BT_MESH_DISPLAY_NUMBER,
	.complete = prov_complete,
	.reset = prov_reset,
};

/* Send CS procedure request message */

static void cs_measure_send(struct k_work *work)
{
	printk("Sending CS measurement message\n");
	if (elements[0].vnd_models[1].pub->addr == BT_MESH_ADDR_UNASSIGNED) {
        printk("No group address found for the CS procedure request Client.\n");
        return;
    }

	struct bt_mesh_msg_ctx ctx = {
		.app_idx = vnd_models[1].keys[0], /* Use the bound key */
		.addr = elements[0].vnd_models[1].pub->addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
	};

	if (ctx.app_idx == BT_MESH_KEY_UNUSED) {
		printk("The CS procedure request Client must be bound to a key before "
		       "sending.\n");
		return;
	}

	BT_MESH_MODEL_BUF_DEFINE(buf, OP_CS_MEASURE_SET, 4);
	bt_mesh_model_msg_init(&buf, OP_CS_MEASURE_SET);
	net_buf_simple_add_le32(&buf, (uint32_t)measured_distance);

	bt_mesh_model_send(&vnd_models[1], &ctx, &buf, NULL, NULL);
}

/* Send CS procedure request message */

static int cs_procedure_request_send()
{
	printk("Sending CS procedure request message\n");
	if (elements[0].vnd_models[0].pub->addr == BT_MESH_ADDR_UNASSIGNED) {
        printk("No group address found for the CS procedure request Client.\n");
        return -EINVAL;
    }

	struct bt_mesh_msg_ctx ctx = {
		.app_idx = vnd_models[0].keys[0], /* Use the bound key */
		.addr = elements[0].vnd_models[0].pub->addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
	};

	if (ctx.app_idx == BT_MESH_KEY_UNUSED) {
		printk("The CS procedure request Client must be bound to a key before "
		       "sending.\n");
		return -ENOENT;
	}

	BT_MESH_MODEL_BUF_DEFINE(buf, OP_CS_PROCEDURE_SET, 4);
	bt_mesh_model_msg_init(&buf, OP_CS_PROCEDURE_SET);
	net_buf_simple_add_le32(&buf, (uint32_t)1);

	return bt_mesh_model_send(&vnd_models[0], &ctx, &buf, NULL, NULL);
}

static void button_pressed(struct k_work *work)
{

	if (!bt_mesh_is_provisioned()) {

        printk("Can't send data, client is not provisioned!\n");
		return;
    }

	// Send the new value
	int err = cs_procedure_request_send();
	if (err) {
		printk("Failed to send CS procedure request message (err: %d)\n", err);
		return;
	}

	printk("CS procedure request message sent\n");
	k_work_schedule(&sender_image_swap_work, K_MSEC(CONFIG_IMAGE_SWAP_DELAY));
}

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	err = bt_mesh_init(&prov, &comp);
	if (err) {
		printk("Initializing mesh failed (err %d)\n", err);
		return;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* This will be a no-op if settings_load() loaded provisioning info */
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);

	printk("Mesh initialized\n");

	// Get distance from previous measurement
	measured_distance = get_distance_measurement();
	if(measured_distance)
	{
		printk("Value measured : %d\n", measured_distance);
		k_work_schedule(&measure_message_work, K_MSEC(CONFIG_IMAGE_SWAP_DELAY));
	}
}

int main(void)
{
	static struct k_work button_work;
	int err = 0;

	printk("Initializing...\n");

	if (IS_ENABLED(CONFIG_HWINFO)) {
		// Currently, the CC27xx does not have a HWID driver. This will allways return -ENOSYS
		err = hwinfo_get_device_id(dev_uuid, sizeof(dev_uuid));
	}
	else
	{
		// TODO : While we don't have a HWID driver, randomize this
		dev_uuid[0] = 0xaa;
		dev_uuid[1] = 0xaa;
	}

	k_work_init(&button_work, button_pressed);
	k_work_init_delayable(&image_swap_work, image_swap);
	k_work_init_delayable(&sender_image_swap_work, sender_image_swap);
	k_work_init_delayable(&measure_message_work, cs_measure_send);

	err = board_init(&button_work);
	if (err) {
		printk("Board init failed (err: %d)\n", err);
		return 0;
	}

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}

	return 0;
}
