#include "gtest/gtest.h"
#include "Foundation/Types.h"

#define AXARRAY_IMPLEMENTATION
#include "Foundation/AxArray.h"

struct Foo { int32_t a; char *b; };
#define FooConstruct(n, t) (Foo { n, t })

class AxArrayTest : public testing::Test
{
protected:
    Foo* Arr;

    void SetUp()
    {
        Arr = NULL;

        ArrayPush(Arr, FooConstruct(1, "Fly,"));
        ArrayPush(Arr, FooConstruct(2, "you"));
        ArrayPush(Arr, FooConstruct(3, "fools!"));
    }

    void TearDown()
    {
        ArrayFree(Arr);
    }
};

TEST_F(AxArrayTest, Header)
{
    ASSERT_NE(Arr, nullptr);

    EXPECT_EQ(ArrayHeader(Arr)->Capacity, 16);
    EXPECT_EQ(ArrayHeader(Arr)->Size, 3);
}

TEST_F(AxArrayTest, Empty)
{
    EXPECT_FALSE(ArrayEmpty(Arr));
    ArraySetSize(Arr, 0);
    EXPECT_TRUE(ArrayEmpty(Arr));
}

// TODO(mdeforge): Review the stb API again and think about if those simple commands will suffice
// Also add a insert and remove at, look at the std page for good details on how

TEST_F(AxArrayTest, Capacity)
{
    EXPECT_EQ(ArrayCapacity(Arr), 16);
}

TEST_F(AxArrayTest, SetCapacity)
{
    EXPECT_EQ(ArrayCapacity(Arr), 16);
    ArraySetCapacity(Arr, 32);
    EXPECT_EQ(ArrayCapacity(Arr), 32);
}

TEST_F(AxArrayTest, ZeroSize)
{
    Foo* ZeroArr = NULL;
    EXPECT_EQ(ArraySize(ZeroArr), 0);
}

TEST_F(AxArrayTest, Size)
{
    EXPECT_EQ(ArraySize(Arr), 3);
}

TEST_F(AxArrayTest, SetSize)
{
    EXPECT_EQ(ArraySize(Arr), 3);
    ArraySetSize(Arr, 1);
    EXPECT_EQ(ArraySize(Arr), 1);
}

TEST_F(AxArrayTest, SizeInBytes)
{
    EXPECT_EQ(ArraySizeInBytes(Arr), sizeof(Foo) * 3);
}

TEST_F(AxArrayTest, NeedsGrowth)
{
    EXPECT_TRUE(ArrayNeedsGrowth(Arr, 18));
}

// [Element Access]

TEST_F(AxArrayTest, Begin)
{
    EXPECT_EQ(ArrayBegin(Arr).a, 1);
}

TEST_F(AxArrayTest, End)
{
    Foo *end = Arr + ArraySize(Arr);
    EXPECT_EQ(ArrayEnd(Arr), end);
}

TEST_F(AxArrayTest, Back)
{
    EXPECT_EQ(ArrayBack(Arr)->a, 3);
}

TEST_F(AxArrayTest, Pop)
{
    EXPECT_EQ(ArrayPop(Arr).a, 3);
    EXPECT_EQ(ArrayPop(Arr).a, 2);
    EXPECT_EQ(ArrayPop(Arr).a, 1);
}

TEST_F(AxArrayTest, NthAccess)
{
    EXPECT_EQ(Arr[0].a, 1);
    EXPECT_EQ(Arr[1].a, 2);
    EXPECT_EQ(Arr[2].a, 3);
    EXPECT_STREQ(Arr[0].b, "Fly,");
    EXPECT_STREQ(Arr[1].b, "you");
    EXPECT_STREQ(Arr[2].b, "fools!");
}

TEST_F(AxArrayTest, Last)
{
    EXPECT_EQ(ArrayBack(Arr)->a, 3);
    EXPECT_STREQ(ArrayBack(Arr)->b, "fools!");
}

TEST_F(AxArrayTest, Push)
{
    int* IntArr = NULL;

    ArrayPush(IntArr, 1);
    ArrayPush(IntArr, 2);
    ArrayPush(IntArr, 3);

    EXPECT_EQ(IntArr[0], 1);
    EXPECT_EQ(IntArr[1], 2);
    EXPECT_EQ(IntArr[2], 3);

    ArrayFree(IntArr);
}

TEST_F(AxArrayTest, PushArray)
{
    Foo *Arr2 = NULL;

    ArrayPush(Arr2, FooConstruct(4, "You"));
    ArrayPush(Arr2, FooConstruct(5, "cannot"));
    ArrayPush(Arr2, FooConstruct(6, "pass"));

    ArrayPushArray(Arr, Arr2);

    EXPECT_EQ(Arr[3].a, 4);
    EXPECT_EQ(Arr[4].a, 5);
    EXPECT_EQ(Arr[5].a, 6);
    EXPECT_STREQ(Arr[3].b, "You");
    EXPECT_STREQ(Arr[4].b, "cannot");
    EXPECT_STREQ(Arr[5].b, "pass");

    EXPECT_EQ(ArraySize(Arr), 6);

    ArrayFree(Arr2);
}

TEST_F(AxArrayTest, Free)
{
    ArrayFree(Arr);
    EXPECT_EQ(ArraySize(Arr), 0);
    ArrayPush(Arr, FooConstruct(1, "Hello"));
    EXPECT_EQ(ArraySize(Arr), 1);
    EXPECT_EQ(Arr[0].a, 1);
    EXPECT_STREQ(Arr[0].b, "Hello");
}