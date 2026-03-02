/**
 * @file error.h
 * @brief Custom error handling functions
 */

#include <stdlib.h>

#ifndef ERROR_H
#define ERROR_H

// forward declaration
typedef struct circuit_ir circuit_ir_t;

typedef struct medusa_err_info {
    char msg[256]; ///< error message describing the assertion failure
    int line;      ///< optional line number in the input file where the failure occurred
} medusa_err_info_t;

/**
 * Function that handles program exiting in case of error.
 *
 * @param error Error message format.
*/
void error_exit(const char *error, ...);

/**
 * @brief Sets the last error message and line number in the circuit IR.
 *
 * @param[in] ir The circuit IR to set the error message for.
 * @param[in] line The line number in the input file where the error occurred (or -1 if not applicable).
 * @param[in] error The error message format string (printf-style).
 */
void error_set(circuit_ir_t *ir, int line, const char *error, ...);

/**
 * @brief Retrieves the last error message and line number from the circuit IR.
 *
 * @param[in] ir The circuit IR to retrieve the error message from.
 * @param[out] msg Retrieved error message string.
 * @param[out] line Retrieved line number where the error occurred (or -1 if not applicable).
 */
void error_get_last(circuit_ir_t *ir, const char **msg, int *line);

/**
 * @brief Clears all recorded errors from the circuit IR.
 *
 * @param[in] ir The circuit IR to clear errors from.
 */
void error_clear(circuit_ir_t *ir);

/**
 * Custom malloc function including error handling.
 */
void* my_malloc(size_t size);

/**
 * Custom realloc function including error handling.
 */
void* my_realloc(void *p, size_t size);

#endif
/* end of "error.h" */