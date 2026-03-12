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
#define HA_DIMMABLE_LIGHT_ENDPOINT     5
#define ZB_OUTPUT_MAX_CMD_PAYLOAD_SIZE 2
#define ZB_HA_DEFINE_DEVICE_CUSTOM_DIMMABLE_LIGHT

#include <ti/log/Log.h>

#include "ti_zigbee_config.h"
#include "zboss_api.h"
#include "zb_led_button.h"
#include "zboss_api_error.h"

#include "zb_ha.h"
#include "ha/custom/zb_ha_custom_dimmable_light.h"

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_fcfg.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)

/* for button handling */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/timer/LGPTimerLPF3.h>
#include "ti_drivers_config.h"

#ifdef ZB_CONFIGURABLE_MEM
#include "zb_mem_config_lprf3.h"
#endif

#include <stdio.h>
#include <math.h>

#define TARGET_CNT_VALUE 24000

LGPTimerLPF3_Handle lgptHandle0;
LGPTimerLPF3_Handle lgptHandle1;
LGPTimerLPF3_Params params;

typedef enum {
    RGBW_CHANGE_COLOR,          
    RGBW_CHANGE_INTENSITY, 
    RGBW_CHANGE_OFF 
} RGBW_Change_States;

/****** Application variables declarations ******/
zb_uint16_t g_dst_addr;
zb_uint8_t g_addr_mode;
zb_uint8_t g_endpoint;
zb_bool_t perform_factory_reset = ZB_FALSE;

zb_uint8_t rVal = 0xFF;
zb_uint8_t grVal = 0x00;
zb_uint8_t bVal = 0x00;
zb_uint8_t wVal = 0x00;
zb_uint8_t saturation = 0xFF;
zb_uint8_t hue = 0x00;
zb_uint8_t value = 255; // Full Value
zb_uint8_t intensity = 0x0A; // duty of 4%

/****** Application function declarations ******/
/* Handler for specific ZCL commands */
zb_uint8_t zcl_specific_cluster_cmd_handler(zb_uint8_t param);
void test_device_interface_cb(zb_uint8_t param);
void button_press_handler(zb_uint8_t param);

/****** Cluster declarations ******/
/* Basic cluster attributes data */
zb_uint8_t g_attr_zcl_version = ZB_ZCL_VERSION;
zb_uint8_t g_attr_app_version;
zb_uint8_t g_attr_stack_version;
zb_uint8_t g_attr_hw_version;
zb_char_t g_attr_mf_name[32];
zb_char_t g_attr_model_id[16];
zb_char_t g_attr_date_code[10];
zb_uint8_t g_attr_power_source = ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN;
zb_char_t g_attr_location_id[5];
zb_uint8_t g_attr_ph_env;
zb_char_t g_attr_sw_build_id[] = ZB_ZCL_BASIC_SW_BUILD_ID_DEFAULT_VALUE;

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(
  basic_attr_list,
  &g_attr_zcl_version,
  &g_attr_app_version,
  &g_attr_stack_version,
  &g_attr_hw_version,
  &g_attr_mf_name,
  &g_attr_model_id,
  &g_attr_date_code,
  &g_attr_power_source,
  &g_attr_location_id,
  &g_attr_ph_env,
  &g_attr_sw_build_id);

/* Identify cluster attributes data */
zb_uint16_t g_attr_identify_time = 0;

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list,
  &g_attr_identify_time);

/* Groups cluster attributes data */
zb_uint8_t g_attr_name_support = 0;

ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST(groups_attr_list, &g_attr_name_support);

/* Scenes cluster attribute data */
zb_uint8_t g_attr_scenes_scene_count;
zb_uint8_t g_attr_scenes_current_scene;
zb_uint16_t g_attr_scenes_current_group;
zb_uint8_t g_attr_scenes_scene_valid;
zb_uint8_t g_attr_scenes_name_support;

ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST(scenes_attr_list, &g_attr_scenes_scene_count,
    &g_attr_scenes_current_scene, &g_attr_scenes_current_group,
    &g_attr_scenes_scene_valid, &g_attr_scenes_name_support);


/* On/Off cluster attributes data */
zb_uint8_t g_attr_on_off  = ZB_FALSE;

