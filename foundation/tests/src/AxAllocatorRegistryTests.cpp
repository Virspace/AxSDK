#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxAllocatorInfo.h"
#include "Foundation/AxAllocatorRegistry.h"
#include "Foundation/AxLinearAllocator.h"

class AxAllocatorRegistryTest : public testing::Test
{
protected:
    AxLinearAllocator *LinearAllocator;

    void SetUp()
    {
        LinearAllocator = LinearAllocatorAPI->Create("LinearTest", 64);
    }

    void TearDown()
    {
        LinearAllocatorAPI->Destroy(LinearAllocator);
    }
};

TEST_F(AxAllocatorRegistry, Registration)
{
    AxAllocatorData *Data = AllocatorRegistryAPI->
}