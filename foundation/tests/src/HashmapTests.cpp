#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"

class HashMapTest : public testing::Test
{
    protected:
        AxHashTable *Table;

        void SetUp()
        {
            Table = HashTableAPI->CreateTable();
        }

        void TearDown()
        {
            HashTableAPI->DestroyTable(Table);
            free(Table);
        }
};

TEST_F(HashMapTest, SetAndFind)
{
    HashTableAPI->Set(Table, "1", "First address");
    HashTableAPI->Set(Table, "2", "Second address");

    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "1"), "First address");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "2"), "Second address");
}

TEST_F(HashMapTest, KeyDoesNotExist)
{
    EXPECT_EQ((int64_t)HashTableAPI->Find(Table, "1"), NULL);
}

TEST_F(HashMapTest, Collision)
{
    HashTableAPI->Set(Table, "1", "First address");
    HashTableAPI->Set(Table, "2", "Second address");
    HashTableAPI->Set(Table, "Hel", "Third address");
    HashTableAPI->Set(Table, "Cau", "Fourth address");
    HashTableAPI->Set(Table, "Dbs", "Fifth address");

    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Hel"), "Third address");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Cau"), "Fourth address");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Dbs"), "Fifth address");
}

TEST_F(HashMapTest, Remove)
{
   HashTableAPI->Set(Table, "1", "First address");
   HashTableAPI->Set(Table, "2", "Second address");
   HashTableAPI->Set(Table, "Hel", "Third address");
   HashTableAPI->Set(Table, "Cau", "Fourth address");
   HashTableAPI->Set(Table, "Dbs", "Fifth address");

   HashTableAPI->Remove(Table, "2");
   HashTableAPI->Remove(Table, "Cau");

   EXPECT_STREQ((char *)HashTableAPI->Find(Table, "1"), "First address");
   EXPECT_STREQ((char *)HashTableAPI->Find(Table, "2"), NULL);
   EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Hel"), "Third address");
   EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Cau"), NULL);
   EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Dbs"), "Fifth address");
}

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

    HashTableAPI->Set(Table, "Bar", BarIn);
    Bar *BarOut = (Bar *)HashTableAPI->Find(Table, "Bar");
    EXPECT_EQ(BarOut->Min, 10);
    EXPECT_EQ(BarOut->Max, 20);

    free(BarIn);
}

TEST_F(HashMapTest, StoringIntegers)
{
    HashTableAPI->Set(Table, "Number", (void *)100);
    void *Entry = (int64_t *)HashTableAPI->Find(Table, "Number");
    EXPECT_EQ((int64_t)Entry, 100);
}

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

    HashTableAPI->Set(Table, "Bar", (void *)MM);

    MinMax *Result = (MinMax *)HashTableAPI->Find(Table, "Bar");
    EXPECT_EQ(MM->Min, Result->Min);
    EXPECT_EQ(MM->Max, Result->Max);
    free(MM);
}

TEST_F(HashMapTest, Iterate)
{
    HashTableAPI->Set(Table, "1", "First address");
    HashTableAPI->Set(Table, "2", "Second address");
    HashTableAPI->Set(Table, "Hel", "Third address");
    HashTableAPI->Set(Table, "Cau", "Fourth address");
    HashTableAPI->Set(Table, "Dbs", "Fifth address");

    HashTableAPI->Remove(Table, "2");
    HashTableAPI->Remove(Table, "Cau");

    EXPECT_STREQ((char *)HashTableAPI->GetKeyAtIndex(Table, 0), "Dbs");
    EXPECT_STREQ((char *)HashTableAPI->GetKeyAtIndex(Table, 1), "1");
    EXPECT_STREQ((char *)HashTableAPI->GetKeyAtIndex(Table, 2), "Hel");
    EXPECT_STREQ((char *)HashTableAPI->GetValueAtIndex(Table, 0), "Fifth address");
    EXPECT_STREQ((char *)HashTableAPI->GetValueAtIndex(Table, 1), "First address");
    EXPECT_STREQ((char *)HashTableAPI->GetValueAtIndex(Table, 2), "Third address");
}

TEST_F(HashMapTest, Expansion)
{
    HashTableAPI->Set(Table, "H", "Hydrogen");     // 0.125
    HashTableAPI->Set(Table, "He", "Helium");      // 0.25
    HashTableAPI->Set(Table, "Li", "Lithium");     // 0.375
    HashTableAPI->Set(Table, "Be", "Beryllium");   // 0.5

    EXPECT_EQ(HashTableAPI->Size(Table), 4);
    EXPECT_EQ(HashTableAPI->Capacity(Table), 8);

    HashTableAPI->Set(Table, "B", "Boron");        // 0.625
    HashTableAPI->Set(Table, "C", "Carbon");       // 0.75
    HashTableAPI->Set(Table, "N", "Nitrogen");     // 0.875

    EXPECT_EQ(HashTableAPI->Size(Table), 7);
    EXPECT_EQ(HashTableAPI->Capacity(Table), 16);

    HashTableAPI->Set(Table, "O", "Oxygen");       // 0.5

    EXPECT_EQ(HashTableAPI->Size(Table), 8);
    EXPECT_EQ(HashTableAPI->Capacity(Table), 16);

    HashTableAPI->Set(Table, "F", "Fluorine");     // 0.5625
    HashTableAPI->Set(Table, "Ne", "Neon");        // 0.625
    HashTableAPI->Set(Table, "Na", "Sodium");      // 0.6875
    HashTableAPI->Set(Table, "Mg", "Magnesium");   // 0.75
    HashTableAPI->Set(Table, "Al", "Aluminum");    // 0.8125

    EXPECT_EQ(HashTableAPI->Size(Table), 13);
    EXPECT_EQ(HashTableAPI->Capacity(Table), 32);

    HashTableAPI->Set(Table, "Si", "Silicon");     // 0.4375
    HashTableAPI->Set(Table, "P", "Phosphorus");   // 0.46875
    HashTableAPI->Set(Table, "S", "Sulfur");       // 0.5

    EXPECT_EQ(HashTableAPI->Size(Table), 16);
    EXPECT_EQ(HashTableAPI->Capacity(Table), 32);

    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "H"), "Hydrogen");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "He"), "Helium");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Li"), "Lithium");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Be"), "Beryllium");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "B"), "Boron");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "C"), "Carbon");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "N"), "Nitrogen");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "O"), "Oxygen");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "F"), "Fluorine");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Ne"), "Neon");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Na"), "Sodium");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Mg"), "Magnesium");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Al"), "Aluminum");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "Si"), "Silicon");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "P"), "Phosphorus");
    EXPECT_STREQ((char *)HashTableAPI->Find(Table, "S"), "Sulfur");
}