#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxAPIRegistry.h"

class PlatformTest : public testing::Test
{
    protected:
        AxPlatformAPI *PlatformAPI;
        AxPlatformFileAPI *FileAPI;
        AxPlatformPathAPI *PathAPI;

        void SetUp()
        {
            AxonInitGlobalAPIRegistry();
            AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

            PlatformAPI = (AxPlatformAPI *)AxonGlobalAPIRegistry->Get(AXON_PLATFORM_API_NAME);
            FileAPI = PlatformAPI->FileAPI;
            PathAPI = PlatformAPI->PathAPI;
        }

        void TearDown()
        {
            AxonTermGlobalAPIRegistry();
        }
};

TEST_F(PlatformTest, WriteFile)
{
    const char *Path = "Test.txt";
    AxFile File = FileAPI->OpenForWrite(Path);

    const char *Buffer = "Hello world";
    FileAPI->Write(File, (void *)Buffer, strlen(Buffer));
    FileAPI->Close(File);

    EXPECT_EQ(PathAPI->FileExists(Path), true);
}

TEST_F(PlatformTest, ReadFile)
{
    AxFile File = FileAPI->OpenForRead("Test.txt");
    uint64_t Size = FileAPI->Size(File);

    char *Buffer = (char *)malloc(Size + 1);
    Buffer[Size] = '\0';
    FileAPI->Read(File, Buffer, Size);
    FileAPI->Close(File);

    EXPECT_STREQ((char *)Buffer, "Hello world");
}
