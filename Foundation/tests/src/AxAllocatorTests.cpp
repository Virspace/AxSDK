#include "gtest/gtest.h"
#include "Foundation/Types.h"

#include "Foundation/AxAllocatorInfo.h"
#include "Foundation/AxLinearAllocator.h"

class LinearAllocatorTest : public testing::Test
{
    protected:
        AxLinearAllocator *LinearAllocator;

        void SetUp()
        {
            LinearAllocator = LinearAllocatorAPI->Create("LinearTest", 64, (void *)0x10000);
        }

        void TearDown()
        {
            LinearAllocatorAPI->Destroy(LinearAllocator);
        }
};

TEST_F(LinearAllocatorTest, Name)
{
    AxAllocatorInfo Info = AllocatorInfoRegistryAPI->GetInfoByIndex(1);
    EXPECT_STREQ(Info.Name, "LinearTest");
}

TEST_F(LinearAllocatorTest, Size)
{
    size_t Size = AllocatorInfoRegistryAPI->Size();
    EXPECT_EQ(Size, 1);
}

TEST_F(LinearAllocatorTest, Stats)
{
    void *Mem1 = LinearAllocatorAPI->Alloc(LinearAllocator, 64, __FILE__, __LINE__);
    AxAllocatorInfo Info = AllocatorInfoRegistryAPI->GetInfoByIndex(1);
    EXPECT_EQ(Info.BytesReserved, 65536);
    EXPECT_EQ(Info.BytesCommitted, 64);
    EXPECT_EQ(Info.PageSize, 4096);
    EXPECT_EQ(Info.AllocationGranularity, 65536);
    EXPECT_EQ(Info.PagesReserved, 16);
    EXPECT_EQ(Info.PagesCommitted, 1);
}