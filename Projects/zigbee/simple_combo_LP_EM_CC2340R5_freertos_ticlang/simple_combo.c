/******************************************************************************
 Group: CMCU LPRF
 Target Device: cc23xx

 ******************************************************************************
 
 Copyright (c) 2024-2025, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 
 
 *****************************************************************************/

/***** Trace related defines *****/
#define ZB_TRACE_FILE_ID 40123

/****** Application defines ******/
#define ZB_OUTPUT_ENDPOINT          5
#define ZB_OUTPUT_MAX_CMD_PAYLOAD_SIZE 2
#define ZB_ENABLE_ZGP_SINK
#define ZGP_SINK_USE_GP_MATCH_TABLE

#include <ti/log/Log.h>

#include "ti_zigbee_config.h"
#include "zboss_api.h"
#include "simple_combo_zcl.h"
#include "zboss_api_zgp.h"
#include "zgp_internal.h"
#include "zb_led_button.h"
#include "zboss_api_error.h"

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_fcfg.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)

/* for button handling */
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"

#ifdef ZB_CONFIGURABLE_MEM
#include "zb_mem_config_lprf3.h"
#endif

/****** Application variables declarations ******/
zb_uint16_t g_dst_addr;
zb_uint8_t g_addr_mode;
zb_uint8_t g_endpoint;
zb_bool_t perform_factory_reset = ZB_FALSE;
zb_bool_t comm_on = ZB_FALSE;
zb_uint8_t g_ind_key[ZB_CCM_KEY_SIZE] = {0xCF,0xCE,0xCD,0xCC,0xCB,0xCA,0xC9,0xC8,0xC7,0xC6,0xC5,0xC4,0xC3,0xC2,0xC1,0xC0};

/****** Application function declarations ******/
/* Handler for specific ZCL commands */
zb_uint8_t zcl_specific_cluster_cmd_handler(zb_uint8_t param);
void test_device_interface_cb(zb_uint8_t param);
void button_press_handler(zb_uint8_t param);

/****** Cluster declarations ******/
/* On/Off cluster attributes */
zb_uint8_t g_attr_on_off = ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
#ifdef ZB_ENABLE_ZLL
/* On/Off cluster attributes additions */
zb_bool_t g_attr_global_scene_ctrl  = ZB_TRUE;
zb_uint16_t g_attr_on_time  = 0;
zb_uint16_t g_attr_off_wait_time  = 0;

ZB_ZCL_DECLARE_ON_OFF_ATTRIB_LIST_EXT(on_off_attr_list, &g_attr_on_off,
    &g_attr_global_scene_ctrl, &g_attr_on_time, &g_attr_off_wait_time);
#else
ZB_ZCL_DECLARE_ON_OFF_ATTRIB_LIST(on_off_attr_list, &g_attr_on_off);
#endif

/* Basic cluster attributes */
zb_uint8_t g_attr_zcl_version  = ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
zb_uint8_t g_attr_power_source = ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE;

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST(basic_attr_list, &g_attr_zcl_version, &g_attr_power_source);

/* Identify cluster attributes */
zb_uint16_t g_attr_identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &g_attr_identify_time);

/* Groups cluster attributes */
zb_uint8_t g_attr_name_support = 0;

ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST(groups_attr_list, &g_attr_name_support);

#ifdef ZB_ZCL_SUPPORT_CLUSTER_SCENES
/* Scenes cluster attributes */
zb_uint8_t g_attr_scenes_scene_count = ZB_ZCL_SCENES_SCENE_COUNT_DEFAULT_VALUE;
zb_uint8_t g_attr_scenes_current_scene = ZB_ZCL_SCENES_CURRENT_SCENE_DEFAULT_VALUE;
zb_uint16_t g_attr_scenes_current_group = ZB_ZCL_SCENES_CURRENT_GROUP_DEFAULT_VALUE;
zb_uint8_t g_attr_scenes_scene_valid = ZB_ZCL_SCENES_SCENE_VALID_DEFAULT_VALUE;
zb_uint16_t g_attr_scenes_name_support = ZB_ZCL_SCENES_NAME_SUPPORT_DEFAULT_VALUE;

ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST(scenes_attr_list, &g_attr_scenes_scene_count,
    &g_attr_scenes_current_scene, &g_attr_scenes_current_group,
    &g_attr_scenes_scene_valid, &g_attr_scenes_name_support);
#else
zb_zcl_attr_t scenes_attr_list[] = { ZB_ZCL_NULL_ID, 0, 0, NULL };
#endif

zb_uint8_t g_current_level;
zb_uint16_t g_remaining_time;
ZB_ZCL_DECLARE_LEVEL_CONTROL_ATTRIB_LIST(level_control_attr_list, &g_current_level, &g_remaining_time);

/********************* Declare device **************************/

ZB_HA_DECLARE_COMBO_CLUSTER_LIST(
  sample_clusters,
  basic_attr_list,
  identify_attr_list,
  on_off_attr_list,
  groups_attr_list,
  level_control_attr_list);

