#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "net_util.h"

void
net_debug(const char* format, ...)
{
        va_list valist;
        /* Initialize valist for num number of arguments. */
        va_start(valist, format);
        vfprintf(stdout, format, valist);
        fflush(stdout);
        /* Clean memory reserved for valist. */
        va_end(valist);
        return;
}

/**
 * Call this function when an NON-FATAL state occurs during networking that
 * the user should know about.
 *
 * Prints a message and returns back to the calling function.
 * TODO: In the future furture logging of events can be placed here.
 */
void
net_print(const char* format, ...)
{
        va_list valist;
        /* Initialize valist for num number of arguments. */
        va_start(valist, format);
        vfprintf(stdout, format, valist);
        fflush(stdout);
        /* Clean memory reserved for valist. */
        va_end(valist);
        return;
}

/**
 * Call this function when an NON-FATAL error occurs. One still has to exit
 * the task at hand.
 *
 * Currently prints a message and returns back to the calling function.
 * TODO: In the future furture logging of warnings can be placed here.
 */
void
net_warn(const char* format, ...)
{
        va_list valist;
        /* Initialize valist for num number of arguments. */
        va_start(valist, format);
        vfprintf(stderr, format, valist);
        /* Clean memory reserved for valist. */
        va_end(valist);
        return;
}

/**
 * Call this function when an error occurs.  One still has to exit the
 * task at hand.
 *
 * Currently prints a message and returns back to the calling function.
 * TODO: In the future furture logging of errors can be placed here. */
void
net_error(const char* format, ...)
{
        va_list valist;
        /* Initialize valist for num number of arguments. */
        va_start(valist, format);
        vfprintf(stderr, format, valist);
        /* Clean memory reserved for valist. */
        va_end(valist);
        return;
}