#ifdef ZB_ENABLE_ZLL
/* On/Off cluster attributes additions data */
zb_bool_t g_attr_global_scene_ctrl  = ZB_TRUE;
zb_uint16_t g_attr_on_time  = 0;
zb_uint16_t g_attr_off_wait_time  = 0;

ZB_ZCL_DECLARE_ON_OFF_ATTRIB_LIST_ZLL(on_off_attr_list, &g_attr_on_off,
    &g_attr_global_scene_ctrl, &g_attr_on_time, &g_attr_off_wait_time);
#else
ZB_ZCL_DECLARE_ON_OFF_ATTRIB_LIST(on_off_attr_list, &g_attr_on_off);
#endif

/* Level Control cluster attribute variables */
zb_uint8_t g_current_level = ZB_ZCL_LEVEL_CONTROL_CURRENT_LEVEL_DEFAULT_VALUE;
zb_uint16_t g_remaining_time = ZB_ZCL_LEVEL_CONTROL_REMAINING_TIME_DEFAULT_VALUE;

ZB_ZCL_DECLARE_LEVEL_CONTROL_ATTRIB_LIST(level_control_attr_list, &g_current_level, &g_remaining_time);

/* Color Control cluster attribute variables */
zb_uint8_t g_color_control_current_hue = ZB_ZCL_COLOR_CONTROL_CURRENT_HUE_MIN_VALUE;
zb_uint8_t g_color_control_current_saturation = ZB_ZCL_COLOR_CONTROL_CURRENT_SATURATION_MIN_VALUE;
zb_uint16_t g_color_control_remaining_time = ZB_ZCL_COLOR_CONTROL_REMAINING_TIME_MIN_VALUE;
zb_uint16_t g_color_control_current_X = ZB_ZCL_COLOR_CONTROL_CURRENT_X_DEF_VALUE;
zb_uint16_t g_color_control_current_Y = ZB_ZCL_COLOR_CONTROL_CURRENT_Y_DEF_VALUE;
zb_uint16_t g_color_control_color_temperature = ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_DEF_VALUE;
zb_uint8_t g_color_control_color_mode = ZB_ZCL_COLOR_CONTROL_COLOR_MODE_HUE_SATURATION;
zb_uint8_t g_color_control_options = ZB_ZCL_COLOR_CONTROL_OPTIONS_DEFAULT_VALUE;

zb_uint8_t g_color_control_number_primaries = ZB_ZCL_COLOR_CONTROL_NUMBER_OF_PRIMARIES_MAX_VALUE;
zb_uint16_t g_color_control_primary_1_X = 0;
zb_uint16_t g_color_control_primary_1_Y = 0;
zb_uint8_t g_color_control_primary_1_intensity = 1;
zb_uint16_t g_color_control_primary_2_X = 0;
zb_uint16_t g_color_control_primary_2_Y = 0;
zb_uint8_t g_color_control_primary_2_intensity = 2;
zb_uint16_t g_color_control_primary_3_X = 0;
zb_uint16_t g_color_control_primary_3_Y = 0;
zb_uint8_t g_color_control_primary_3_intensity = 3;
zb_uint16_t g_color_control_primary_4_X = 0;
zb_uint16_t g_color_control_primary_4_Y = 0;
zb_uint8_t g_color_control_primary_4_intensity = 4;
zb_uint16_t g_color_control_primary_5_X = 0;
zb_uint16_t g_color_control_primary_5_Y = 0;
zb_uint8_t g_color_control_primary_5_intensity = 5;
zb_uint16_t g_color_control_primary_6_X = 0;
zb_uint16_t g_color_control_primary_6_Y = 0;
zb_uint8_t g_color_control_primary_6_intensity = 6;