ZB_HA_DECLARE_COMBO_EP(sample_ep, ZB_OUTPUT_ENDPOINT, sample_clusters);

ZB_HA_DECLARE_COMBO_CTX(sample_ctx, sample_ep);


static zb_bool_t error_ind_handler(zb_uint8_t severity,
                                   zb_ret_t error_code,
                                   void *additional_info);

void off_network_attention(zb_uint8_t param)
{
  ZVUNUSED(param);
  Log_printf(LogModule_Zigbee_App, Log_INFO, "off_network_attention");
  zb_osif_led_toggle(1);

  ZB_SCHEDULE_APP_ALARM(off_network_attention, 0, 1 * ZB_TIME_ONE_SECOND);
}

void start_comm(zb_uint8_t param);
void stop_comm(zb_uint8_t param);

#define CONTACT_STATUS_SWITCH_OFF 0x01 /* 0b01 */
#define CONTACT_STATUS_SWITCH_ON  0x02 /* 0b10 */

typedef ZB_PACKED_PRE struct app_mapping_entry_s
{
  zb_zgps_mapping_entry_t base;
  zb_zgps_mapping_entry_additional_info_t length;
  zb_zgps_mapping_entry_generic_switch_option_t switch_option;
} ZB_PACKED_STRUCT app_mapping_entry_t;

#define MAX_MAPPING_TABLE_ENTRIES 10
static app_mapping_entry_t zgp_mapping_table_items[MAX_MAPPING_TABLE_ENTRIES];
static app_mapping_entry_t *zgp_mapping_table[MAX_MAPPING_TABLE_ENTRIES];
static zb_uint16_t zgp_mapping_table_length = MAX_MAPPING_TABLE_ENTRIES;

MAIN()
{
  ARGV_UNUSED;

  /* Global ZBOSS initialization */
  ZB_INIT("combo");

  #ifdef ZB_LONG_ADDR
  // use the address that the customer set in the pre-defined symbols tab
  zb_ieee_addr_t g_long_addr = ZB_LONG_ADDR;
  zb_set_long_address(g_long_addr);
  #else
  /* Set the device's long address to the IEEE address pulling from the FCFG of the device */
  zb_ieee_addr_t ieee_mac_addr;
  ZB_MEMCPY(ieee_mac_addr, fcfg->deviceInfo.macAddr, 8);
  zb_set_long_address(ieee_mac_addr);
  #endif // ZB_LONG_ADDR

#ifdef ZB_COORDINATOR_ROLE
  /* Set up defaults for the commissioning */
  zb_set_network_coordinator_role(DEFAULT_CHANLIST);

  /* Set keepalive mode to mac data poll so sleepy zeds consume less power */
  zb_set_keepalive_mode(MAC_DATA_POLL_KEEPALIVE);
#ifdef DEFAULT_NWK_KEY
  zb_uint8_t nwk_key[16] = DEFAULT_NWK_KEY;
  zb_secur_setup_nwk_key(nwk_key, 0);
#endif //DEFAULT_NWK_KEY

#ifdef ZBOSS_REV23
  zb_nwk_set_max_ed_capacity(MAX_ED_CAPACITY);
#endif //ZBOSS_REV23

#elif defined ZB_ROUTER_ROLE && !defined ZB_COORDINATOR_ROLE
  zb_set_network_router_role(DEFAULT_CHANLIST);

#ifdef ZBOSS_REV23
  zb_nwk_set_max_ed_capacity(MAX_ED_CAPACITY);
#endif //ZBOSS_REV23

  /* Set keepalive mode to mac data poll so sleepy zeds consume less power */
  zb_set_keepalive_mode(MAC_DATA_POLL_KEEPALIVE);

#elif defined ZB_ED_ROLE
  zb_set_network_ed_role(DEFAULT_CHANLIST);

  /* Set end-device configuration parameters */
  zb_set_ed_timeout(ED_TIMEOUT_VALUE);
  zb_set_rx_on_when_idle(ED_RX_ALWAYS_ON);
#if ( ED_RX_ALWAYS_ON == ZB_FALSE )
  zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(ED_POLL_RATE));
  zb_zdo_pim_set_long_poll_interval(ED_POLL_RATE);
#ifdef DISABLE_TURBO_POLL
  // Disable turbo poll feature
  zb_zdo_pim_permit_turbo_poll(ZB_FALSE);
#endif // DISABLE_TURBO_POLL
#endif //ED_RX_ALWAYS_ON
#endif //ZB_ED_ROLE

  zb_set_nvram_erase_at_start(ZB_FALSE);

#ifdef ZB_ENABLE_PTA
  zb_enable_pta(0);
