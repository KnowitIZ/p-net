/*********************************************************************
 *        _       _         _
 *  _ __ | |_  _ | |  __ _ | |__   ___
 * | '__|| __|(_)| | / _` || '_ \ / __|
 * | |   | |_  _ | || (_| || |_) |\__ \
 * |_|    \__|(_)|_| \__,_||_.__/ |___/
 *
 * www.rt-labs.com
 * Copyright 2021 rt-labs AB, Sweden.
 *
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 ********************************************************************/

/* 
 *  _  __                    _ _              ___  _        __      __                
 * | |/ /_ __   _____      _(_) |_           / _ \| | ___  / _|___ / _| ___  _ __ ___ 
 * | ' /| '_ \ / _ \ \ /\ / / | __|  _____  | | | | |/ _ \| |_/ __| |_ / _ \| '__/ __|
 * | . \| | | | (_) \ V  V /| | |_  |_____| | |_| | | (_) |  _\__ \  _| (_) | |  \__ \
 * |_|\_\_| |_|\___/ \_/\_/ |_|\__|          \___/|_|\___/|_| |___/_|  \___/|_|  |___/
 *
 */ 
                                                                                    

#include "app_data.h"
#include "app_utils.h"
#include "app_gsdml.h"
#include "app_log.h"
#include "interface.h"
#include "sampleapp_common.h"
#include "osal.h"
#include "pnal.h"
#include <pnet_api.h>
#include "py_interface.h"

#include <assert.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_DATA_DEFAULT_OUTPUT_DATA CMD_CREATE(0, 0, 0)

/* Parameter data for digital submodules
 * The stored value is shared between all digital submodules in this example.
 *
 * Todo: Data is always in pnio data format. Add conversion to uint32_t.
 */
static uint32_t app_param_1 = 0; /* Network endianness */
static uint32_t app_param_2 = 0; /* Network endianness */

/* Parameter data for echo submodules
 * The stored value is shared between all echo submodules in this example.
 *
 * Todo: Data is always in pnio data format. Add conversion to uint32_t.
 */
static uint32_t app_param_echo_gain = 1; /* Network endianness */

/* Digital submodule process data
 * The stored value is shared between all digital submodules in this example. */

/**
 * status_reg_union_t is used to hold status register data. It allows easy
 * conversion between uint32_t, which is needed for bitwise operations, and a
 * uint8_t array, which is needed for transmission over the network.
 *
 * @note The length of 'array' in status_reg_union_t must be equal to the size
 * of 'u32' for proper conversion between array and uint32_t. The htonl API is
 * used on the 'u32' member to ensure correct endianness, which takes a
 * uint32_t parameter; 'u32' must be of type uint32_t.
 */
typedef union {
  uint32_t u32;
  uint8_t array[APP_GSDML_INPUT_DATA_DIGITAL_SIZE];
} status_reg_union_t;
_Static_assert(APP_GSDML_INPUT_DATA_DIGITAL_SIZE == sizeof(uint32_t), "size of 'u32' must match length of 'array'");

/**
 * command_reg_union_t is used to hold status register data. It allows easy
 * conversion between uint32_t, which is needed for bitwise operations, and a
 * uint8_t array, which is needed for transmission over the network.
 *
 * @note The length of 'array' in command_reg_union_t must be equal to the size
 * of 'u32' for proper conversion between array and uint32_t. The htonl API is
 * used on the 'u32' member to ensure correct endianness, which takes a
 * uint32_t parameter; 'u32' must be of type uint32_t.
 */
typedef union {
  uint32_t u32;
  uint8_t array[APP_GSDML_OUTPUT_DATA_DIGITAL_SIZE];
} command_reg_union_t;
_Static_assert(APP_GSDML_OUTPUT_DATA_DIGITAL_SIZE == sizeof(uint32_t), "size of 'u32' must match length of 'array'");;

static command_reg_union_t command_reg = {0};
static status_reg_union_t status_reg = {0};
static bool is_status_reg_set = false;
static bool exec_command = false;
static command_reg_cmd_t command = 0;
static command_reg_param_t parameter = 0;

/* Network endianness */
static uint8_t echo_inputdata[APP_GSDML_INPUT_DATA_ECHO_SIZE] = {0};
static uint8_t echo_outputdata[APP_GSDML_OUTPUT_DATA_ECHO_SIZE] = {0};

CC_PACKED_BEGIN
typedef struct CC_PACKED app_echo_data
{
   /* Network endianness.
      Used as a float, but we model it as a 4-byte integer to easily
      do endianness conversion */
   uint32_t echo_float_bytes;

   /* Network endianness */
   uint32_t echo_int;
} app_echo_data_t;
CC_PACKED_END
CC_STATIC_ASSERT (sizeof (app_echo_data_t) == APP_GSDML_INPUT_DATA_ECHO_SIZE);
CC_STATIC_ASSERT (sizeof (app_echo_data_t) == APP_GSDML_OUTPUT_DATA_ECHO_SIZE);

