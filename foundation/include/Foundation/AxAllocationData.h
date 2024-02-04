
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct AxAllocationData
{
    // TODO(mdeforge): We don't name allocations, but maybe we should. Sounds useful in debugging.
    void *Address;
    size_t Size;
};

#ifdef __cplusplus
}
#endif