#endif

 /* Register device ZCL context */
  ZB_AF_REGISTER_DEVICE_CTX(&sample_ctx);

  /* Register cluster commands handler for a specific endpoint */
  ZB_AF_SET_ENDPOINT_HANDLER(ZB_OUTPUT_ENDPOINT, zcl_specific_cluster_cmd_handler);

  /* Set Device user application callback */
  ZB_ZCL_REGISTER_DEVICE_CB(test_device_interface_cb);

  ZB_ZGP_SET_MAPPING_TABLE((const zb_zgps_mapping_entry_t **)zgp_mapping_table, &zgp_mapping_table_length);

  zb_zgps_set_security_level(ZB_ZGP_FILL_GPS_SECURITY_LEVEL(
    ZB_ZGP_SEC_LEVEL_FULL_WITH_ENC,
    ZB_ZGP_DEFAULT_SEC_LEVEL_PROTECTION_WITH_GP_LINK_KEY,
    ZB_ZGP_DEFAULT_SEC_LEVEL_INVOLVE_TC));

  ZGP_GP_SET_SHARED_SECURITY_KEY_TYPE(ZB_ZGP_SEC_KEY_TYPE_ZGPD_INDIVIDUAL);
  ZB_MEMCPY(ZGP_GP_SHARED_SECURITY_KEY, g_ind_key, ZB_CCM_KEY_SIZE);
  //zb_zgp_set_shared_security_key(g_ind_key);

  zb_zgps_set_communication_mode(ZGP_COMMUNICATION_MODE_LIGHTWEIGHT_UNICAST);

  zb_error_register_app_handler(error_ind_handler);

  /* Initiate the stack start without starting the commissioning */
  if (zboss_start_no_autostart() != RET_OK)
  {
    Log_printf(LogModule_Zigbee_App, Log_ERROR, "zboss_start failed");
  }
  else
  {
    GPIO_setConfig(CONFIG_GPIO_BTN1, GPIO_CFG_IN_PU);
    GPIO_setConfig(CONFIG_GPIO_BTN2, GPIO_CFG_IN_PU);
    // if either button 1 or button 2 gets pressed
    zb_bool_t sideButtonPressed = ((GPIO_read((zb_uint8_t)CONFIG_GPIO_BTN1) == 0U) || (GPIO_read((zb_uint8_t)CONFIG_GPIO_BTN2) == 0U));
    // then perform a factory reset
    if (sideButtonPressed)
    {
      perform_factory_reset = ZB_TRUE;
      Log_printf(LogModule_Zigbee_App, Log_INFO, "performing factory reset");
    }

    zb_osif_led_button_init();
#ifndef ZB_COORDINATOR_ROLE
    ZB_SCHEDULE_APP_ALARM(off_network_attention, 0, 1 * ZB_TIME_ONE_SECOND);
#endif /* ZB_COORDINATOR_ROLE */

    /* Call the main loop */
    zboss_main_loop();
  }

  MAIN_RETURN(0);
}


static zb_bool_t error_ind_handler(zb_uint8_t severity,
                                    zb_ret_t error_code,
                                    void *additional_info)
{
  zb_bool_t ret = ZB_FALSE;
  ZVUNUSED(additional_info);
  /* Unused without trace. */
  ZVUNUSED(severity);

  Log_printf(LogModule_Zigbee_App, Log_ERROR, "error_ind_handler severity %d error_code %d", severity, error_code);

  if (error_code == ERROR_CODE(ERROR_CATEGORY_MACSPLIT, ZB_ERROR_MACSPLIT_RADIO_HANG))
  {
    Log_printf(LogModule_Zigbee_App, Log_ERROR, "Fatal macsplit error");

    ret = ZB_TRUE;
    /* return TRUE to prevent default error handling by the stack */
  }
  if (error_code == ERROR_CODE(ERROR_CATEGORY_MACSPLIT, ZB_ERROR_MACSPLIT_RADIO_REBOOT))
  {
    Log_printf(LogModule_Zigbee_App, Log_ERROR, "macsplit radio reboot");

    ret = ZB_TRUE;
  }
  Log_printf(LogModule_Zigbee_App, Log_ERROR, "error_ind_handler ret %d", ret);
  return ret;
}


void test_device_interface_cb(zb_uint8_t param)
{
  zb_zcl_device_callback_param_t *device_cb_param =
    ZB_BUF_GET_PARAM(param, zb_zcl_device_callback_param_t);

  Log_printf(LogModule_Zigbee_App, Log_INFO, "> test_device_interface_cb param %d id 0x%x",
             param, device_cb_param->device_cb_id);

  device_cb_param->status = RET_OK;

  switch (device_cb_param->device_cb_id)
  {
    case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
      if (device_cb_param->cb_param.set_attr_value_param.cluster_id == ZB_ZCL_CLUSTER_ID_ON_OFF &&
          device_cb_param->cb_param.set_attr_value_param.attr_id == ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID)
      {
        if (device_cb_param->cb_param.set_attr_value_param.values.data8)
        {
          Log_printf(LogModule_Zigbee_App, Log_INFO, "set ON");
#ifdef ZB_USE_BUTTONS
          zb_osif_led_on(0);
#endif
        }
        else
        {
          Log_printf(LogModule_Zigbee_App, Log_INFO, "set OFF");
#ifdef ZB_USE_BUTTONS
          zb_osif_led_off(0);
#endif
        }
      }
      else if (device_cb_param->cb_param.set_attr_value_param.cluster_id == ZB_ZCL_CLUSTER_ID_IDENTIFY &&
               device_cb_param->cb_param.set_attr_value_param.attr_id == ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID)
      {
        Log_printf(LogModule_Zigbee_App, Log_INFO, "identify time changed to %d (0x%x)",
                   device_cb_param->cb_param.set_attr_value_param.values.data16, device_cb_param->cb_param.set_attr_value_param.values.data16);
      }
      else
      {
        /* MISRA rule 15.7 requires empty 'else' branch. */
      }
      break;

    default:
      device_cb_param->status = RET_ERROR;
      break;
  }

  Log_printf(LogModule_Zigbee_App, Log_INFO, "< test_device_interface_cb %d", device_cb_param->status);
}

