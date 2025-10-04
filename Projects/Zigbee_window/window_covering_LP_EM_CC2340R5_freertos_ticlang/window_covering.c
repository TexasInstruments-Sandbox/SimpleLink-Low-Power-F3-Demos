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
#define ZB_WINDOW_COVERING_ENDPOINT    5
#define ZB_OUTPUT_MAX_CMD_PAYLOAD_SIZE 2

#include <ti/log/Log.h>

#include "ti_zigbee_config.h"
#include "zboss_api.h"
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
zb_bool_t inNetwork = ZB_FALSE;

/****** Application function declarations ******/
/* Handler for specific ZCL commands */
zb_uint8_t zcl_specific_cluster_cmd_handler(zb_uint8_t param);
void test_device_interface_cb(zb_uint8_t param);
void button_press_handler(zb_uint8_t param);

extern void motorCommand(uint8_t percentLift);

#define TEST_DEVICE_SCENES_TABLE_SIZE 3
zb_uint8_t test_device_scenes_get_entry(zb_uint16_t group_id, zb_uint8_t scene_id);
typedef struct test_device_scenes_table_entry_s
{
  zb_zcl_scene_table_record_fixed_t common;
  zb_uint8_t lift_percentage_state;
  zb_uint8_t tilt_percentage_state;
}
test_device_scenes_table_entry_t;
test_device_scenes_table_entry_t scenes_table[TEST_DEVICE_SCENES_TABLE_SIZE];

void test_device_scenes_table_init()
{
  zb_uint8_t i = 0;
  while (i < TEST_DEVICE_SCENES_TABLE_SIZE)
  {
    scenes_table[i].common.group_id = ZB_ZCL_SCENES_FREE_SCENE_TABLE_RECORD;
    ++i;
  }
}

zb_uint8_t test_device_scenes_get_entry(zb_uint16_t group_id, zb_uint8_t scene_id)
{
  zb_uint8_t i = 0;
  zb_uint8_t idx = 0xFF, free_idx = 0xFF;

  while (i < TEST_DEVICE_SCENES_TABLE_SIZE)
  {
    if (scenes_table[i].common.group_id == group_id &&
        scenes_table[i].common.group_id == scene_id)
    {
      idx = i;
      break;
    }
    else if (free_idx == 0xFF &&
             scenes_table[i].common.group_id == ZB_ZCL_SCENES_FREE_SCENE_TABLE_RECORD)
    {
      /* Remember free index */
      free_idx = i;
    }
    ++i;
  }

  return ((idx != 0xFF) ? idx : free_idx);
}

/* Basic cluster attributes data */
zb_uint8_t g_attr_basic_zcl_version = ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_application_version = ZB_ZCL_BASIC_APPLICATION_VERSION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_stack_version = ZB_ZCL_BASIC_STACK_VERSION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_hw_version = ZB_ZCL_BASIC_HW_VERSION_DEFAULT_VALUE;
zb_char_t g_attr_basic_manufacturer_name[] = ZB_ZCL_BASIC_MANUFACTURER_NAME_DEFAULT_VALUE;
zb_char_t g_attr_basic_model_identifier[] = ZB_ZCL_BASIC_MODEL_IDENTIFIER_DEFAULT_VALUE;
zb_char_t g_attr_basic_date_code[] = ZB_ZCL_BASIC_DATE_CODE_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_power_source = ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE;
zb_char_t g_attr_basic_location_description[] = ZB_ZCL_BASIC_LOCATION_DESCRIPTION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_physical_environment = ZB_ZCL_BASIC_PHYSICAL_ENVIRONMENT_DEFAULT_VALUE;
zb_char_t g_attr_sw_build_id[] = ZB_ZCL_BASIC_SW_BUILD_ID_DEFAULT_VALUE;
ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_attr_list, &g_attr_basic_zcl_version, &g_attr_basic_application_version, &g_attr_basic_stack_version, &g_attr_basic_hw_version, &g_attr_basic_manufacturer_name, &g_attr_basic_model_identifier, &g_attr_basic_date_code, &g_attr_basic_power_source, &g_attr_basic_location_description, &g_attr_basic_physical_environment, &g_attr_sw_build_id);

