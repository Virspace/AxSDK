#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxAllocUtils.h"
#include "Foundation/AxAllocatorRegistry.h"
#include "Foundation/AxLinearAllocator.h"

TEST(AlocUtils, IsPowerOfTwo)
{
    EXPECT_EQ(IsPowerOfTwo(3), false);
    EXPECT_EQ(IsPowerOfTwo(4), true);
}

TEST(AlocUtils, RoundUpToPowerOfTwo)
{
    // Value is zero case
    EXPECT_EQ(RoundUpToPowerOfTwo(0, 16), 0);

    // Multiple is zero case
    EXPECT_EQ(RoundUpToPowerOfTwo(7, 0), 0);

    // Value is less than multiple case
    EXPECT_EQ(RoundUpToPowerOfTwo(7, 16), 16);

    // Value is more than twice as small as multiple case
    EXPECT_EQ(RoundUpToPowerOfTwo(7, 32), 32);

    // Value is greater than the multiple case
    EXPECT_EQ(RoundUpToPowerOfTwo(17, 8), 24);

    // Multiple is less than not a power of two case
    EXPECT_EQ(RoundUpToPowerOfTwo(7, 9), 0);

    // Multiple is greater than not a power of two case
    EXPECT_EQ(RoundUpToPowerOfTwo(13, 9), 0);
}

TEST(AlocUtils, RoundDownToPowerOfTwo)
{
    // Value is zero case
    EXPECT_EQ(RoundDownToPowerOfTwo(0, 16), 0);

    // Multiple is zero case
    EXPECT_EQ(RoundDownToPowerOfTwo(18, 0), 0);

    // Value is greater than multiple case
    EXPECT_EQ(RoundDownToPowerOfTwo(18, 16), 16);

    // Value is more than twice as small as multiple case
    EXPECT_EQ(RoundDownToPowerOfTwo(31, 16), 16);

    // Value is smaller than the multiple case
    EXPECT_EQ(RoundDownToPowerOfTwo(3, 16), 0);

    // Multiple is less than not a power of two case
    EXPECT_EQ(RoundDownToPowerOfTwo(5, 13), 0);

    // Multiple is greater than not a power of two case
    EXPECT_EQ(RoundDownToPowerOfTwo(18, 13), 0);
}

TEST(AllocUtils, Alignment)
{
    uintptr_t Start = 0xFF1040; // true
    uintptr_t End = 0xFF1048; // true

    EXPECT_EQ(IsAligned((void *)Start), true);

    for (uintptr_t Current = Start + 1; Current < End; Current++) {
        EXPECT_EQ(IsAligned((void *)Current), false);
    }

    EXPECT_EQ(IsAligned((void *)End), true);
}

class LinearAllocatorTest : public testing::Test
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

TEST_F(LinearAllocatorTest, Length)
{
    size_t Length = AllocatorRegistryAPI->Length();
    EXPECT_EQ(Length, 1);
}

// TEST_F(LinearAllocatorTest, Info)
// {
//     void *Mem1 = LinearAllocatorAPI->Alloc(LinearAllocator, 64, __FILE__, __LINE__);
//     AxAllocatorInfo *Info = AllocatorRegistryAPI->GetAllocatorInfoByIndex(1);
//     EXPECT_STREQ(AllocatorRegistryAPI->Name(Info), "LinearTest");
//     EXPECT_EQ(AllocatorRegistryAPI->BytesReserved(Info), 65536);
//     EXPECT_EQ(AllocatorRegistryAPI->BytesCommitted(Info), 64);
//     EXPECT_EQ(AllocatorRegistryAPI->PageSize(Info), 4096);
//     EXPECT_EQ(AllocatorRegistryAPI->AllocationGranularity(Info), 65536);
//     EXPECT_EQ(AllocatorRegistryAPI->PagesReserved(Info), 16);
//     EXPECT_EQ(AllocatorRegistryAPI->PagesCommitted(Info), 1);
// }