zb_uint16_t g_color_control_enhanced_current_hue = ZB_ZCL_COLOR_CONTROL_ENHANCED_CURRENT_HUE_DEFAULT_VALUE;
zb_uint8_t g_color_control_enhanced_color_mode = ZB_ZCL_COLOR_CONTROL_ENHANCED_COLOR_MODE_DEFAULT_VALUE;
zb_uint8_t g_color_control_color_loop_active = ZB_ZCL_COLOR_CONTROL_COLOR_LOOP_ACTIVE_DEFAULT_VALUE;
zb_uint8_t g_color_control_color_loop_direction = ZB_ZCL_COLOR_CONTROL_COLOR_LOOP_DIRECTION_DEFAULT_VALUE;
zb_uint16_t g_color_control_color_loop_time = ZB_ZCL_COLOR_CONTROL_COLOR_LOOP_TIME_DEF_VALUE;
zb_uint16_t g_color_control_color_loop_start = ZB_ZCL_COLOR_CONTROL_COLOR_LOOP_START_DEF_VALUE;
zb_uint16_t g_color_control_color_loop_stored = ZB_ZCL_COLOR_CONTROL_COLOR_LOOP_STORED_ENHANCED_HUE_DEFAULT_VALUE;
zb_uint16_t g_color_control_color_capabilities = 0x11;
zb_uint16_t g_color_control_color_temp_physical_min = ZB_ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_DEFAULT_VALUE;
zb_uint16_t g_color_control_color_temp_physical_max = ZB_ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_DEFAULT_VALUE;
zb_uint16_t g_color_control_couple_color_temp_to_level_min = ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_MIN_VALUE;
zb_uint16_t g_color_control_start_up_color_temp = ZB_ZCL_COLOR_CONTROL_START_UP_COLOR_TEMPERATURE_MAX_VALUE;

ZB_ZCL_DECLARE_COLOR_CONTROL_ATTRIB_LIST_EXT(color_control_attr_list,
    &g_color_control_current_hue, &g_color_control_current_saturation, &g_color_control_remaining_time,
    &g_color_control_current_X, &g_color_control_current_Y,
    &g_color_control_color_temperature, &g_color_control_color_mode, &g_color_control_options,
    &g_color_control_number_primaries,
    &g_color_control_primary_1_X, &g_color_control_primary_1_Y, &g_color_control_primary_1_intensity,
    &g_color_control_primary_2_X, &g_color_control_primary_2_Y, &g_color_control_primary_2_intensity,
    &g_color_control_primary_3_X, &g_color_control_primary_3_Y, &g_color_control_primary_3_intensity,
    &g_color_control_primary_4_X, &g_color_control_primary_4_Y, &g_color_control_primary_4_intensity,
    &g_color_control_primary_5_X, &g_color_control_primary_5_Y, &g_color_control_primary_5_intensity,
    &g_color_control_primary_6_X, &g_color_control_primary_6_Y, &g_color_control_primary_6_intensity,
    &g_color_control_enhanced_current_hue, &g_color_control_enhanced_color_mode, 
    &g_color_control_color_loop_active, &g_color_control_color_loop_direction, 
    &g_color_control_color_loop_time, &g_color_control_color_loop_start, 
    &g_color_control_color_loop_stored, &g_color_control_color_capabilities, 
    &g_color_control_color_temp_physical_min, &g_color_control_color_temp_physical_max, 
    &g_color_control_couple_color_temp_to_level_min, &g_color_control_start_up_color_temp);

/********************* Declare device **************************/

ZB_HA_DECLARE_CUSTOM_DIMMABLE_LIGHT_CLUSTER_LIST(
  dimmable_light_clusters, basic_attr_list,
  identify_attr_list,
  groups_attr_list,
  scenes_attr_list,
  on_off_attr_list,
  level_control_attr_list,
  color_control_attr_list);

ZB_HA_DECLARE_CUSTOM_DIMMABLE_LIGHT_EP(
  dimmable_light_ep, HA_DIMMABLE_LIGHT_ENDPOINT,
  dimmable_light_clusters);

ZB_HA_DECLARE_CUSTOM_DIMMABLE_LIGHT_CTX(
  dimmable_light_ctx,
  dimmable_light_ep);


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