static void handle_diag_data_resp(zb_bufid_t buf)
{
  zb_zdo_get_diag_data_resp_params_t *resp_params;

  resp_params = ZB_BUF_GET_PARAM(buf, zb_zdo_get_diag_data_resp_params_t);

  ZVUNUSED(resp_params);

  Log_printf(LogModule_Zigbee_App, Log_INFO, "handle_diag_data_resp, status: %d, addr: 0x%x, lqi: %d, rssi: %d",
             resp_params->status, resp_params->short_address,
             resp_params->lqi, resp_params->rssi);

  zb_buf_free(buf);
}

static void send_diag_data_req(zb_uint16_t short_address)
{
  zb_zdo_get_diag_data_req_params_t *req;
  zb_bufid_t buf;

  buf = zb_buf_get_out();
  if (buf != ZB_BUF_INVALID)
  {
    req = ZB_BUF_GET_PARAM(buf, zb_zdo_get_diag_data_req_params_t);
    ZB_BZERO(req, sizeof(*req));

    req->short_address = short_address;
    zb_zdo_get_diag_data_async(buf, handle_diag_data_resp);
  }
  else
  {
    Log_printf(LogModule_Zigbee_App, Log_ERROR, "Failed to get a buffer");
  }
}

zb_uint8_t zcl_specific_cluster_cmd_handler(zb_uint8_t param)
{
  zb_zcl_parsed_hdr_t cmd_info;

  Log_printf(LogModule_Zigbee_App, Log_INFO, "> zcl_specific_cluster_cmd_handler");

  ZB_ZCL_COPY_PARSED_HEADER(param, &cmd_info);

  g_dst_addr = ZB_ZCL_PARSED_HDR_SHORT_DATA(&cmd_info).source.u.short_addr;
  g_endpoint = ZB_ZCL_PARSED_HDR_SHORT_DATA(&cmd_info).src_endpoint;
  g_addr_mode = ZB_APS_ADDR_MODE_16_ENDP_PRESENT;

  ZB_ZCL_DEBUG_DUMP_HEADER(&cmd_info);
  Log_printf(LogModule_Zigbee_App, Log_INFO, "payload size: %i", zb_buf_len(param));

  send_diag_data_req(g_dst_addr);

  if (cmd_info.cmd_direction == ZB_ZCL_FRAME_DIRECTION_TO_CLI)
  {
    Log_printf(LogModule_Zigbee_App, Log_ERROR, "Unsupported \"from server\" command direction");
  }

  Log_printf(LogModule_Zigbee_App, Log_INFO, "< zcl_specific_cluster_cmd_handler");
  return ZB_FALSE;
}

void restart_commissioning(zb_uint8_t param)
{
  ZVUNUSED(param);
  bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
}

void button_press_handler(zb_uint8_t param)
{
  if (!param)
  {
    /* Button is pressed, gets buffer for outgoing command */
    zb_buf_get_out_delayed(button_press_handler);
  }
  else
  {
    Log_printf(LogModule_Zigbee_App, Log_INFO, "button_press_handler %d", param);
    if(comm_on)
    {
      comm_on = ZB_FALSE;
      stop_comm(param);
    }
    else
    {
      comm_on = ZB_TRUE;
      start_comm(param);
    }
#ifndef ZB_USE_BUTTONS
    /* Do not have buttons in simulator - just start periodic on/off sending */
    ZB_SCHEDULE_APP_ALARM(button_press_handler, 0, 7 * ZB_TIME_ONE_SECOND);
#endif
  }
}

void set_tx_power(zb_int8_t power)
{
  zb_uint32_t chanlist = DEFAULT_CHANLIST;
  for (zb_uint8_t i = 0; i < 32; i++) {
    if (chanlist & (1U << i)) {
      zb_bufid_t buf = zb_buf_get_out();
      if (!buf)
      {
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "no buffer available");
        return;
      }

      zb_tx_power_params_t *power_params = (zb_tx_power_params_t *)zb_buf_begin(buf);
      power_params->status = RET_OK;
      power_params->page = 0;
      power_params->channel = i;
      power_params->tx_power = power;
      power_params->cb = NULL;

      zb_set_tx_power_async(buf);
    }
  }
}

