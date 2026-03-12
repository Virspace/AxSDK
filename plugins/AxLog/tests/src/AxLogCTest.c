/* AxLogCTest.c -- verifies AX_LOG macros compile and work from a C translation unit */
#include "AxLog/AxLog.h"

void AxLogCTestFunction(void)
{
    AX_LOG(INFO, "message from C translation unit");
    AX_LOG(ERROR, "error from C: code %d", 42);
    AX_LOG_IF(WARNING, 1, "conditional from C: %s", "active");
    AX_LOG_IF(DEBUG, 0, "this should not appear from C");
}
