#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxAllocUtils.h"
#include "Foundation/AxAllocatorRegistry.h"
#include "Foundation/AxAllocatorInfo.h"
#include "Foundation/AxLinearAllocator.h"

static uintptr_t AlignedAddress1 = 0x000001bb20350000;
static uintptr_t AlignedAddress2 = 0x000001bb20350008;
static uintptr_t UnalignedAddress = 0x000001bb20350001;
static uintptr_t NextPage = 0x000001bb20351000;

TEST(AllocUtils, RoundAddressUp)
{
    EXPECT_EQ(RoundAddressUpToPowerOfTwo(UnalignedAddress, 4096), NextPage);
}

TEST(AllocUtils, RoundAddressDown)
{
    EXPECT_EQ(RoundAddressDownToPowerOfTwo(UnalignedAddress, 4096), AlignedAddress1);
}

TEST(AllocUtils, AddressDistance)
{
    size_t Size = 7;
    uintptr_t End = UnalignedAddress + Size;
    EXPECT_EQ(AddressDistance(UnalignedAddress, End), 7);
}

TEST(AllocUtils, IsStraddlingBoundary)
{
    EXPECT_EQ(IsStraddlingBoundary(AlignedAddress1, 7, 4096), true);
}

TEST(AllocUtils, IsAligned)
{
    uintptr_t Start = 0xFF1040; // true
    uintptr_t End = 0xFF1048; // true

    EXPECT_EQ(IsAligned(Start), true);

    // Test every address in-between
    for (uintptr_t Current = Start + 1; Current < End; Current++) {
        EXPECT_EQ(IsAligned(Current), false);
    }

    EXPECT_EQ(IsAligned(End), true);
}

TEST(AllocUtils, AlignOffset)
{
    EXPECT_EQ(AlignOffset(AlignedAddress1), 0);
    EXPECT_EQ(AlignOffset(UnalignedAddress), 7);
}

TEST(AllocUtils, AlignAddress)
{
    EXPECT_EQ(AlignAddress(UnalignedAddress), AlignedAddress2);
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

TEST_F(LinearAllocatorTest, Info)
{
    void *Mem1 = LinearAllocatorAPI->Alloc(LinearAllocator, 64, __FILE__, __LINE__);
    AxAllocatorData *Data = AllocatorRegistryAPI->GetAllocatorDataByName("LinearTest");
    //EXPECT_STREQ(AllocatorDataAPI->Name(Data), "LinearTest");
    EXPECT_EQ(AllocatorDataAPI->BytesReserved(Data), 65536);
    EXPECT_EQ(AllocatorDataAPI->BytesCommitted(Data), 64);
    EXPECT_EQ(AllocatorDataAPI->PageSize(Data), 4096);
    EXPECT_EQ(AllocatorDataAPI->AllocationGranularity(Data), 65536);
    EXPECT_EQ(AllocatorDataAPI->PagesReserved(Data), 16);
    EXPECT_EQ(AllocatorDataAPI->PagesCommitted(Data), 1);
}