zb_bool_t mapping_table_delete_entry_by_zgpd_id(zb_zgpd_id_t* zgpd_id)
{
  zb_uindex_t i;

  for (i = 0; i < MAX_MAPPING_TABLE_ENTRIES; i++)
  {
    if (zgp_mapping_table[i] != NULL)
    {
      zb_zgpd_id_t id;
      ZB_MAKE_ZGPD_ID(id, zgp_mapping_table[i]->base.options & 7U,
                          zgp_mapping_table[i]->base.gpd_endpoint,
                          zgp_mapping_table[i]->base.gpd_id);

      if (ZB_ZGPD_IDS_ARE_EQUAL(zgpd_id, &id))
      {
        zgp_mapping_table[i] = NULL;
      }
    }
  }

  return ZB_TRUE;
}

zb_zgps_mapping_entry_t * mapping_table_add_entry(zb_zgpd_id_t* zgpd_id,
                                                  zb_uint8_t gpd_command,
                                                  zb_uint8_t endpoint,
                                                  zb_uint16_t profile,
                                                  zb_uint16_t cluster_id,
                                                  zb_uint8_t zcl_command)
{
  zb_uindex_t i;

  for (i = 0; i < MAX_MAPPING_TABLE_ENTRIES; i++)
  {
    if (zgp_mapping_table[i] == NULL)
    {
      zb_zgps_mapping_entry_t *entry;

      entry = (zb_zgps_mapping_entry_t *)&zgp_mapping_table_items[i];

      entry->options = zgpd_id->app_id;
      ZB_MEMCPY(&entry->gpd_id, &zgpd_id->addr, sizeof(zb_zgpd_addr_t));
      entry->gpd_endpoint = zgpd_id->endpoint;

      entry->gpd_command = gpd_command;
      entry->endpoint = endpoint;
      entry->profile = profile;
      entry->cluster = cluster_id;
      entry->zcl_command = zcl_command;
      entry->zcl_payload_length = ZB_ZGP_MAPPING_ENTRY_PARSED_PAYLOAD;

      zgp_mapping_table[i] = (app_mapping_entry_t *)entry;

      return entry;
    }
  }

  return NULL;
}

/**
   Callback for accept Commissioning GP

   @param params - @ref zgp_approve_comm_params_t GP device parameters
 */
