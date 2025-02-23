#include "AxMath.h"
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
    AxVec3 F = Normalize(Vec3Sub(Center, Eye));
    AxVec3 S = Normalize(CrossProduct(F, Up));
    AxVec3 U = CrossProduct(S, F);

    AxMat4x4 Result = {0};

    // First Column
    Result.E[0][0] = S.X;
    Result.E[0][1] = U.X;
    Result.E[0][2] = -F.X;
    Result.E[0][3] = 0.0f;

    // Second Column
    Result.E[1][0] = S.Y;
    Result.E[1][1] = U.Y;
    Result.E[1][2] = -F.Y;
    Result.E[1][3] = 0.0f;

    // Third Column
    Result.E[2][0] = S.Z;
    Result.E[2][1] = U.Z;
    Result.E[2][2] = -F.Z;
    Result.E[2][3] = 0.0f;

    // Fourth Column
    Result.E[3][0] = -DotProduct(S, Eye);
    Result.E[3][1] = -DotProduct(U, Eye);
    Result.E[3][2] = DotProduct(F, Eye);
    Result.E[3][3] = 1.0f;

    return (Result);
}