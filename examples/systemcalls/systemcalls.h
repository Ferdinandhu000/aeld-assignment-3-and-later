#ifndef SYSTEMCALLS_H
#define SYSTEMCALLS_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() library function, false if an error occurred.
 */
bool do_system(const char *cmd);

/**
 * @param count -The numbers of variables passed to the function. The number of arguments
 *   to follow should be matching.
 * @param ... - A list of 1 or more arguments followed by the command to execute.
 *   The first argument is the name of the command to execute.
 *   The remainder of the arguments are the arguments to that command.
 * @return true if the command @param ... with arguments @param arguments were executed successfully
 *   using the execv() library function, false if an error occurred.
 */
bool do_exec(int count, ...);

/**
 * @param outputfile - The full path to the file to write with output of the command
 * @param count -The numbers of variables passed to the function. The number of arguments
 *   to follow should be matching.
 * @param ... - A list of 1 or more arguments followed by the command to execute.
 *   The first argument is the name of the command to execute.
 *   The remainder of the arguments are the arguments to that command.
 * @return true if the command @param ... with arguments @param arguments were executed successfully
 *   using the execv() library function, false if an error occurred.
 */
bool do_exec_redirect(const char *outputfile, int count, ...);

#endif /* SYSTEMCALLS_H */