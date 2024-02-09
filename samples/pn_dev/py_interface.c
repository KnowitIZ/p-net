/*********************************************************************
 *                             Includes
 *********************************************************************/
#include <Python.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#include "app_log.h"
#include "interface.h"
#include "py_interface.h"

/*********************************************************************
 *                        Literal Constants
 *********************************************************************/

// Module name of the Python file (excludes .py). The Python module must be in
// the current working directory.
#define MODULE_NAME ("image_processing")

// constants for the execute_command() Python function
#define NUM_ARGS (2)
#define ARGS_CMD_INDEX (0)
#define ARGS_PARAM_INDEX (1)
#define NUM_RETURN_VALUES (2)
#define RETURN_ERROR_INDEX (0)
#define RETURN_STATUS_INDEX (1)

/*********************************************************************
 *                              Types
 *********************************************************************/

typedef unsigned long py_return_t;

/*********************************************************************
 *                              Macros
 *********************************************************************/

/*********************************************************************
 *                            Variables
 *********************************************************************/

static PyObject* py_module_init;    // init() function in Python module
static PyObject* py_module_deinit;  // deinit() function in Python module
static PyObject* py_exec_cmd;       // execute_command() function in Python module
static PyObject* py_exec_cmd_args;  // arg tuple for the Python execute_command() function

static bool is_init = false;

/*********************************************************************
 *                       Function Prototypes
 *********************************************************************/

/*********************************************************************
 *                      Function Definitions
 *********************************************************************/

/**
 * Initialize the Python module.
 *
 * @return true if success, else false.
 */
bool py_init(void) {
  bool success = true;

  {
    // Set PYTHONPATH env variable to the current working directory. This is
    // where Python will search for the image_processing.py module. Searching
    // in cwd allows the real image_processing.py located in the project root
    // to be imported and used when building (using build.sh) and running the
    // project, and also allows the sample image_processing.py module in this
    // directory to be imported and used when building and running py_sample.c.
    char cwd[PATH_MAX] = {'\0'};
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      APP_LOG_FATAL("py_init: getcwd() failed");
      return false;
    }
    setenv("PYTHONPATH", cwd, 1);
  }

  Py_Initialize();

  // import the Python module
  PyObject* py_name = PyUnicode_FromString(MODULE_NAME);
  PyObject* py_module = PyImport_Import(py_name);
  Py_DECREF(py_name);
  success = py_module && PyModule_Check(py_module);
  if (!success) APP_LOG_FATAL("py_init: module import failed\n");

  if (success) {
    // get a reference to the Python execute_command() function
    py_exec_cmd = PyObject_GetAttrString(py_module, "execute_command");
    success = py_exec_cmd && PyCallable_Check(py_exec_cmd);
    if (!success) APP_LOG_FATAL("py_init: get ref to execute_command fail\n");
  }

  if (success) {
    // create the execute_command() argument tuple
    py_exec_cmd_args = PyTuple_New(NUM_ARGS);
    success = py_exec_cmd_args && PyTuple_Check(py_exec_cmd_args) &&
              (PyTuple_Size(py_exec_cmd_args) == NUM_ARGS);
    if (!success) APP_LOG_FATAL("py_init: creating arg tuple failed\n");
  }

  if (success) {
    // get a reference to the python module deinit function
    py_module_deinit = PyObject_GetAttrString(py_module, "deinit");
    success = success && py_module_deinit && PyCallable_Check(py_module_deinit);
    if (!success) APP_LOG_FATAL("py_init: get ref to deinit fail\n");
  }

  if (success) {
    // call the Python py_module_init function
    py_module_init = PyObject_GetAttrString(py_module, "init");
    success = py_module_init && PyCallable_Check(py_module_init);
    if (!success) APP_LOG_FATAL("py_init: get ref to py_module_init fail\n");
    success = success && (PyObject_CallObject(py_module_init, NULL) != NULL);
    if (!success) APP_LOG_FATAL("py_init: py_module_init call fail\n");
  }

  // we don't need the reference to the module anymore
  Py_XDECREF(py_module);

  is_init = success;
  if (!success) {
    py_deinit();
  }

  return success;
}

/**
 * Release references to Python objects and finalize the interpreter.
 */
void py_deinit(void) {
  is_init = false;
  PyObject_CallObject(py_module_deinit, NULL);
  Py_XDECREF(py_module_init);
  Py_XDECREF(py_exec_cmd);
  Py_XDECREF(py_exec_cmd_args);
  Py_Finalize();
}

/**
 * Wrapper for the Python execute_command() fuction. Calls the function and
 * gets the return values.
 *
 * @param cmd Command received from the PLC
 * @param param Parameter received from the PLC
 * @return Status register
 */
status_reg_t py_execute_command(command_reg_cmd_t cmd,
                                command_reg_param_t param) {
  if (!is_init) {
    APP_LOG_ERROR("py_execute_command: is_init is false\n");
    return STATUS_CREATE(1, 0, ERROR_INTERNAL, STATUS_ERROR);
  }

  // build the arg tuple
  PyObject* py_cmd = PyLong_FromUnsignedLong(cmd);
  PyTuple_SetItem(py_exec_cmd_args, ARGS_CMD_INDEX, py_cmd);
  py_cmd = PyLong_FromUnsignedLong(param);
  PyTuple_SetItem(py_exec_cmd_args, ARGS_PARAM_INDEX, py_cmd);
  Py_DECREF(py_cmd);

  // call the function
  PyObject* py_result = PyObject_CallObject(py_exec_cmd, py_exec_cmd_args);

  // extract the return values
  py_return_t ret_vals[NUM_RETURN_VALUES];
  bool success = py_result && PyTuple_Check(py_result) &&
                 (PyTuple_Size(py_result) == NUM_RETURN_VALUES);
  if (success) {
    for (int i = 0; i < NUM_RETURN_VALUES; i++) {
      PyObject* py_item = PyTuple_GetItem(py_result, i);
      success = py_item && PyLong_Check(py_item);
      if (success) {
        ret_vals[i] = PyLong_AsUnsignedLong(py_item);
      }
      Py_XDECREF(py_item);
      if (!success) {
        APP_LOG_ERROR("py_execute_command: tuple element %u is not valid\n", i);
        break;
      }
    }
  }

  Py_XDECREF(py_result);

  if (success) {
    return STATUS_CREATE(1, 0, ret_vals[RETURN_ERROR_INDEX],
                         ret_vals[RETURN_STATUS_INDEX]);
  }
  return STATUS_CREATE(1, 0, ERROR_INTERNAL, STATUS_ERROR);
}
