#ifndef INTERFACE_H
#define INTERFACE_H
/* 
 *  _  __                    _ _              ___  _        __      __                
 * | |/ /_ __   _____      _(_) |_           / _ \| | ___  / _|___ / _| ___  _ __ ___ 
 * | ' /| '_ \ / _ \ \ /\ / / | __|  _____  | | | | |/ _ \| |_/ __| |_ / _ \| '__/ __|
 * | . \| | | | (_) \ V  V /| | |_  |_____| | |_| | | (_) |  _\__ \  _| (_) | |  \__ \
 * |_|\_\_| |_|\___/ \_/\_/ |_|\__|          \___/|_|\___/|_| |___/_|  \___/|_|  |___/
 *
 */ 

#include <stdint.h>

// Command register typedefs
typedef uint32_t command_reg_t;
typedef uint8_t command_reg_cmd_t;
typedef uint32_t command_reg_param_t;

// Collection of typical commands for the interface. Add as needed.
#define CMD_NOP                         0x00
#define CMD_REBOOT                      0x01
#define CMD_PING                        0x02
#define CMD_SET_WORKPIECE_TYPE_NONE     0x03
#define CMD_SET_WORKPIECE_TYPE_122      0x04 // Prefix for plowsteel article numbers

#define CMD_TAKE_PICTURE                0x10
#define CMD_SET_WORKPIECE_ORIENTATION   0x11
#define CMD_SET_WORKPIECE_SERIAL_NUMBER 0x12

// Command register is 32 bits. 1 bit for command excution, 8 bits for command, 23 bits for parameter(s).
// -----------------------------------------------------------------------------------------------------
// |  1 bit  |  8 bits  |  23 bits  |
// -----------------------------------------------------------------------------------------------------
// | Execute | Command  | Parameter |
// -----------------------------------------------------------------------------------------------------
// Execute: 1 = Execute command, 0 = Do not execute command.
// Command: Command to execute.

// Macros to create command register value and extract command from command register value.
#define CMD_CREATE(execute, command, parameter) (((execute) << 31) | ((command) << 23) | (parameter))
#define CMD_IS_EXECUTE_BIT_SET(command_register) (((command_register) >> 31) & 0x01)
#define CMD_EXTRACT_COMMAND(command_register) (((command_register) >> 23) & 0xFF)
#define CMD_EXTRACT_PARAMETER(command_register) ((command_register) & 0x7FFFFF)

// status register type defs
typedef uint32_t status_reg_t;
typedef uint8_t status_reg_oper_t;
typedef uint8_t status_reg_busy_t;
typedef uint8_t status_reg_error_t;
typedef uint32_t status_reg_status_t;

// Status register is 32 bits. 1 bit indicates operational device,
// 1 bit for busy indication, 8 bits for status, 22 bits for errors/additional status.
// -----------------------------------------------------------------------------------------------------
// |  1 bit         |  1 bit  |  8 bits  |  22 bits          |
// -----------------------------------------------------------------------------------------------------
// |  Operational   |  Busy   |  Errors  | Additional status |
// -----------------------------------------------------------------------------------------------------
// Operational: 1 = Operational, 0 = Not operational.
// Busy: 1 = Busy, 0 = Not busy.
// Errors: 8 bits for error code
// Additional status: 22 bits for additional status.

// Macros to set busy, operational, error and additional status.
#define STATUS_CREATE(operational, busy, error, additional_status) (((operational) << 31) | ((busy) << 30) | ((error) << 22) | (additional_status))
#define STATUS_EXTRACT_OPERATIONAL(status_register) (((status_register) >> 31) & 0x01)
#define STATUS_EXTRACT_BUSY(status_register) (((status_register) >> 30) & 0x01)
#define STATUS_EXTRACT_ERROR(status_register) (((status_register) >> 22) & 0xFF)
#define STATUS_EXTRACT_ADDITIONAL_STATUS(status_register) ((status_register) & 0x3FFFFF)

#define STATUS_FLAG_OPERATIONAL 1
#define STATUS_FLAG_BUSY 1

// Collection of typical statusess for the interface. Add as needed.
#define STATUS_UNDEFINED                0x00
#define STATUS_BOOTING                  0x01
#define STATUS_PING_REPLY               0x02
#define STATUS_READY                    0x03
#define STATUS_BUSY                     0x04
#define STATUS_ERROR                    0x05
#define STATUS_WORKPIECE_OK             0x06
#define STATUS_WORKPIECE_NOK            0x07
#define STATUS_WORKPIECE_NONE           0x08

// Collection of typical error codes for the interface. Add as needed.
#define ERROR_UNDEFINED                 0x00
#define ERROR_INVALID_COMMAND           0x03
#define ERROR_INVALID_PARAMETER         0x04
#define ERROR_NO_CAMERA                 0x05
#define ERROR_INTERNAL                  0x06
#define ERROR_INVALID_WORKPIECE_SIZE    0x07
#define ERROR_INVALID_WORKPIECE_COLOR   0x08
#define ERROR_INVALID_WORKPIECE_SHAPE   0x09
#define ERROR_INVALID_WORKPIECE_WEIGHT  0x0A
#define ERROR_INVALID_WORKPIECE_TEXTURE 0x0B


#endif // INTERFACE_H
