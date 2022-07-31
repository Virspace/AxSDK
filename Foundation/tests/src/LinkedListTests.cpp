#include "gtest/gtest.h"
#include "Foundation/Types.h"
#include "Foundation/LinkedList.h"

struct Foo
{
    int32_t Value;
    AxLink Node;
};

static int32_t Add(int32_t a, int32_t b)
{
    return (a + b);
}

static int32_t Subtract(int32_t a, int32_t b)
{
    return (a - b);
}

struct TestAPI
{
    int32_t (*Add)(int32_t a, int32_t b);
    int32_t (*Subtract)(int32_t a, int32_t b);
};

// Designated initializer Work around for C++
static TestAPI STestAPI {
    Add,
    Subtract
};

TestAPI *ATestAPI = &STestAPI;

class LinkedListTest : public testing::Test
{
    protected:
        AxLink ListHead;
        Foo FooArray[3];

        void SetUp()
        {
            FooArray[0].Value = 10;
            FooArray[1].Value = 20;
            FooArray[2].Value = 30;

            InitList(&ListHead);
        }
};

TEST_F(LinkedListTest, InitList)
{
    EXPECT_EQ(ListHead.Next, &ListHead);
    EXPECT_EQ(ListHead.Prev, &ListHead);
}

TEST_F(LinkedListTest, InsertListHead)
{
    // ->[H]<->[2]<->[1]<->[0]<-
    InsertListHead(&FooArray[0].Node, &ListHead);
    InsertListHead(&FooArray[1].Node, &ListHead);
    InsertListHead(&FooArray[2].Node, &ListHead);

    AxLink *Current = ListHead.Next;
    int32_t Result1 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result1, 30);
    Current = Current->Next;
    int32_t Result2 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result2, 20);
    Current = Current->Next;
    int32_t Result3 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result3, 10);
}

TEST_F(LinkedListTest, InsertListTail)
{
    // ->[H]<->[0]<->[1]<->[2]<-
    InsertListTail(&FooArray[0].Node, &ListHead);
    InsertListTail(&FooArray[1].Node, &ListHead);
    InsertListTail(&FooArray[2].Node, &ListHead);

    AxLink *Current = ListHead.Next;
    int32_t Result1 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result1, 10);
    Current = Current->Next;
    int32_t Result2 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result2, 20);
    Current = Current->Next;
    int32_t Result3 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result3, 30);
}

TEST_F(LinkedListTest, RemoveListHead)
{
    // ->[H]<->[2]<->[1]<->[0]<-
    InsertListHead(&FooArray[0].Node, &ListHead);
    InsertListHead(&FooArray[1].Node, &ListHead);
    InsertListHead(&FooArray[2].Node, &ListHead);

    // ->[H]<->[1]<->[0]<-
    AxLink *ExHead = RemoveListHead(&ListHead);
    EXPECT_EQ(ExHead, &FooArray[2].Node);
    EXPECT_EQ(ListHead.Next, &FooArray[1].Node);
}

TEST_F(LinkedListTest, RemoveListTail)
{
    // ->[H]<->[0]<->[1]<->[2]<-
    InsertListTail(&FooArray[0].Node, &ListHead);
    InsertListTail(&FooArray[1].Node, &ListHead);
    InsertListTail(&FooArray[2].Node, &ListHead);

    // ->[H]<->[0]<->[1]<-
    AxLink *ExTail = RemoveListTail(&ListHead);
    EXPECT_EQ(ExTail, &FooArray[2].Node);
    EXPECT_EQ(ListHead.Prev, &FooArray[1].Node);
}

TEST_F(LinkedListTest, RemoveListLink)
{
    // ->[H]<->[0]<->[1]<->[2]<-
    InsertListTail(&FooArray[0].Node, &ListHead);
    InsertListTail(&FooArray[1].Node, &ListHead);
    InsertListTail(&FooArray[2].Node, &ListHead);

    // ->[H]<->[1]<->[2]<-
    bool Result1 = RemoveListLink(ListHead.Next);
    EXPECT_EQ(ListHead.Next, &FooArray[1].Node);
    EXPECT_EQ(Result1, false);

    // ->[H]<->[2]<-
    bool Result2 = RemoveListLink(ListHead.Next);
    EXPECT_EQ(ListHead.Next, &FooArray[2].Node);
    EXPECT_EQ(Result2, false);

    // ->[H]<-
    bool Result3 = RemoveListLink(ListHead.Next);
    EXPECT_EQ(ListHead.Next, &ListHead);
    EXPECT_EQ(Result3, true);

    // Remove ListHead to create a headless list.
    bool Result4 = RemoveListLink(&ListHead);
    EXPECT_EQ(Result4, true);
}

TEST_F(LinkedListTest, IsListEmpty)
{
    EXPECT_EQ(IsListEmpty(&ListHead), true);
    InsertListTail(&FooArray[0].Node, &ListHead);
    EXPECT_EQ(IsListEmpty(&ListHead), false);
}

TEST_F(LinkedListTest, AppendListTail)
{
    // Create new list ->[H]<->[0]<->[1]<->[2]<-
    AxLink ListToAppend;

    Foo FooArray2[3];
    FooArray2[0].Value = 40;
    FooArray2[1].Value = 50;
    FooArray2[2].Value = 60;

    InitList(&ListToAppend);
    InsertListTail(&FooArray2[0].Node, &ListToAppend);
    InsertListTail(&FooArray2[1].Node, &ListToAppend);
    InsertListTail(&FooArray2[2].Node, &ListToAppend);

    // Setup existing list ->[H]<->[0]<->[1]<->[2]<-
    InsertListTail(&FooArray[0].Node, &ListHead);
    InsertListTail(&FooArray[1].Node, &ListHead);
    InsertListTail(&FooArray[2].Node, &ListHead);

    // ->[H]<->[0]<->[1]<->[2]<->[3]<->[4]<->[5]<-
    AxLink *Entry = ListToAppend.Next;

    RemoveListLink(&ListToAppend);
    InitList(&ListToAppend);
    AppendListTail(Entry, &ListHead);

    AxLink *Current = ListHead.Next;
    int32_t Result1 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result1, 10);

    Current = Current->Next;
    int32_t Result2 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result2, 20);

    Current = Current->Next;
    int32_t Result3 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result3, 30);

    Current = Current->Next;
    int32_t Result4 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result4, 40);

    Current = Current->Next;
    int32_t Result5 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result5, 50);

    Current = Current->Next;
    int32_t Result6 = container_of(Current, Foo, Node)->Value;
    EXPECT_EQ(Result6, 60);
}