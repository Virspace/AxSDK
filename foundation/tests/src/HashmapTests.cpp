#include "gtest/gtest.h"
#include "Foundation/Types.h"

extern "C"
{
    #include "Foundation/HashTable.h"
    #include "Foundation/LinkedList.h"
}

struct Foo
{
    int32_t Value;
    AxLink Node;
};

struct TestAPI
{
    int32_t (*Add)(int32_t a, int32_t b);
    int32_t (*Subtract)(int32_t a, int32_t b);
};

static int32_t Add(int32_t a, int32_t b)
{
    return (a + b);
}

static int32_t Subtract(int32_t a, int32_t b)
{
    return (a - b);
}

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

class HashMapTest : public testing::Test
{
    protected:
        HashTable *Table;

        void SetUp()
        {
            Table = CreateTable(16);
        }
};

TEST_F(HashMapTest, InsertAndSearch)
{
    HashInsert(Table, "1", "First address");
    HashInsert(Table, "2", "Second address");

    const char *Result1 = (char *)HashTableSearch(Table, "1");
    const char* Result2 = (char *)HashTableSearch(Table, "2");
    EXPECT_STREQ(Result1, "First address");
    EXPECT_STREQ(Result2, "Second address");
}

TEST_F(HashMapTest, KeyDoesNotExist)
{
    EXPECT_EQ((int64_t)HashTableSearch(Table, "1"), NULL);
}

TEST_F(HashMapTest, Collision)
{
    HashInsertString(Table, "1", "First address");
    HashInsertString(Table, "2", "Second address");
    HashInsertString(Table, "Hel", "Third address");
    HashInsertString(Table, "Cau", "Fourth address");
    HashInsertString(Table, "Dbs", "Fifth address");

    char *Result1 = (char *)HashTableSearch(Table, "Hel");
    EXPECT_STREQ(Result1, "Third address");

    char *Result2 = (char *)HashTableSearch(Table, "Cau");
    EXPECT_STREQ(Result2, "Fourth address");

    char *Result3 = (char *)HashTableSearch(Table, "Dbs");
    EXPECT_STREQ(Result3, "Fifth address");
}

//TEST_F(HashMapTest, HashDelete)
//{
//    HashInsertString(Table, "1", "First address");
//    HashInsertString(Table, "2", "Second address");
//    HashInsertString(Table, "Hel", "Third address");
//    HashInsertString(Table, "Cau", "Fourth address");
//    HashInsertString(Table, "Dbs", "Fifth address");
//
//    HashDelete(Table, "2");
//    EXPECT_STREQ((char *)HashTableSearch(Table, "2"), NULL);
//
//    HashDelete(Table, "Cau");
//    EXPECT_STREQ((char *)HashTableSearch(Table, "Hel"), "Third address");
//    EXPECT_STREQ((char *)HashTableSearch(Table, "Dbs"), "Fifth address");
//}

TEST_F(HashMapTest, NonStringData)
{
    struct Bar
    {
        int32_t Min;
        int32_t Max;
    };

    Bar *BarIn = (Bar *)malloc(sizeof(Bar));
    if (BarIn)
    {
        BarIn->Min = 10;
        BarIn->Max = 20;
    }

    HashInsert(Table, "Bar", BarIn);
    Bar *BarOut = (Bar *)HashTableSearch(Table, "Bar");
    EXPECT_EQ(BarOut->Min, 10);
    EXPECT_EQ(BarOut->Max, 20);

    free(BarIn);
}

//TEST_F(HashMapTest, ItemResize)
//{
//    struct Bar1
//    {
//        int32_t Value;
//    };
//
//    struct Bar2
//    {
//        int32_t Min;
//        int32_t Max;
//    };
//
//    Bar1 *Bar1In = (Bar1*)malloc(sizeof(Bar1));
//    if (Bar1In)
//    {
//        Bar1In->Value = 10;
//        HashInsert(Table, "Ba", Bar1In);
//        HashInsert(Table, "Bar", Bar1In);
//    }
//
//    Bar2 *Bar2In = (Bar2*)malloc(sizeof(Bar2));
//    Bar2In->Min = 20;
//    Bar2In->Max = 30;
//    HashInsert(Table, "Bar", Bar2In);
//    HashInsert(Table, "Bars", Bar2In);
//
//    Bar2 *BaOut = (Bar2 *)HashTableSearch(Table, "Ba");
//    Bar2 *Bar2Out = (Bar2 *)HashTableSearch(Table, "Bar");
//    Bar2 *BarsOut = (Bar2 *)HashTableSearch(Table, "Bars");
//    
//    EXPECT_EQ(BaOut->Min, 10);
//    EXPECT_EQ(Bar2Out->Min, 20);
//    EXPECT_EQ(Bar2Out->Max, 30);
//    EXPECT_EQ(BarsOut->Min, 20);
//    EXPECT_EQ(BarsOut->Max, 30);
//
//    free(Bar1In);
//    free(Bar2In);
//}

TEST_F(HashMapTest, DestroyTable)
{
    struct Bar
    {
        int32_t Min;
        int32_t Max;
    };

    Bar* BarIn = (Bar*)malloc(sizeof(Bar));
    if (BarIn)
    {
        BarIn->Min = 10;
        BarIn->Max = 20;
    }

    HashInsert(Table, "Bar", BarIn);
    Bar* BarOut = (Bar*)HashTableSearch(Table, "Bar");
    EXPECT_EQ(BarOut->Min, 10);
    EXPECT_EQ(BarOut->Max, 20);

    DestroyTable(Table);
    free(BarIn);
}

// TEST_F(HashMapTest, StoringIntegers)
// {
//     int32_t *Number = (int32_t *)malloc(sizeof(int32_t));
//     *Number = 123;
//     HashInsert(Table, "Number", (void *)Number, sizeof(void *));
//     int32_t *Result = (int32_t *)HashTableSearch(Table, "Number");
//     EXPECT_EQ(*Number, *Result);
// }

TEST_F(HashMapTest, StoringPointers)
{
    struct MinMax
    {
        int32_t Min;
        int32_t Max;
    };

    MinMax *MM = (MinMax *)malloc(sizeof(MinMax));
    if (MM)
    {
        MM->Min = 10;
        MM->Max = 20;
    }

    HashInsert(Table, "Bar", (void *)MM);

    MinMax *Result = (MinMax *)HashTableSearch(Table, "Bar");
    EXPECT_EQ(MM->Min, Result->Min);
    EXPECT_EQ(MM->Max, Result->Max);
}

TEST_F(HashMapTest, Iterate)
{
    HashInsert(Table, "1", "First address");
    HashInsert(Table, "2", "Second address");

    int32_t Length = GetHashTableLength(Table);
    EXPECT_EQ(Length, 2);

    const char *Result1 = (char *)GetHashTableEntry(Table, 0);
    const char *Result2 = (char *)GetHashTableEntry(Table, 1);

    EXPECT_STREQ(Result1, "Second address");
    EXPECT_STREQ(Result2, "First address");
}

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