/* [accept_comm] */
void accept_comm_cb(zgp_approve_comm_params_t *params)
{
  zb_bool_t accept = ZB_TRUE;

  /* Delete all entries for this device, if any */
  mapping_table_delete_entry_by_zgpd_id(&params->zgpd_id);

  /* check supported Device ID */
  if (params->device_id == ZB_ZGP_ON_OFF_SWITCH_DEV_ID
      || params->device_id == ZB_ZGP_MANUF_SPECIFIC_DEV_ID
      || params->device_id == ZB_ZGP_LEVEL_CONTROL_SWITCH_DEV_ID)
  {
    /* check Application Description if present */
    if (params->num_of_reports == 0)
    {
      /* GPD didn't give us any specific data, let's be as generic as possible */
      accept = (mapping_table_add_entry(&params->zgpd_id,
                                        ZB_GPDF_CMD_UNDEFINED,
                                        params->pairing_endpoint,
                                        ZB_AF_WILDCARD_PROFILE_ID,
                                        ZB_ZGP_ZCL_CLUSTER_ID_UNDEFINED, /* Match all cluster_ids */
                                        ZB_ZGP_ZCL_CMD_ID_UNDEFINED) != NULL);
    }
    else
    {
      zb_bool_t ent_added = ZB_FALSE;

      /* Check that at least one cluster matched */
      for (zb_uint8_t idx = 0; idx < params->num_of_reports; idx++)
      {
        zb_uint8_t len = params->reports[idx].len;
        zb_uint8_t *data = params->reports[idx].data;
        zb_uint8_t i = 0;

        while (i < len)
        {
          zb_uint16_t cluster_id;
          zb_uint8_t num_of_attr_records, client_server, cluster_role;
          zb_bool_t manuf_id_present;

          /* check Data point options */
          num_of_attr_records = (data[i] & 0x07) + 1;
          client_server = data[i] & 0x08;
          cluster_role = client_server ? ZB_ZCL_CLUSTER_SERVER_ROLE : ZB_ZCL_CLUSTER_CLIENT_ROLE;
          manuf_id_present = ZB_U2B(data[i] & 0x10);
          i++;

          ZB_LETOH16(&cluster_id, &data[i]);
          i += 2;

          // TRACE_MSG(TRACE_APP2, "cluster_id: 0x%x, client_server: %d, num_of_attr_records: %d",
          //                     (FMT__D_D_D, cluster_id, client_server ? 1 : 0, num_of_attr_records));

          /* check Cluster ID and role */
          if (   (cluster_role == ZB_ZCL_CLUSTER_SERVER_ROLE
                  && (cluster_id == ZB_ZCL_CLUSTER_ID_BASIC             ||
                      cluster_id == ZB_ZCL_CLUSTER_ID_POWER_CONFIG      ||
                      cluster_id == ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT  ||
                      cluster_id == ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING ||
                      cluster_id == ZB_ZCL_CLUSTER_ID_IAS_ZONE) )
              || (cluster_role == ZB_ZCL_CLUSTER_CLIENT_ROLE
                  && (cluster_id == ZB_ZCL_CLUSTER_ID_SCENES            ||
                      cluster_id == ZB_ZCL_CLUSTER_ID_ON_OFF            ||
                      cluster_id == ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) ) )
          {
            // TRACE_MSG(TRACE_APP2, "cluster_id: 0x%x, supported", (FMT__D, cluster_id));
            accept = mapping_table_add_entry(&params->zgpd_id,
                                             ZB_GPDF_CMD_UNDEFINED,
                                             params->pairing_endpoint,
                                             ZB_AF_WILDCARD_PROFILE_ID,
                                             cluster_id,
                                             ZB_ZGP_ZCL_CMD_ID_UNDEFINED);
            if (accept == ZB_TRUE)
            {
              ent_added = ZB_TRUE;
            }
          }

          if (manuf_id_present)
          {
            i += 2;
          }

          /* skip Attribute records */
          while (num_of_attr_records--)
          {
            /* skip: Attribute ID (2) + Attribute Data Type (1) + Attribute Options (1)
             * + Remaining Attribute Record Length + 1 (from Attribute Options)
             */
            i += 2 + 1 + 1 + (data[i + 3] & 0x0FU) + 1;
          }
        }
      }

      accept = ent_added;
    }
  }
  else
  if (params->device_id == ZB_ZGP_GEN_8_CONT_SWITCH_DEV_ID)
  {
    zb_zgps_mapping_entry_t *entry;

    entry = mapping_table_add_entry(&params->zgpd_id,
                                    ZB_GPDF_CMD_8BIT_VECTOR_PRESS,
                                    params->pairing_endpoint,
                                    ZB_AF_WILDCARD_PROFILE_ID,
                                    ZB_ZCL_CLUSTER_ID_ON_OFF,
                                    ZB_ZCL_CMD_ON_OFF_ON_ID);

    if (entry)
    {
      app_mapping_entry_t *app_entry = (app_mapping_entry_t *)entry;

      app_entry->base.options = ZB_ZGP_MAPPING_ENTRY_OPTIONS(params->zgpd_id.app_id, 1);

      /* 5330 The Additional information block length field carries the total length in octets of the Additional in-
       * 5331 formation block, including the length of the Additional information block length field, decremented
       * 5332 by one.
       */
      app_entry->length.length = sizeof(app_mapping_entry_t) - sizeof(zb_zgps_mapping_entry_t) - 1;

      /* 5339 The bits b0 - b3 of the Option selector field indicate the total octet length of the following Option data
       * 5340 field, decremented by one.
       */
      app_entry->switch_option.selector.option_length = sizeof(zb_zgps_mapping_entry_generic_switch_option_data_t) - 1;
      app_entry->switch_option.selector.option_id = ZB_ZGPS_MAPPING_ENTRY_8BIT_VECTOR_OPTION_ID_GENERIC_SWITCH_COMMAND_EXECUTION;
      app_entry->switch_option.data.contact_status = CONTACT_STATUS_SWITCH_ON;
      app_entry->switch_option.data.contact_bitmask = 0x03;
    }

    entry = mapping_table_add_entry(&params->zgpd_id,
                                    ZB_GPDF_CMD_8BIT_VECTOR_PRESS,
                                    params->pairing_endpoint,
                                    ZB_AF_WILDCARD_PROFILE_ID,
                                    ZB_ZCL_CLUSTER_ID_ON_OFF,
                                    ZB_ZCL_CMD_ON_OFF_OFF_ID);

    if (entry)
    {
      app_mapping_entry_t *app_entry = (app_mapping_entry_t *)entry;

      /* 5330 The Additional information block length field carries the total length in octets of the Additional in-
       * 5331 formation block, including the length of the Additional information block length field, decremented
       * 5332 by one.
       */
      app_entry->length.length = sizeof(app_mapping_entry_t) - sizeof(zb_zgps_mapping_entry_t) - 1;

      /* 5339 The bits b0 - b3 of the Option selector field indicate the total octet length of the following Option data
       * 5340 field, decremented by one.
       */
      app_entry->switch_option.selector.option_length = sizeof(zb_zgps_mapping_entry_generic_switch_option_data_t) - 1;
      app_entry->switch_option.selector.option_id = ZB_ZGPS_MAPPING_ENTRY_8BIT_VECTOR_OPTION_ID_GENERIC_SWITCH_COMMAND_EXECUTION;
      app_entry->switch_option.data.contact_status = CONTACT_STATUS_SWITCH_OFF;
      app_entry->switch_option.data.contact_bitmask = 0x03;
    }

    accept = ZB_TRUE;
  }
  else
  {
    accept = ZB_FALSE;
  }

  // TRACE_MSG(TRACE_APP1, "accept_comm_cb, result %d", (FMT__D, accept));
  zb_zgps_accept_commissioning(accept);
}
/* [accept_comm] */