void rgbwChange(zb_uint8_t param)
{
  zb_uint8_t region, remainder, p, q, t;
  switch(param)
  {
    case RGBW_CHANGE_COLOR:
      if(!saturation) // Achromatic (gray)
      {    
        rVal = value;
        grVal = value;
        bVal = value;
      }
      else 
      {
        // Convert H to a value from 0 to 1530 (6 * 255)
        // The range 0-255 for H corresponds to 0-360 degrees
        // We can work in this scaled integer space
        uint16_t scaled_h = (uint16_t)hue * 6; 
        region = scaled_h / 255; // Which sextant (0-5)
        remainder = scaled_h % 255; // Remainder within the sextant

        // Calculate p, q, t
        // p = V * (1 - S)
        // q = V * (1 - S * f)
        // t = V * (1 - S * (1 - f))
        // Using integer math with scaling by 255 to avoid floats:
        p = (value * (255 - saturation)) / 255;
        q = (value * (255 - saturation * remainder / 255)) / 255; // Note: integer division order matters
        t = (value * (255 - saturation * (255 - remainder) / 255)) / 255;

        switch (region) 
        {
        case 0: // [0, 60) degrees
            rVal = value;
            grVal = t;
            bVal = p;
            break;
        case 1: // [60, 120) degrees
            rVal = q;
            grVal = value;
            bVal = p;
            break;
        case 2: // [120, 180) degrees
            rVal = p;
            grVal = value;
            bVal = t;
            break;
        case 3: // [180, 240) degrees
            rVal = p;
            grVal = q;
            bVal = value;
            break;
        case 4: // [240, 300) degrees
            rVal = t;
            grVal = p;
            bVal = value;
            break;
        default: // case 5: [300, 360) degrees
            rVal = value;
            grVal = p;
            bVal = q;
            break;
        }
      }

      LGPTimerLPF3_setNextChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_0, \
          (TARGET_CNT_VALUE*rVal/255)*intensity/255, false);
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_1, \
          (TARGET_CNT_VALUE*grVal/255)*intensity/255, false);
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_2, \
          (TARGET_CNT_VALUE*bVal/255)*intensity/255, false);
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle1, LGPTimerLPF3_CH_NO_2, \
          (TARGET_CNT_VALUE*wVal/255)*intensity/255, false);

      break;      
    case RGBW_CHANGE_INTENSITY:
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_0, \
          (TARGET_CNT_VALUE*rVal/255)*intensity/255, false);
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_1, \
          (TARGET_CNT_VALUE*grVal/255)*intensity/255, false);
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_2, \
          (TARGET_CNT_VALUE*bVal/255)*intensity/255, false);
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle1, LGPTimerLPF3_CH_NO_2, \
          (TARGET_CNT_VALUE*wVal/255)*intensity/255, false);
      break;
    case RGBW_CHANGE_OFF:
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_0, 0, false);
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_1, 0, false);
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_2, 0, false);
      LGPTimerLPF3_setNextChannelCompVal(lgptHandle1, LGPTimerLPF3_CH_NO_2, 0, false);
      break;
  }
}

