#include "gtest/gtest.h"
#include "Foundation/Types.h"

extern "C"
{
    #include "Foundation/AxArray.h"
}

TEST(AxArrayTest, AddElement)
{
    AX_ARRAY(Foo);
    AxArrayPushBack(Foo, (void *)10);
    AxArrayPushBack(Foo, (void *)20);
    AxArrayPushBack(Foo, (void *)30);

    EXPECT_EQ((int32_t)AxArrayAt(Foo, 0), 10);
    EXPECT_EQ((int32_t)AxArrayAt(Foo, 1), 20);
    EXPECT_EQ((int32_t)AxArrayAt(Foo, 2), 30);
    
    //EXPECT_STREQ(Result2, "Second address");
}