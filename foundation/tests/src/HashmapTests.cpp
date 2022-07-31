#include "gtest/gtest.h"
#include "Foundation/Types.h"
#include "Foundation/AxHashTable.h"

class HashMapTest : public testing::Test
{
    protected:
        AxHashTable *Table;

        void SetUp()
        {
            Table = HashTableAPI->CreateTable(16);
        }

        void TearDown()
        {
            HashTableAPI->DestroyTable(Table);
        }
};

TEST_F(HashMapTest, InsertAndSearch)
{
    HashTableAPI->Insert(Table, "1", "First address");
    HashTableAPI->Insert(Table, "2", "Second address");

    const char *Result1 = (char *)HashTableAPI->Search(Table, "1");
    const char* Result2 = (char *)HashTableAPI->Search(Table, "2");
    EXPECT_STREQ(Result1, "First address");
    EXPECT_STREQ(Result2, "Second address");
}

TEST_F(HashMapTest, KeyDoesNotExist)
{
    EXPECT_EQ((int64_t)HashTableAPI->Search(Table, "1"), NULL);
}

TEST_F(HashMapTest, Collision)
{
    HashTableAPI->Insert(Table, "1", "First address");
    HashTableAPI->Insert(Table, "2", "Second address");
    HashTableAPI->Insert(Table, "Hel", "Third address");
    HashTableAPI->Insert(Table, "Cau", "Fourth address");
    HashTableAPI->Insert(Table, "Dbs", "Fifth address");

    char *Result1 = (char *)HashTableAPI->Search(Table, "Hel");
    EXPECT_STREQ(Result1, "Third address");

    char *Result2 = (char *)HashTableAPI->Search(Table, "Cau");
    EXPECT_STREQ(Result2, "Fourth address");

    char *Result3 = (char *)HashTableAPI->Search(Table, "Dbs");
    EXPECT_STREQ(Result3, "Fifth address");
}

//TEST_F(HashMapTest, HashDelete)
//{
//    HashTableAPI->Insert(Table, "1", "First address");
//    HashTableAPI->Insert(Table, "2", "Second address");
//    HashTableAPI->Insert(Table, "Hel", "Third address");
//    HashTableAPI->Insert(Table, "Cau", "Fourth address");
//    HashTableAPI->Insert(Table, "Dbs", "Fifth address");
//
//    HashDelete(Table, "2");
//    EXPECT_STREQ((char *)HashTableAPI->Search(Table, "2"), NULL);
//
//    HashDelete(Table, "Cau");
//    EXPECT_STREQ((char *)HashTableAPI->Search(Table, "Hel"), "Third address");
//    EXPECT_STREQ((char *)HashTableAPI->Search(Table, "Dbs"), "Fifth address");
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

    HashTableAPI->Insert(Table, "Bar", BarIn);
    Bar *BarOut = (Bar *)HashTableAPI->Search(Table, "Bar");
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
//        HashTableAPI->Insert(Table, "Ba", Bar1In);
//        HashTableAPI->Insert(Table, "Bar", Bar1In);
//    }
//
//    Bar2 *Bar2In = (Bar2*)malloc(sizeof(Bar2));
//    Bar2In->Min = 20;
//    Bar2In->Max = 30;
//    HashTableAPI->Insert(Table, "Bar", Bar2In);
//    HashTableAPI->Insert(Table, "Bars", Bar2In);
//
//    Bar2 *BaOut = (Bar2 *)HashTableAPI->Search(Table, "Ba");
//    Bar2 *Bar2Out = (Bar2 *)HashTableAPI->Search(Table, "Bar");
//    Bar2 *BarsOut = (Bar2 *)HashTableAPI->Search(Table, "Bars");
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

// TEST_F(HashMapTest, StoringIntegers)
// {
//     int32_t *Number = (int32_t *)malloc(sizeof(int32_t));
//     *Number = 123;
//     HashTableAPI->Insert(Table, "Number", (void *)Number, sizeof(void *));
//     int32_t *Result = (int32_t *)HashTableAPI->Search(Table, "Number");
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

    HashTableAPI->Insert(Table, "Bar", (void *)MM);

    MinMax *Result = (MinMax *)HashTableAPI->Search(Table, "Bar");
    EXPECT_EQ(MM->Min, Result->Min);
    EXPECT_EQ(MM->Max, Result->Max);
    free(MM);
}

TEST_F(HashMapTest, Iterate)
{
    HashTableAPI->Insert(Table, "1", "First address");
    HashTableAPI->Insert(Table, "2", "Second address");

    size_t Length = HashTableAPI->Length(Table);
    EXPECT_EQ(Length, 2);

    const char *Result1 = (char *)HashTableAPI->GetHashTableValue(Table, 0);
    const char *Result2 = (char *)HashTableAPI->GetHashTableValue(Table, 1);

    EXPECT_STREQ(Result1, "Second address");
    EXPECT_STREQ(Result2, "First address");
}