MAIN()
{
  ARGV_UNUSED;

  /* Global ZBOSS initialization */
  ZB_INIT("color_light");

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
  ZB_AF_REGISTER_DEVICE_CTX(&dimmable_light_ctx);

  /* Register cluster commands handler for a specific endpoint */
  ZB_AF_SET_ENDPOINT_HANDLER(HA_DIMMABLE_LIGHT_ENDPOINT,
                             zcl_specific_cluster_cmd_handler);
  /* Set Device user application callback */
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

    zb_osif_led_button_init();
#ifndef ZB_COORDINATOR_ROLE
    ZB_SCHEDULE_APP_ALARM(off_network_attention, 0, 1 * ZB_TIME_ONE_SECOND);
#endif /* ZB_COORDINATOR_ROLE */

    // Configure channels action
    LGPTimerLPF3_Params_init(&params);
    params.channelProperty[2].action = LGPTimerLPF3_CH_SET_ON_0_TOGGLE_ON_CMP_PERIODIC;
    lgptHandle1 = LGPTimerLPF3_open(CONFIG_LGPTIMER_1, &params);
    params.channelProperty[1].action = LGPTimerLPF3_CH_SET_ON_0_TOGGLE_ON_CMP_PERIODIC;
    params.channelProperty[0].action = LGPTimerLPF3_CH_SET_ON_0_TOGGLE_ON_CMP_PERIODIC;

    // Open driver
    lgptHandle0 = LGPTimerLPF3_open(CONFIG_LGPTIMER_0, &params);

    // Set channel output signal period
    LGPTimerLPF3_setInitialCounterTarget(lgptHandle0, TARGET_CNT_VALUE, false);
    LGPTimerLPF3_setInitialCounterTarget(lgptHandle1, TARGET_CNT_VALUE, false);

    // Set channel output signal duty cycle
    LGPTimerLPF3_setInitialChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_0, \
        (TARGET_CNT_VALUE*rVal/255)*intensity/255, false);
    LGPTimerLPF3_setInitialChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_1, \
        (TARGET_CNT_VALUE*grVal/255)*intensity/255, false); 
    LGPTimerLPF3_setInitialChannelCompVal(lgptHandle0, LGPTimerLPF3_CH_NO_2, \
        (TARGET_CNT_VALUE*bVal/255)*intensity/255, false);
    LGPTimerLPF3_setInitialChannelCompVal(lgptHandle1, LGPTimerLPF3_CH_NO_2, \
        (TARGET_CNT_VALUE*wVal/255)*intensity/255, false);

    // Start the LGPTimer in up-down-periodic mode
    LGPTimerLPF3_start(lgptHandle0, LGPTimerLPF3_CTL_MODE_UPDWN_PER);
    LGPTimerLPF3_start(lgptHandle1, LGPTimerLPF3_CTL_MODE_UPDWN_PER);

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
    case ZB_ZCL_LEVEL_CONTROL_SET_VALUE_CB_ID:
      intensity = device_cb_param->cb_param.level_control_set_value_param.new_value/10;
      ZB_SCHEDULE_APP_CALLBACK(rgbwChange, RGBW_CHANGE_INTENSITY);
      break;
    case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
      if (device_cb_param->cb_param.set_attr_value_param.cluster_id == ZB_ZCL_CLUSTER_ID_ON_OFF &&
          device_cb_param->cb_param.set_attr_value_param.attr_id == ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID)
      {
        g_attr_on_off = device_cb_param->cb_param.set_attr_value_param.values.data8;
        if(!g_attr_on_off) ZB_SCHEDULE_APP_CALLBACK(rgbwChange, RGBW_CHANGE_OFF);
        else ZB_SCHEDULE_APP_CALLBACK(rgbwChange, RGBW_CHANGE_INTENSITY);
        Log_printf(LogModule_Zigbee_App, Log_INFO, "on/off setting: %hd", (FMT__H, device_cb_param->cb_param.set_attr_value_param.values.data8));
        device_cb_param->status = RET_OK;
      }
      else if (device_cb_param->cb_param.set_attr_value_param.cluster_id == ZB_ZCL_CLUSTER_ID_COLOR_CONTROL)
      {
        if (device_cb_param->cb_param.set_attr_value_param.attr_id == ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID)
        {
          hue = device_cb_param->cb_param.set_attr_value_param.values.data8;
          ZB_SCHEDULE_APP_ALARM(rgbwChange, RGBW_CHANGE_COLOR, ZB_MILLISECONDS_TO_BEACON_INTERVAL(100));
        }
        else if (device_cb_param->cb_param.set_attr_value_param.attr_id == ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID)
        {
          saturation = device_cb_param->cb_param.set_attr_value_param.values.data8;
        }
         /* Color control value update - check attr_id, send value to device h/w etc. */
        /* ZCL attr value will be updated automatically, it is not needed to do it here. */
        device_cb_param->status = RET_OK;
      }
      else
      {
        Log_printf(LogModule_Zigbee_App, Log_ERROR, "ZB_ZCL_SET_ATTR_VALUE_CB_ID: cluster_id %d", (FMT__D, device_cb_param->cb_param.set_attr_value_param.cluster_id));
        device_cb_param->status = RET_ERROR;
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
  ZVUNUSED(param);
  Log_printf(LogModule_Zigbee_App, Log_INFO, "button is pressed, do nothing");
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
        zb_bdb_finding_binding_target(HA_DIMMABLE_LIGHT_ENDPOINT);
        ZB_SCHEDULE_APP_ALARM_CANCEL(off_network_attention, ZB_ALARM_ANY_PARAM);
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
