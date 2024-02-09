/*
 * For demonstration purposes.
 *
 * To build:
 * 1. Ensure cwd is p-net/samples/pn_dev/
 * 2. gcc -I /usr/include/python3.11 -c py_test.c -o py_test.o
 * 3. gcc -I /usr/include/python3.11 -c py_interface.c -o py_interface.o
 * 4. gcc py_test.o py_interface.o -lpython3.11 -o program
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "py_interface.h"

int main (int argc, char * argv[])
{
   if (!py_init())
   {
      printf ("py_init fail!");
      py_deinit();
      return 0;
   }

   status_reg_t status_reg;
   status_reg_oper_t operational;
   status_reg_busy_t busy;
   status_reg_error_t error;
   status_reg_status_t status;

   uint32_t i = 1;
   while (true)
   {
      // simulates receiving commands from the PLC
      command_reg_cmd_t cmd = 0;
      if (i % 7 == 0)
      {
         cmd = CMD_SET_WORKPIECE_TYPE_122;
      }
      else if (i % 10 == 0)
      {
         cmd = CMD_REBOOT;
         py_deinit();
         if (py_init())
         {
            error = ERROR_UNDEFINED;
            status = STATUS_BOOTING;
         }
         else
         {
            error = ERROR_INTERNAL;
            status = STATUS_ERROR;
         }
      }
      else
      {
         cmd = CMD_TAKE_PICTURE;
      }

      if (cmd != CMD_REBOOT)
      {
         printf ("\nC call py_execute_command cmd=%u\n", cmd);
         status_reg = py_execute_command (cmd, 0);
         operational = STATUS_EXTRACT_OPERATIONAL (status_reg);
         busy = STATUS_EXTRACT_BUSY (status_reg);
         error = STATUS_EXTRACT_ERROR (status_reg);
         status = STATUS_EXTRACT_ADDITIONAL_STATUS (status_reg);

         printf ("C error = %u\n", error);
         printf ("C status = %u\n", status);
         printf ("\n");
      }

      sleep (1);
      ++i;
   }

   py_deinit();

   return 0;
}