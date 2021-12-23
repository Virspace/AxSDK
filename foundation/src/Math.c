#include "Math.h"
#include <stdlib.h>

// TODO(mdeforge): Use better rand function

void SeedRandom(uint32_t Seed)
{
    srand(Seed);
}

float RandomFloat(const float Min, const float Max)
{
    return ((float)rand() / (RAND_MAX) * (Max - Min) + Min);
}

AxMat4x4 LookAt(AxVec3 Eye, AxVec3 Center, AxVec3 Up)
{
    const AxVec3 F = Normalize(Vec3Sub(Center, Eye));
    const AxVec3 S = Normalize(CrossProduct(F, Up));
    const AxVec3 U = CrossProduct(S, F);

    AxMat4x4 Result = {0};
    Result.E[0][0] = S.X;
    Result.E[1][0] = S.Y;
    Result.E[2][0] = S.Z;
    Result.E[3][0] = 1;

    Result.E[0][1] = U.X;
    Result.E[1][1] = U.Y;
    Result.E[2][1] = U.Z;
    Result.E[3][1] = 1;

    Result.E[0][2] = -F.X;
    Result.E[1][2] = -F.Y;
    Result.E[2][2] = -F.Z;
    Result.E[3][2] = 1;

    Result.E[3][0] = -DotProduct(S, Eye);
    Result.E[3][1] = -DotProduct(U, Eye);
    Result.E[3][2] = DotProduct(F, Eye);
    Result.E[3][3] = 1;

    return (Result);
}