/* Identify cluster attributes data */
zb_uint16_t g_attr_identify_identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
/* Groups cluster attributes data */
zb_uint8_t g_attr_groups_name_support = 0;
/* Scenes cluster attributes data */
zb_uint8_t g_attr_scenes_scene_count = ZB_ZCL_SCENES_SCENE_COUNT_DEFAULT_VALUE;
zb_uint8_t g_attr_scenes_current_scene = ZB_ZCL_SCENES_CURRENT_SCENE_DEFAULT_VALUE;
zb_uint16_t g_attr_scenes_current_group = ZB_ZCL_SCENES_CURRENT_GROUP_DEFAULT_VALUE;
zb_uint8_t g_attr_scenes_scene_valid = ZB_ZCL_SCENES_SCENE_VALID_DEFAULT_VALUE;
zb_uint8_t g_attr_scenes_name_support = 0;
/* Window Covering cluster attributes data */
zb_uint8_t g_attr_window_covering_window_covering_type = ZB_ZCL_WINDOW_COVERING_WINDOW_COVERING_TYPE_DEFAULT_VALUE;
zb_uint8_t g_attr_window_covering_config_status = ZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_DEFAULT_VALUE;
zb_uint8_t g_attr_window_covering_current_position_lift_percentage = ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_DEFAULT_VALUE;
zb_uint8_t g_attr_window_covering_current_position_tilt_percentage = 0x32;//ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_DEFAULT_VALUE;
zb_uint16_t g_attr_window_covering_installed_open_limit_lift = ZB_ZCL_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_DEFAULT_VALUE;
zb_uint16_t g_attr_window_covering_installed_closed_limit_lift = ZB_ZCL_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_DEFAULT_VALUE;
zb_uint16_t g_attr_window_covering_installed_open_limit_tilt = ZB_ZCL_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_DEFAULT_VALUE;
zb_uint16_t g_attr_window_covering_installed_closed_limit_tilt = ZB_ZCL_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_DEFAULT_VALUE;
zb_uint8_t g_attr_window_covering_mode = ZB_ZCL_WINDOW_COVERING_MODE_DEFAULT_VALUE;
ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &g_attr_identify_identify_time);
ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST(groups_attr_list, &g_attr_groups_name_support);
ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST(scenes_attr_list, &g_attr_scenes_scene_count, &g_attr_scenes_current_scene, &g_attr_scenes_current_group, &g_attr_scenes_scene_valid, &g_attr_scenes_name_support);
ZB_ZCL_DECLARE_WINDOW_COVERING_CLUSTER_ATTRIB_LIST(window_covering_attr_list, &g_attr_window_covering_window_covering_type, &g_attr_window_covering_config_status, &g_attr_window_covering_current_position_lift_percentage, &g_attr_window_covering_current_position_tilt_percentage, &g_attr_window_covering_installed_open_limit_lift, &g_attr_window_covering_installed_closed_limit_lift, &g_attr_window_covering_installed_open_limit_tilt, &g_attr_window_covering_installed_closed_limit_tilt, &g_attr_window_covering_mode);
/********************* Declare device **************************/
ZB_HA_DECLARE_WINDOW_COVERING_CLUSTER_LIST(window_covering_clusters, basic_attr_list, identify_attr_list, groups_attr_list, scenes_attr_list, window_covering_attr_list);
ZB_HA_DECLARE_WINDOW_COVERING_EP(window_covering_ep, ZB_WINDOW_COVERING_ENDPOINT, window_covering_clusters);
ZB_HA_DECLARE_WINDOW_COVERING_CTX(device_ctx, window_covering_ep);

static zb_bool_t error_ind_handler(zb_uint8_t severity,
                                   zb_ret_t error_code,
                                   void *additional_info);


MAIN()
{
  ARGV_UNUSED;

  /* Global ZBOSS initialization */
  ZB_INIT("window_covering");

//  zboss_use_r22_behavior();

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
  zb_nwk_set_max_ed_capacity(MAX_ED_CAPACITY);

#elif defined ZB_ROUTER_ROLE && !defined ZB_COORDINATOR_ROLE
  zb_set_network_router_role(DEFAULT_CHANLIST);
  zb_nwk_set_max_ed_capacity(MAX_ED_CAPACITY);

  /* Set keepalive mode to mac data poll so sleepy zeds consume less power */
  zb_set_keepalive_mode(MAC_DATA_POLL_KEEPALIVE); 

#elif defined ZB_ED_ROLE
  zb_set_network_ed_role(DEFAULT_CHANLIST);

  /* Set end-device configuration parameters */
  zb_set_ed_timeout(ED_TIMEOUT_VALUE);
  zb_set_rx_on_when_idle(ED_RX_ALWAYS_ON);
#if ( ED_RX_ALWAYS_ON == ZB_FALSE )
  zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(ED_POLL_RATE));
#ifdef DISABLE_TURBO_POLL
  // Disable turbo poll feature
  zb_zdo_pim_permit_turbo_poll(ZB_FALSE);
  zb_zdo_pim_set_long_poll_interval(ED_POLL_RATE);
