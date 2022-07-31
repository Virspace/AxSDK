#include "gtest/gtest.h"
#include "Foundation/Types.h"
#include "Foundation/AxAllocatorRegistry.h"
#include "Foundation/AxLinearAllocator.h"

class LinearAllocatorTest : public testing::Test
{
    protected:
        AxLinearAllocator *LinearAllocator;

        void SetUp()
        {
            LinearAllocator = LinearAllocatorAPI->Create("LinearTest", 64, 0);
        }

        void TearDown()
        {
            LinearAllocatorAPI->Destroy(LinearAllocator);
        }
};

TEST_F(LinearAllocatorTest, Length)
{
    size_t Length = AllocatorRegistryAPI->Length();
    EXPECT_EQ(Length, 1);
}

TEST_F(LinearAllocatorTest, Info)
{
    void *Mem1 = LinearAllocatorAPI->Alloc(LinearAllocator, 64, __FILE__, __LINE__);
    AxAllocatorInfo *Info = AllocatorRegistryAPI->GetAllocatorInfoByIndex(1);
    EXPECT_STREQ(AllocatorRegistryAPI->Name(Info), "LinearTest");
    EXPECT_EQ(AllocatorRegistryAPI->BytesReserved(Info), 65536);
    EXPECT_EQ(AllocatorRegistryAPI->BytesCommitted(Info), 64);
    EXPECT_EQ(AllocatorRegistryAPI->PageSize(Info), 4096);
    EXPECT_EQ(AllocatorRegistryAPI->AllocationGranularity(Info), 65536);
    EXPECT_EQ(AllocatorRegistryAPI->PagesReserved(Info), 16);
    EXPECT_EQ(AllocatorRegistryAPI->PagesCommitted(Info), 1);
}