uint8_t * app_data_get_input_data (
   uint16_t slot_nbr,
   uint16_t subslot_nbr,
   uint32_t submodule_id,
   uint16_t * size,
   uint8_t * iops)
{
   float inputfloat;
   float outputfloat;
   uint32_t hostorder_inputfloat_bytes;
   uint32_t hostorder_outputfloat_bytes;
   app_echo_data_t * p_echo_inputdata = (app_echo_data_t *)&echo_inputdata;
   app_echo_data_t * p_echo_outputdata = (app_echo_data_t *)&echo_outputdata;

   if (size == NULL || iops == NULL)
   {
      return NULL;
   }

   if (submodule_id == APP_GSDML_SUBMOD_ID_STATUS)
   {
      *size = APP_GSDML_INPUT_DATA_DIGITAL_SIZE;
      *iops = PNET_IOXS_GOOD;

      // If app_data_process_output_data wasn't called to set status_reg,
      // ensure we have a valid value.
      if (!is_status_reg_set) {
        status_reg.u32 =
            STATUS_CREATE(STATUS_FLAG_OPERATIONAL, !STATUS_FLAG_BUSY,
                          ERROR_UNDEFINED, STATUS_READY);
      }

      // Ensure correct endianness (host to network). This is the only location
      // where other translation units can access the status register, so make
      // a single call to htonl here. htonl does not need to be (and should not
      // be) used when creating or modifying the status register elsewhere in
      // this translation unit.
      status_reg.u32 = htonl(status_reg.u32);
      return status_reg.array;
   }

   if (submodule_id == APP_GSDML_SUBMOD_ID_ECHO)
   {
      /* Calculate echodata input (to the PLC)
       * by multiplying the output (from the PLC) with a gain factor
       */

      /* Integer */
      p_echo_inputdata->echo_int = CC_TO_BE32 (
         CC_FROM_BE32 (p_echo_outputdata->echo_int) *
         CC_FROM_BE32 (app_param_echo_gain));

      /* Float */
      /* Use memcopy to avoid strict-aliasing rule warnings */
      hostorder_outputfloat_bytes =
         CC_FROM_BE32 (p_echo_outputdata->echo_float_bytes);
      memcpy (&outputfloat, &hostorder_outputfloat_bytes, sizeof (outputfloat));
      inputfloat = outputfloat * CC_FROM_BE32 (app_param_echo_gain);
      memcpy (&hostorder_inputfloat_bytes, &inputfloat, sizeof (outputfloat));
      p_echo_inputdata->echo_float_bytes =
         CC_TO_BE32 (hostorder_inputfloat_bytes);

      *size = APP_GSDML_INPUT_DATA_ECHO_SIZE;
      *iops = PNET_IOXS_GOOD;
      return echo_inputdata;
   }

   /* Automated RT Tester scenario 2 - unsupported (sub)module */
   return NULL;
}

/**
 * Execute commands received from the PLC.
 *
 * Any commands that cannot be processed here are forwarded to the Python image
 * processing component.
 *
 * @param cmd Command received from the PLC
 * @param param Parameter received from the PLC
 * @return Status register
 */
static status_reg_t execute_command(command_reg_cmd_t cmd,
                                    command_reg_param_t param) {
  status_reg_t status = STATUS_CREATE(1, 0, ERROR_INTERNAL, STATUS_ERROR);
  switch (cmd) {
    case CMD_NOP:
      status = STATUS_CREATE(1, 0, ERROR_UNDEFINED, STATUS_UNDEFINED);
      break;

    case CMD_REBOOT:
      py_deinit();
      if (py_init()) {
        status = STATUS_CREATE(1, 0, ERROR_UNDEFINED, STATUS_BOOTING);
      } else {
        APP_LOG_FATAL("py_init failed during reboot!\n");
      }
      break;

    case CMD_PING:
      status = STATUS_CREATE(1, 0, ERROR_UNDEFINED, STATUS_PING_REPLY);
      break;

    default:
      status = py_execute_command(cmd, param);
      break;
  }
  return status;
}

/**
 * Process the command register and set the status register
 *
 * The command is executed when the execute bit flips from true to false.
 */
static void process_command_reg(void) {
  // TODO-bwahl - need a way to determine and report OPERATIONAL bit
  if (CMD_IS_EXECUTE_BIT_SET(command_reg.u32)) {
    exec_command = true;
    command = CMD_EXTRACT_COMMAND(command_reg.u32);
    parameter = CMD_EXTRACT_PARAMETER(command_reg.u32);
    status_reg.u32 = STATUS_CREATE(STATUS_FLAG_OPERATIONAL, STATUS_FLAG_BUSY,
                                   ERROR_UNDEFINED, STATUS_BUSY);
  } else if (!CMD_IS_EXECUTE_BIT_SET(command_reg.u32) && exec_command) {
    exec_command = false;
    status_reg.u32 = execute_command(command, parameter);
  } else {
    status_reg.u32 = STATUS_CREATE(STATUS_FLAG_OPERATIONAL, !STATUS_FLAG_BUSY,
                                   ERROR_UNDEFINED, STATUS_READY);
  }
  is_status_reg_set = true;
}

