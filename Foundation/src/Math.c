#include "Math.h"
#include <stdlib.h>

// TODO(mdeforge): Use better rand function

float RandomFloat(const float Min, const float Max)
{
    srand(time(NULL));
    return (((float)rand() / (RAND_MAX)) + 1);
}