/**
   Callback for ZGP commissioning complete

   @param zgpd_id - commissioned ZGPD id (valid if result ==
   ZB_ZGP_COMMISSIONING_COMPLETED or ZB_ZGP_ZGPD_DECOMMISSIONED).
   @param result - commissioning status
 */
void comm_done_cb(zb_zgpd_id_t zgpd_id,
                  zb_zgp_comm_status_t result)
{
  // TRACE_MSG(TRACE_APP1, ">> comm_done_cb status %d", (FMT__D, result));
  if (result == ZB_ZGP_COMMISSIONING_COMPLETED || result == ZB_ZGP_ZGPD_DECOMMISSIONED)
  {
    //  TRACE_MSG(TRACE_APP1, "comm_done_cb app_id %hd endpoint %hd addr.src_id 0x%x addr.ieee_addr "TRACE_FORMAT_64,
    //            (FMT__H_H_D_A, zgpd_id.app_id, zgpd_id.endpoint, zgpd_id.addr.src_id,  TRACE_ARG_64(zgpd_id.addr.ieee_addr)));

    if (result == ZB_ZGP_ZGPD_DECOMMISSIONED)
    {
      mapping_table_delete_entry_by_zgpd_id(&zgpd_id);
      ZB_SCHEDULE_APP_ALARM(start_comm, 0,
                            ZB_MILLISECONDS_TO_BEACON_INTERVAL(1000));
    }
    else /*result == ZG_ZGP_COMMISSIONING_COMPLETED*/
    {
      Log_printf(LogModule_Zigbee_App, Log_INFO, "ZC: Commissioned ZGPD");
    }
  }
  Log_printf(LogModule_Zigbee_App, Log_INFO, "<< comm_done_cb");
}

/**
   Callback for ZGP mode change

   @param reason - @ref zb_zgp_mode_change_reason_t reason why the mode has changed
   @param new_mode - @ref zb_zgp_mode_t new mode of ZGP endpoint
 */
void mode_change_cb(zb_zgp_mode_t new_mode, zb_zgp_mode_change_reason_t reason)
{
  static zb_zgp_mode_t prev_mode = ZB_ZGP_OPERATIONAL_MODE;
  // TRACE_MSG(TRACE_APP1, ">> mode_change_cb new mode %hx cause: %hx", (FMT__H_H, new_mode, reason));

  if (new_mode == ZB_ZGP_OPERATIONAL_MODE)
  {
    /* Switched from comm mode to op mode */
    if (prev_mode == ZB_ZGP_COMMISSIONING_MODE)
    {
      switch(reason)
      {
        case ZB_ZGP_MODE_CHANGE_ON_FIRST_PARING_EXIT:
          Log_printf(LogModule_Zigbee_App, Log_INFO, "ZC: GP commissioning stopped: successful pairing");
          break;
        case ZB_ZGP_MODE_CHANGE_TIMEOUT:
          Log_printf(LogModule_Zigbee_App, Log_INFO, "ZC: GP commissioning started: timeout");
          break;
        default:
          Log_printf(LogModule_Zigbee_App, Log_INFO, "ZC: GP commissioning stopped");
          break;
      }
    }

#ifdef ZB_USE_BUTTONS
//    zb_led_blink_off(ZB_LED_ARG_CREATE(3, ZB_LED_BLINK_HALF_SEC));
#endif
  }
  else if (new_mode == ZB_ZGP_COMMISSIONING_MODE)
  {
    /* GP comm caused by user */
    if(reason == ZB_ZGP_MODE_CHANGE_TRIGGERED_BY_USER)
    {
      Log_printf(LogModule_Zigbee_App, Log_INFO, "ZC: GP commissioning started");
      
    }

#ifdef ZB_USE_BUTTONS
//    zb_led_blink_on(ZB_LED_ARG_CREATE(3, ZB_LED_BLINK_HALF_SEC));
#endif
  }

  prev_mode = new_mode;
  Log_printf(LogModule_Zigbee_App, Log_INFO, "<< mode_change_cb");
}

/**
   Start ZGP commissioning

   @param param - not used.
 */
//! [start_comm]
void start_comm(zb_uint8_t param)
{
  ZVUNUSED(param);
  Log_printf(LogModule_Zigbee_App, Log_INFO, "start commissioning");
  zb_osif_led_on(1);
  zb_zgps_start_commissioning_on_ep(ZB_OUTPUT_ENDPOINT, 0);
}
//! [start_comm]

/**
   Force ZGP commissioning stop

   @param param - not used.
 */
//! [stop_comm]
void stop_comm(zb_uint8_t param)
{
  ZVUNUSED(param);
  Log_printf(LogModule_Zigbee_App, Log_INFO, "stop commissioning");
  zb_osif_led_off(1);
  zb_zgps_stop_commissioning();
}
//! [stop_comm]