int app_data_process_output_data(
   uint16_t slot_nbr,
   uint16_t subslot_nbr,
   uint32_t submodule_id,
   uint8_t * data,
   uint16_t size)
{
   if (data == NULL)
   {
      return -1;
   }

   if (submodule_id == APP_GSDML_SUBMOD_ID_COMMAND)
   {
      if (size == APP_GSDML_OUTPUT_DATA_DIGITAL_SIZE)
      {
        // Get data from the PLC and process commands. According to the P-Net
        // docs, "The Profinet payload is sent as big endian (network endian)
        // on the wire." Convert from buffer to uint32_t and ensure correct
        // endianness (network to host). ntohl calls should not be called
        // elsewhere in this translation unit when creating or modifying the
        // command register.
        memcpy(command_reg.array, data, size);
        command_reg.u32 = ntohl(command_reg.u32);
        process_command_reg();
        return 0;
      }
   }
   else if (submodule_id == APP_GSDML_SUBMOD_ID_ECHO)
   {
      if (size == APP_GSDML_OUTPUT_DATA_ECHO_SIZE)
      {
         memcpy (echo_outputdata, data, size);

         return 0;
      }
   }

   return -1;
}

int app_data_set_default_outputs (void)
{
   command_reg.u32 = APP_DATA_DEFAULT_OUTPUT_DATA;
   return 0;
}

int app_data_write_parameter (
   uint16_t slot_nbr,
   uint16_t subslot_nbr,
   uint32_t submodule_id,
   uint32_t index,
   const uint8_t * data,
   uint16_t length)
{
   const app_gsdml_param_t * par_cfg;

   par_cfg = app_gsdml_get_parameter_cfg (submodule_id, index);
   if (par_cfg == NULL)
   {
      APP_LOG_WARNING (
         "PLC write request unsupported submodule/parameter. "
         "Submodule id: %u Index: %u\n",
         (unsigned)submodule_id,
         (unsigned)index);
      return -1;
   }

   if (length != par_cfg->length)
   {
      APP_LOG_WARNING (
         "PLC write request unsupported length. "
         "Index: %u Length: %u Expected length: %u\n",
         (unsigned)index,
         (unsigned)length,
         par_cfg->length);
      return -1;
   }

   if (index == APP_GSDML_PARAMETER_1_IDX)
   {
      memcpy (&app_param_1, data, length);
   }
   else if (index == APP_GSDML_PARAMETER_2_IDX)
   {
      memcpy (&app_param_2, data, length);
   }
   else if (index == APP_GSDML_PARAMETER_ECHO_IDX)
   {
      memcpy (&app_param_echo_gain, data, length);
   }

   APP_LOG_DEBUG ("  Writing parameter \"%s\"\n", par_cfg->name);
   app_log_print_bytes (APP_LOG_LEVEL_DEBUG, data, length);

   return 0;
}

int app_data_read_parameter (
   uint16_t slot_nbr,
   uint16_t subslot_nbr,
   uint32_t submodule_id,
   uint32_t index,
   uint8_t ** data,
   uint16_t * length)
{
   const app_gsdml_param_t * par_cfg;

   par_cfg = app_gsdml_get_parameter_cfg (submodule_id, index);
   if (par_cfg == NULL)
   {
      APP_LOG_WARNING (
         "PLC read request unsupported submodule/parameter. "
         "Submodule id: %u Index: %u\n",
         (unsigned)submodule_id,
         (unsigned)index);
      return -1;
   }

   if (*length < par_cfg->length)
   {
      APP_LOG_WARNING (
         "PLC read request unsupported length. "
         "Index: %u Max length: %u Data length for our parameter: %u\n",
         (unsigned)index,
         (unsigned)*length,
         par_cfg->length);
      return -1;
   }

   APP_LOG_DEBUG ("  Reading \"%s\"\n", par_cfg->name);
   if (index == APP_GSDML_PARAMETER_1_IDX)
   {
      *data = (uint8_t *)&app_param_1;
      *length = sizeof (app_param_1);
   }
   else if (index == APP_GSDML_PARAMETER_2_IDX)
   {
      *data = (uint8_t *)&app_param_2;
      *length = sizeof (app_param_2);
   }
   else if (index == APP_GSDML_PARAMETER_ECHO_IDX)
   {
      *data = (uint8_t *)&app_param_echo_gain;
      *length = sizeof (app_param_echo_gain);
   }

   app_log_print_bytes (APP_LOG_LEVEL_DEBUG, *data, *length);

   return 0;
}
