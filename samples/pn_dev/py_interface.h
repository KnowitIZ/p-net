#ifndef PY_INTERFACE_H_
#define PY_INTERFACE_H_

/*********************************************************************
 *                             Includes
 *********************************************************************/
#include <stdbool.h>

#include "interface.h"

/*********************************************************************
 *                       Function Prototypes
 *********************************************************************/

bool py_init(void);
void py_deinit(void);
status_reg_t py_process_command(command_reg_cmd_t cmd,
                                 command_reg_param_t param);

#endif  // PY_INTERFACE_H_