/* Callback to handle the stack events */
void zboss_signal_handler(zb_uint8_t param)
{
  zb_zdo_app_signal_hdr_t *sg_p = NULL;
  zb_zdo_app_signal_type_t sig = zb_get_app_signal(param, &sg_p);

  if (ZB_GET_APP_SIGNAL_STATUS(param) == 0)
  {
    switch(sig)
    {
#ifndef ZB_MACSPLIT_HOST
      case ZB_ZDO_SIGNAL_SKIP_STARTUP:
#else
      case ZB_MACSPLIT_DEVICE_BOOT:
#endif /* ZB_MACSPLIT_HOST */

#ifdef TEST_USE_INSTALLCODE
        zb_secur_ic_str_add(g_ed_addr, g_installcode, NULL);
#endif
        set_tx_power(DEFAULT_TX_PWR);
        zboss_start_continue();
        break;

      case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
      case ZB_BDB_SIGNAL_DEVICE_REBOOT:
      {
        zb_nwk_device_type_t device_type = ZB_NWK_DEVICE_TYPE_NONE;
        device_type = zb_get_device_type();
        ZVUNUSED(device_type);
        Log_printf(LogModule_Zigbee_App, Log_INFO, "Device (%d) STARTED OK", device_type);
        if (perform_factory_reset)
        {
          Log_printf(LogModule_Zigbee_App, Log_INFO, "Performing a factory reset.");
          zb_bdb_reset_via_local_action(0);
          perform_factory_reset = ZB_FALSE;
        }
        else if(ZB_BDB_SIGNAL_DEVICE_REBOOT == sig)
        {
          ZB_SCHEDULE_APP_ALARM_CANCEL(off_network_attention, ZB_ALARM_ANY_PARAM);
          zb_osif_led_off(1);
        }
        set_tx_power(DEFAULT_TX_PWR);
        bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);

#ifdef ZB_USE_BUTTONS
        zb_button_register_handler(0, 0, button_press_handler);
#endif
        break;
      }

      case ZB_ZGP_SIGNAL_APPROVE_COMMISSIONING:
      {
        zb_zgp_signal_approve_comm_params_t *app_comm_params = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, zb_zgp_signal_approve_comm_params_t);
        Log_printf(LogModule_Zigbee_App, Log_INFO, "ZB_ZGP_SIGNAL_APPROVE_COMMISSIONING");
        accept_comm_cb(app_comm_params->params);
      }
      break;
      /* [zgp_signal_approve_comm] */
      case ZB_ZGP_SIGNAL_COMMISSIONING:
      {
        zb_zgp_signal_commissioning_params_t *comm_params = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, zb_zgp_signal_commissioning_params_t);
        comm_done_cb(comm_params->zgpd_id, comm_params->result);
      }
      break;
      /* [zgp_signal_mode_change] */
      case ZB_ZGP_SIGNAL_MODE_CHANGE:
      {
        zb_zgp_signal_mode_change_params_t *signal_params = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, zb_zgp_signal_mode_change_params_t);
        mode_change_cb(signal_params->new_mode, signal_params->reason);
      }
      break;

      case ZB_COMMON_SIGNAL_CAN_SLEEP:
      {
#ifdef ZB_USE_SLEEP
        Log_printf(LogModule_Zigbee_App, Log_INFO, "Sleeping now");
        zb_sleep_now();
#endif
        break;
      }
      case ZB_BDB_SIGNAL_STEERING:
        Log_printf(LogModule_Zigbee_App, Log_INFO, "Successful steering, start f&b target");
        zb_bdb_finding_binding_target(ZB_OUTPUT_ENDPOINT);
        ZB_SCHEDULE_APP_ALARM_CANCEL(off_network_attention, ZB_ALARM_ANY_PARAM);
        zb_button_register_handler(0, 0, button_press_handler);
        zb_osif_led_off(1);
        break;

      default:
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "Unknown signal %d", (zb_uint16_t)sig);
    }
  }
  else
  {
    switch(sig)
    {
      case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "Device can not find any network on start, so try to perform network steering");
        ZB_SCHEDULE_APP_ALARM(restart_commissioning, 0, 10 * ZB_TIME_ONE_SECOND);
        break; /* ZB_BDB_SIGNAL_DEVICE_FIRST_START */

      case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
        Log_printf(LogModule_Zigbee_App, Log_INFO, "Production config is not present or invalid");
        break;

      case ZB_BDB_SIGNAL_STEERING:
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "Steering failed, retrying again in 10 seconds");
        ZB_SCHEDULE_APP_ALARM(restart_commissioning, 0, 10 * ZB_TIME_ONE_SECOND);
        break; /* ZB_BDB_SIGNAL_STEERING */

      default:
        Log_printf(LogModule_Zigbee_App, Log_INFO, "Device started FAILED status %d sig %d", ZB_GET_APP_SIGNAL_STATUS(param), sig);
    }
  }

  /* Free the buffer if it is not used */
  if (param)
  {
    zb_buf_free(param);
  }
}