#endif // DISABLE_TURBO_POLL
#endif //ED_RX_ALWAYS_ON
#endif //ZB_ED_ROLE

  zb_set_nvram_erase_at_start(ZB_FALSE);

#ifdef ZB_ENABLE_PTA
  zb_enable_pta(0);
#endif

  /* Register device list */
  ZB_AF_REGISTER_DEVICE_CTX(&device_ctx);
  ZB_AF_SET_ENDPOINT_HANDLER(ZB_WINDOW_COVERING_ENDPOINT, zcl_specific_cluster_cmd_handler);
  ZB_ZCL_REGISTER_DEVICE_CB(test_device_interface_cb);

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
    /* Call the main loop */
    zboss_main_loop();
  }

  MAIN_RETURN(0);
}

/*
 * Callback used in bdc_motor to update Zigbee lift percentage value
 */
void updateWindowPosition(zb_uint8_t lift)
{
  if(inNetwork)
  {
    ZVUNUSED(zb_zcl_set_attr_val(ZB_WINDOW_COVERING_ENDPOINT,
                                  ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
                                  ZB_ZCL_CLUSTER_SERVER_ROLE,
                                  ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
                                  &lift,
                                  ZB_FALSE));
  }
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
  TRACE_MSG(TRACE_APP1, "> test_device_interface_cb param %hd id %hd", (FMT__H_H,
      param, device_cb_param->device_cb_id));
  device_cb_param->status = RET_OK;
  switch (device_cb_param->device_cb_id)
  {
    case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
      if (device_cb_param->cb_param.set_attr_value_param.cluster_id == ZB_ZCL_CLUSTER_ID_ON_OFF &&
          device_cb_param->cb_param.set_attr_value_param.attr_id == ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID)
      {
        if (device_cb_param->cb_param.set_attr_value_param.values.data8)
        {
          TRACE_MSG(TRACE_APP1, "set ON", (FMT__0));
#ifdef ZB_USE_BUTTONS
          zb_osif_led_on(0);
#endif
        }
        else
        {
          TRACE_MSG(TRACE_APP1, "set OFF", (FMT__0));
#ifdef ZB_USE_BUTTONS
          zb_osif_led_off(0);
#endif
        }
      }
      break;
    case ZB_ZCL_WINDOW_COVERING_UP_OPEN_CB_ID:
    {
#ifdef ZB_USE_BUTTONS
//          zb_osif_led_on(0);
#endif
      motorCommand(0);
      // zb_uint8_t lift_percentage_val = 0x00;
      // zb_uint8_t tilt_percentage_val = 0x00;
      // ZVUNUSED(zb_zcl_set_attr_val(ZB_WINDOW_COVERING_ENDPOINT,
      //                              ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
      //                              ZB_ZCL_CLUSTER_SERVER_ROLE,
      //                              ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
      //                              &lift_percentage_val,
      //                              ZB_FALSE));
      // ZVUNUSED(zb_zcl_set_attr_val(ZB_WINDOW_COVERING_ENDPOINT,
      //                              ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
      //                              ZB_ZCL_CLUSTER_SERVER_ROLE,
      //                              ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID,
      //                              &tilt_percentage_val,
      //                              ZB_FALSE));
      break;
    }
    case ZB_ZCL_WINDOW_COVERING_DOWN_CLOSE_CB_ID:
    {
#ifdef ZB_USE_BUTTONS
//          zb_osif_led_off(0);
#endif
      motorCommand(100);
      // zb_uint8_t lift_percentage_val = 0x64;
      // zb_uint8_t tilt_percentage_val = 0x64;
      // ZVUNUSED(zb_zcl_set_attr_val(ZB_WINDOW_COVERING_ENDPOINT,
      //                              ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
      //                              ZB_ZCL_CLUSTER_SERVER_ROLE,
      //                              ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
      //                              &lift_percentage_val,
      //                              ZB_FALSE));
      // ZVUNUSED(zb_zcl_set_attr_val(ZB_WINDOW_COVERING_ENDPOINT,
      //                              ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
      //                              ZB_ZCL_CLUSTER_SERVER_ROLE,
      //                              ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID,
      //                              &tilt_percentage_val,
      //                              ZB_FALSE));
      break;
    }
    case ZB_ZCL_WINDOW_COVERING_STOP_CB_ID:
      break;
    case ZB_ZCL_WINDOW_COVERING_GO_TO_LIFT_PERCENTAGE_CB_ID:
    {
      const zb_zcl_go_to_lift_percentage_req_t *lift_percentage = ZB_ZCL_DEVICE_CMD_PARAM_IN_GET(param, zb_zcl_go_to_lift_percentage_req_t);
      zb_zcl_attr_t *attr_desc = zb_zcl_get_attr_desc_a(ZB_WINDOW_COVERING_ENDPOINT,
          ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID);
      motorCommand(lift_percentage->percentage_lift_value);
      // if (attr_desc)
      // {
      //   ZVUNUSED(zb_zcl_set_attr_val(ZB_WINDOW_COVERING_ENDPOINT,
      //                                ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
      //                                ZB_ZCL_CLUSTER_SERVER_ROLE,
      //                                ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
      //                                (zb_uint8_t *)&(lift_percentage->percentage_lift_value),
      //                                ZB_FALSE));
      // }
      break;
    }
    case ZB_ZCL_WINDOW_COVERING_GO_TO_TILT_PERCENTAGE_CB_ID:
    {
      const zb_zcl_go_to_tilt_percentage_req_t *tilt_percentage = ZB_ZCL_DEVICE_CMD_PARAM_IN_GET(param, zb_zcl_go_to_tilt_percentage_req_t);
      zb_zcl_attr_t *attr_desc = zb_zcl_get_attr_desc_a(ZB_WINDOW_COVERING_ENDPOINT,
            ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID);
      if (attr_desc)
      {
        ZVUNUSED(zb_zcl_set_attr_val(ZB_WINDOW_COVERING_ENDPOINT,
                                     ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
                                     ZB_ZCL_CLUSTER_SERVER_ROLE,
                                     ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID,
                                     (zb_uint8_t *)&(tilt_percentage->percentage_tilt_value),
                                     ZB_FALSE));
      }
      break;
    }
    /* >>>> Scenes */
    case ZB_ZCL_SCENES_STORE_SCENE_CB_ID:
    {
      const zb_zcl_scenes_store_scene_req_t *store_scene_req = ZB_ZCL_DEVICE_CMD_PARAM_IN_GET(param, zb_zcl_scenes_store_scene_req_t);
      zb_uint8_t idx = 0xFF;
      zb_uint8_t *store_scene_status = ZB_ZCL_DEVICE_CMD_PARAM_OUT_GET(param, zb_uint8_t);
      const zb_zcl_parsed_hdr_t *in_cmd_info = ZB_ZCL_DEVICE_CMD_PARAM_CMD_INFO(param);
      TRACE_MSG(TRACE_APP1, "ZB_ZCL_SCENES_STORE_SCENE_CB_ID: group_id 0x%x scene_id 0x%hd", (FMT__D_H, store_scene_req->group_id, store_scene_req->scene_id));
      if (!zb_aps_is_endpoint_in_group(
                 store_scene_req->group_id,
                 ZB_ZCL_PARSED_HDR_SHORT_DATA(in_cmd_info).dst_endpoint))
      {
        *store_scene_status = ZB_ZCL_STATUS_INVALID_FIELD;
      }
      else
      {
        idx = test_device_scenes_get_entry(store_scene_req->group_id, store_scene_req->scene_id);
        if (idx != 0xFF)
        {
          if (scenes_table[idx].common.group_id != ZB_ZCL_SCENES_FREE_SCENE_TABLE_RECORD)
          {
            /* Update existing entry with current On/Off state */
            device_cb_param->status = RET_ALREADY_EXISTS;
            scenes_table[idx].lift_percentage_state = g_attr_window_covering_current_position_lift_percentage;
            scenes_table[idx].tilt_percentage_state = g_attr_window_covering_current_position_tilt_percentage;
          }
          else
          {
            /* Create new entry with empty name and 0 transition time */
            scenes_table[idx].common.group_id = store_scene_req->group_id;
            scenes_table[idx].common.scene_id = store_scene_req->scene_id;
            scenes_table[idx].common.transition_time = 0;
            scenes_table[idx].lift_percentage_state = g_attr_window_covering_current_position_lift_percentage;
            scenes_table[idx].tilt_percentage_state = g_attr_window_covering_current_position_tilt_percentage;
          }
          *store_scene_status = ZB_ZCL_STATUS_SUCCESS;
        }
        else
        {
          *store_scene_status = ZB_ZCL_STATUS_INSUFF_SPACE;
        }
      }
    }
    break;
    case ZB_ZCL_SCENES_RECALL_SCENE_CB_ID:
    {
      const zb_zcl_scenes_recall_scene_req_t *recall_scene_req = ZB_ZCL_DEVICE_CMD_PARAM_IN_GET(param, zb_zcl_scenes_recall_scene_req_t);
      zb_uint8_t idx = 0xFF;
      zb_uint8_t *recall_scene_status = ZB_ZCL_DEVICE_CMD_PARAM_OUT_GET(param, zb_uint8_t);
      TRACE_MSG(TRACE_APP1, "ZB_ZCL_SCENES_RECALL_SCENE_CB_ID: group_id 0x%x scene_id 0x%hd", (FMT__D_H, recall_scene_req->group_id, recall_scene_req->scene_id));
      idx = test_device_scenes_get_entry(recall_scene_req->group_id, recall_scene_req->scene_id);
      if (idx != 0xFF &&
          scenes_table[idx].common.group_id != ZB_ZCL_SCENES_FREE_SCENE_TABLE_RECORD)
      {
        /* Recall this entry */
        ZB_ZCL_SET_ATTRIBUTE(
          ZB_WINDOW_COVERING_ENDPOINT,
          ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
          ZB_ZCL_CLUSTER_SERVER_ROLE,
          ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
          &scenes_table[idx].lift_percentage_state,
          ZB_FALSE);
        ZB_ZCL_SET_ATTRIBUTE(
          ZB_WINDOW_COVERING_ENDPOINT,
          ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
          ZB_ZCL_CLUSTER_SERVER_ROLE,
          ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID,
          &scenes_table[idx].tilt_percentage_state,
          ZB_FALSE);
        *recall_scene_status = ZB_ZCL_STATUS_SUCCESS;
      }
      else
      {
        *recall_scene_status = ZB_ZCL_STATUS_NOT_FOUND;
      }
    }
    break;
    case ZB_ZCL_SCENES_INTERNAL_REMOVE_ALL_SCENES_ALL_ENDPOINTS_ALL_GROUPS_CB_ID:
    {
      test_device_scenes_table_init();
    }
    break;
      /* <<<< Scenes */
    default:
      device_cb_param->status = RET_OK;
      break;
  }
  TRACE_MSG(TRACE_APP1, "< test_device_interface_cb %hd", (FMT__H, device_cb_param->status));
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
  zb_uint8_t lqi = ZB_MAC_LQI_UNDEFINED;
  zb_int8_t rssi = ZB_MAC_RSSI_UNDEFINED;
  TRACE_MSG(TRACE_APP1, "> zcl_specific_cluster_cmd_handler", (FMT__0));
  ZB_ZCL_COPY_PARSED_HEADER(param, &cmd_info);
  g_dst_addr = ZB_ZCL_PARSED_HDR_SHORT_DATA(&cmd_info).source.u.short_addr;
  ZB_ZCL_DEBUG_DUMP_HEADER(&cmd_info);
  TRACE_MSG(TRACE_APP3, "payload size: %i", (FMT__D, zb_buf_len(param)));
  zb_zdo_get_diag_data(g_dst_addr, &lqi, &rssi);
  TRACE_MSG(TRACE_APP3, "lqi %hd rssi %d", (FMT__H_H, lqi, rssi));
  if (cmd_info.cmd_direction == ZB_ZCL_FRAME_DIRECTION_TO_CLI)
  {
    TRACE_MSG(
        TRACE_ERROR,
        "Unsupported \"from server\" command direction",
        (FMT__0));
  }
  TRACE_MSG(TRACE_APP1, "< zcl_specific_cluster_cmd_handler", (FMT__0));
  return ZB_FALSE;
}

void button_press_handler(zb_uint8_t param)
{
  ZVUNUSED(param);
  Log_printf(LogModule_Zigbee_App, Log_INFO, "button is pressed, do nothing");
}

/* Callback to handle the stack events */
void zboss_signal_handler(zb_uint8_t param)
{
  zb_zdo_app_signal_type_t sig = zb_get_app_signal(param, NULL);

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
        bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);

#ifdef ZB_USE_BUTTONS
        zb_button_register_handler(0, 0, button_press_handler);
#endif
        break;
      }
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
        inNetwork = true;
        zb_bdb_finding_binding_target(ZB_WINDOW_COVERING_ENDPOINT);
        break;

      default:
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "Unknown signal %d", (zb_uint16_t)sig);
    }
  }
  else if (sig == ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY)
  {
    Log_printf(LogModule_Zigbee_App, Log_INFO, "Production config is not present or invalid");
  }
  else
  {
    Log_printf(LogModule_Zigbee_App, Log_INFO, "Device started FAILED status %d sig %d", ZB_GET_APP_SIGNAL_STATUS(param), sig);
  }

  /* Free the buffer if it is not used */
  if (param)
  {
    zb_buf_free(param);
  }
}
