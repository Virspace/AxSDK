#include "gtest/gtest.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "Foundation/AxTypes.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxAPIRegistry.h"

class PlatformTest : public testing::Test
{
    protected:
        AxPlatformAPI *PlatformAPI;
        AxPlatformFileAPI *FileAPI;
        AxPlatformPathAPI *PathAPI;
        AxPlatformDirectoryAPI *DirectoryAPI;

        void SetUp()
        {
            AxonInitGlobalAPIRegistry();
            AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

            PlatformAPI = (AxPlatformAPI *)AxonGlobalAPIRegistry->Get(AXON_PLATFORM_API_NAME);
            FileAPI = PlatformAPI->FileAPI;
            PathAPI = PlatformAPI->PathAPI;
            DirectoryAPI = PlatformAPI->DirectoryAPI;

            // Clean up any test directories that might exist from previous runs
            // Use recursive removal to clean up directories and their contents
            AxPlatformError errorCode;
            DirectoryAPI->RemoveDir("TestDirectory", true, &errorCode);
            DirectoryAPI->RemoveDir("TestIterationDir", true, &errorCode);
            DirectoryAPI->RemoveDir("EmptyTestDir", true, &errorCode);
            DirectoryAPI->RemoveDir("AlreadyExistsTestDir", true, &errorCode);
            DirectoryAPI->RemoveDir("RecursiveTestDir", true, &errorCode);

            // Clean up any individual test files that might be left over
            // (files created by tests that don't clean up after themselves)
            FileAPI->DeleteFile("TestFileForDirTest.txt", &errorCode);
            FileAPI->DeleteFile("Test.txt", &errorCode);
            FileAPI->DeleteFile("TestDeleteFile.txt", &errorCode);
            FileAPI->DeleteFile("NonExistentFile.txt", &errorCode);
        }

        void TearDown()
        {
            AxonTermGlobalAPIRegistry();
        }
};

TEST_F(PlatformTest, WriteFile)
{
    const char *Path = "Test.txt";
    const char *Content = "Hello world";
    
    // Write to file
    AxFile File = FileAPI->OpenForWrite(Path);
    EXPECT_TRUE(FileAPI->IsValid(File)) << "Failed to open file for writing";
    
    uint64_t BytesWritten = FileAPI->Write(File, (void *)Content, strlen(Content));
    FileAPI->Close(File);
    
    EXPECT_EQ(BytesWritten, strlen(Content)) << "Should write all bytes to file";
    EXPECT_TRUE(PathAPI->FileExists(Path)) << "File should exist after writing";
}

TEST_F(PlatformTest, ReadFile)
{
    const char *Path = "Test.txt";
    const char *ExpectedContent = "Hello world";
    
    // Create the file first
    AxFile WriteFile = FileAPI->OpenForWrite(Path);
    FileAPI->Write(WriteFile, (void *)ExpectedContent, strlen(ExpectedContent));
    FileAPI->Close(WriteFile);
    
    // Now read the file
    AxFile ReadFile = FileAPI->OpenForRead(Path);
    EXPECT_TRUE(FileAPI->IsValid(ReadFile)) << "Failed to open file for reading";
    
    uint64_t Size = FileAPI->Size(ReadFile);
    EXPECT_GT(Size, 0) << "File size should be greater than 0";
    
    char *Buffer = (char *)malloc(Size + 1);
    Buffer[Size] = '\0';
    uint64_t BytesRead = FileAPI->Read(ReadFile, Buffer, Size);
    FileAPI->Close(ReadFile);
    
    EXPECT_EQ(BytesRead, Size) << "Should read all bytes from file";
    EXPECT_STREQ(Buffer, ExpectedContent);
    
    free(Buffer);
}

TEST_F(PlatformTest, CreateAndRemoveDirectory)
{
    const char *TestDir = "TestDirectory";
    AxPlatformError errorCode;
    
    // Create directory
    EXPECT_TRUE(DirectoryAPI->CreateDir(TestDir, &errorCode)) 
        << "Failed to create directory. Error code: " << (int)errorCode;
    
    // Verify directory exists
    EXPECT_TRUE(PathAPI->DirectoryExists(TestDir));
    
    // Remove directory
    EXPECT_TRUE(DirectoryAPI->RemoveDir(TestDir, false, &errorCode))
        << "Failed to remove directory. Error code: " << (int)errorCode;
}

TEST_F(PlatformTest, ChangeDirectory)
{
    const char *TestDir = "TestDirectory";
    AxPlatformError errorCode;
    
    // Create directory
    EXPECT_TRUE(DirectoryAPI->CreateDir(TestDir, &errorCode))
        << "Failed to create directory. Error code: " << (int)errorCode;
    
    // Get current directory
    char *OriginalDir = DirectoryAPI->GetCurrentDirectory();
    EXPECT_NE(OriginalDir, (char*)nullptr);
    
    // Change to test directory
    EXPECT_TRUE(DirectoryAPI->ChangeDirectory(TestDir));
    
    // Change back to original directory
    EXPECT_TRUE(DirectoryAPI->ChangeDirectory(OriginalDir));
    

}

TEST_F(PlatformTest, DirectoryIteration)
{
    const char *TestDir = "TestIterationDir";
    AxPlatformError errorCode;
    
    // Create test directory
    EXPECT_TRUE(DirectoryAPI->CreateDir(TestDir, &errorCode))
        << "Failed to create directory. Error code: " << (int)errorCode;
    
    // Create some test files and subdirectories in the test directory
    char TestFile1Path[260];
    char TestFile2Path[260];
    char TestSubDirPath[260];
    
    sprintf(TestFile1Path, "%s\\TestFile1.txt", TestDir);
    sprintf(TestFile2Path, "%s\\TestFile2.txt", TestDir);
    sprintf(TestSubDirPath, "%s\\SubDir", TestDir);
    
    // Create test files
    AxFile File1 = FileAPI->OpenForWrite(TestFile1Path);
    const char *Content1 = "Test content 1";
    FileAPI->Write(File1, (void *)Content1, strlen(Content1));
    FileAPI->Close(File1);
    
    AxFile File2 = FileAPI->OpenForWrite(TestFile2Path);
    const char *Content2 = "Test content 2";
    FileAPI->Write(File2, (void *)Content2, strlen(Content2));
    FileAPI->Close(File2);
    
    // Create subdirectory
    EXPECT_TRUE(DirectoryAPI->CreateDir(TestSubDirPath, &errorCode))
        << "Failed to create subdirectory. Error code: " << (int)errorCode;
    
    // Open directory for iteration
    AxDirectoryHandle DirHandle = DirectoryAPI->OpenDirectory(TestDir);
    EXPECT_TRUE(DirectoryAPI->IsDirectoryHandleValid(DirHandle));
    
    // Iterate through directory entries
    AxDirectoryEntry Entry;
    int FileCount = 0;
    int DirCount = 0;
    bool FoundFile1 = false;
    bool FoundFile2 = false;
    bool FoundSubDir = false;
    
    while (DirectoryAPI->ReadDirectoryEntry(DirHandle, &Entry))
    {
        if (Entry.IsDirectory)
        {
            DirCount++;
            if (strcmp(Entry.Name, "SubDir") == 0)
            {
                FoundSubDir = true;
            }
        }
        else
        {
            FileCount++;
            if (strcmp(Entry.Name, "TestFile1.txt") == 0)
            {
                FoundFile1 = true;
                EXPECT_GT(Entry.Size, 0);
            }
            else if (strcmp(Entry.Name, "TestFile2.txt") == 0)
            {
                FoundFile2 = true;
                EXPECT_GT(Entry.Size, 0);
            }
        }
    }
    
    // Verify we found our test entries
    EXPECT_EQ(FileCount, 2);
    EXPECT_EQ(DirCount, 1);
    EXPECT_TRUE(FoundFile1);
    EXPECT_TRUE(FoundFile2);
    EXPECT_TRUE(FoundSubDir);
    
    // Close directory handle
    EXPECT_TRUE(DirectoryAPI->CloseDirectory(DirHandle));
    

}

TEST_F(PlatformTest, EmptyDirectoryIteration)
{
    const char *TestDir = "EmptyTestDir";
    AxPlatformError errorCode;
    
    // Create empty test directory
    EXPECT_TRUE(DirectoryAPI->CreateDir(TestDir, &errorCode))
        << "Failed to create empty directory. Error code: " << (int)errorCode;
    
    // Open directory for iteration
    AxDirectoryHandle DirHandle = DirectoryAPI->OpenDirectory(TestDir);
    EXPECT_TRUE(DirectoryAPI->IsDirectoryHandleValid(DirHandle));
    
    // Try to read from empty directory
    AxDirectoryEntry Entry;
    bool HasEntries = DirectoryAPI->ReadDirectoryEntry(DirHandle, &Entry);
    EXPECT_FALSE(HasEntries);
    
    // Close directory handle
    EXPECT_TRUE(DirectoryAPI->CloseDirectory(DirHandle));
    

}

TEST_F(PlatformTest, InvalidDirectoryHandle)
{
    AxDirectoryHandle InvalidHandle = { 0 };
    EXPECT_FALSE(DirectoryAPI->IsDirectoryHandleValid(InvalidHandle));
    
    // Don't call functions that would trigger assertions with invalid handles
    // These functions have assertions for development/debugging purposes
    // EXPECT_FALSE(DirectoryAPI->ReadDirectoryEntry(InvalidHandle, &Entry)); // Would trigger assertion
    // EXPECT_FALSE(DirectoryAPI->CloseDirectory(InvalidHandle)); // Would trigger assertion
    
    // Test that we can detect invalid handles without triggering assertions
    EXPECT_FALSE(DirectoryAPI->IsDirectoryHandleValid(InvalidHandle));
    
    // Test with another invalid handle
    AxDirectoryHandle AnotherInvalidHandle = { (uint64_t)-1 }; // INVALID_HANDLE_VALUE
    EXPECT_FALSE(DirectoryAPI->IsDirectoryHandleValid(AnotherInvalidHandle));
}

TEST_F(PlatformTest, DirectoryErrorHandling)
{
    AxPlatformError errorCode;
    
    // Test creating directory with invalid path
    EXPECT_FALSE(DirectoryAPI->CreateDir("", &errorCode));
    EXPECT_EQ(errorCode, AX_PLATFORM_ERROR_INVALID_PARAMETER);
    
    // Test removing non-existent directory
    EXPECT_FALSE(DirectoryAPI->RemoveDir("NonExistentDirectory", false, &errorCode));
    EXPECT_EQ(errorCode, AX_PLATFORM_ERROR_FILE_NOT_FOUND);
    
    // Test removing a file as if it were a directory
    const char *TestFile = "TestFileForDirTest.txt";
    AxFile File = FileAPI->OpenForWrite(TestFile);
    const char *Content = "Test content";
    FileAPI->Write(File, (void *)Content, strlen(Content));
    FileAPI->Close(File);
    
    EXPECT_FALSE(DirectoryAPI->RemoveDir(TestFile, false, &errorCode));
    EXPECT_EQ(errorCode, AX_PLATFORM_ERROR_ACCESS_DENIED);
    

}

TEST_F(PlatformTest, DirectoryAlreadyExists)
{
    const char *TestDir = "AlreadyExistsTestDir";
    AxPlatformError errorCode;
    
    // Create directory first time
    EXPECT_TRUE(DirectoryAPI->CreateDir(TestDir, &errorCode));
    EXPECT_EQ(errorCode, AX_PLATFORM_SUCCESS);
    
    // Try to create the same directory again
    EXPECT_FALSE(DirectoryAPI->CreateDir(TestDir, &errorCode));
    EXPECT_EQ(errorCode, AX_PLATFORM_ERROR_ALREADY_EXISTS);
    

}

TEST_F(PlatformTest, DeleteFile)
{
    const char *TestFile = "TestDeleteFile.txt";
    AxPlatformError errorCode;
    
    // Create a test file first
    AxFile File = FileAPI->OpenForWrite(TestFile);
    const char *Content = "Test content for deletion";
    FileAPI->Write(File, (void *)Content, strlen(Content));
    FileAPI->Close(File);
    
    // Verify file exists
    EXPECT_TRUE(PathAPI->FileExists(TestFile));
    
    // Delete the file
    EXPECT_TRUE(FileAPI->DeleteFile(TestFile, &errorCode))
        << "Failed to delete file. Error code: " << (int)errorCode;
    
    // Verify file no longer exists
    EXPECT_FALSE(PathAPI->FileExists(TestFile));
}

TEST_F(PlatformTest, DeleteFileErrorHandling)
{
    AxPlatformError errorCode;
    
    // Test deleting non-existent file
    EXPECT_FALSE(FileAPI->DeleteFile("NonExistentFile.txt", &errorCode));
    EXPECT_EQ(errorCode, AX_PLATFORM_ERROR_FILE_NOT_FOUND);
    
    // Test deleting with empty path
    EXPECT_FALSE(FileAPI->DeleteFile("", &errorCode));
    EXPECT_EQ(errorCode, AX_PLATFORM_ERROR_INVALID_PARAMETER);
    
    // Test deleting with null path (should be handled by assertion)
    // Note: This would cause an assertion failure in debug builds
}

TEST_F(PlatformTest, RecursiveDirectoryRemoval)
{
    const char *TestDir = "RecursiveTestDir";
    AxPlatformError errorCode;
    
    // Create test directory structure
    EXPECT_TRUE(DirectoryAPI->CreateDir(TestDir, &errorCode));
    
    // Create subdirectories
    char SubDir1[260], SubDir2[260], SubSubDir[260];
    sprintf(SubDir1, "%s\\SubDir1", TestDir);
    sprintf(SubDir2, "%s\\SubDir2", TestDir);
    sprintf(SubSubDir, "%s\\SubDir1\\SubSubDir", TestDir);
    
    EXPECT_TRUE(DirectoryAPI->CreateDir(SubDir1, &errorCode));
    EXPECT_TRUE(DirectoryAPI->CreateDir(SubDir2, &errorCode));
    EXPECT_TRUE(DirectoryAPI->CreateDir(SubSubDir, &errorCode));
    
    // Create files in various directories
    char File1[260], File2[260], File3[260];
    sprintf(File1, "%s\\file1.txt", TestDir);
    sprintf(File2, "%s\\SubDir1\\file2.txt", TestDir);
    sprintf(File3, "%s\\SubDir1\\SubSubDir\\file3.txt", TestDir);
    
    AxFile File1Handle = FileAPI->OpenForWrite(File1);
    const char *Content1 = "Test content 1";
    FileAPI->Write(File1Handle, (void *)Content1, strlen(Content1));
    FileAPI->Close(File1Handle);
    
    AxFile File2Handle = FileAPI->OpenForWrite(File2);
    const char *Content2 = "Test content 2";
    FileAPI->Write(File2Handle, (void *)Content2, strlen(Content2));
    FileAPI->Close(File2Handle);
    
    AxFile File3Handle = FileAPI->OpenForWrite(File3);
    const char *Content3 = "Test content 3";
    FileAPI->Write(File3Handle, (void *)Content3, strlen(Content3));
    FileAPI->Close(File3Handle);
    
    // Verify files exist
    EXPECT_TRUE(PathAPI->FileExists(File1));
    EXPECT_TRUE(PathAPI->FileExists(File2));
    EXPECT_TRUE(PathAPI->FileExists(File3));
    
    // Try to remove directory non-recursively (should fail)
    EXPECT_FALSE(DirectoryAPI->RemoveDir(TestDir, false, &errorCode));
    EXPECT_EQ(errorCode, AX_PLATFORM_ERROR_BUSY);
    
    // Remove directory recursively (should succeed)
    EXPECT_TRUE(DirectoryAPI->RemoveDir(TestDir, true, &errorCode))
        << "Failed to remove directory recursively. Error code: " << (int)errorCode;
    
    // Verify directory no longer exists
    EXPECT_FALSE(PathAPI->DirectoryExists(TestDir));
}

TEST_F(PlatformTest, SetPositionTest)
{
    const char *Path = "SetPositionTest.txt";
    const char *Content = "Hello world test for file positioning";
    const size_t ContentLength = strlen(Content);
    
    // Create a test file with known content
    AxFile WriteFile = FileAPI->OpenForWrite(Path);
    EXPECT_TRUE(FileAPI->IsValid(WriteFile));
    FileAPI->Write(WriteFile, (void *)Content, ContentLength);
    FileAPI->Close(WriteFile);
    
    // Open file for reading
    AxFile ReadFile = FileAPI->OpenForRead(Path);
    EXPECT_TRUE(FileAPI->IsValid(ReadFile));
    
    // Test setting position to beginning
    int64_t NewPosition = FileAPI->SetPosition(ReadFile, 0);
    EXPECT_EQ(NewPosition, 0) << "Should be able to set position to beginning";
    
    // Read first 5 characters from beginning
    char Buffer[10] = {0};
    uint64_t BytesRead = FileAPI->Read(ReadFile, Buffer, 5);
    EXPECT_EQ(BytesRead, 5);
    EXPECT_STREQ(Buffer, "Hello");
    
    // Test setting position to middle of file
    NewPosition = FileAPI->SetPosition(ReadFile, 6);
    EXPECT_EQ(NewPosition, 6) << "Should be able to set position to middle";
    
    // Read from middle position
    memset(Buffer, 0, sizeof(Buffer));
    BytesRead = FileAPI->Read(ReadFile, Buffer, 4);
    EXPECT_EQ(BytesRead, 4);
    EXPECT_STREQ(Buffer, "worl");
    
    // Test setting position to end of file
    NewPosition = FileAPI->SetPosition(ReadFile, ContentLength);
    EXPECT_EQ(NewPosition, (int64_t)ContentLength) << "Should be able to set position to end";
    
    // Try to read from end (should return 0 bytes)
    memset(Buffer, 0, sizeof(Buffer));
    BytesRead = FileAPI->Read(ReadFile, Buffer, 5);
    EXPECT_EQ(BytesRead, 0) << "Should read 0 bytes from end of file";
    
    // Test setting position beyond file size
    NewPosition = FileAPI->SetPosition(ReadFile, ContentLength + 10);
    EXPECT_EQ(NewPosition, (int64_t)(ContentLength + 10)) << "Should be able to set position beyond file size";
    
    // Try to read from beyond file size (should return 0 bytes)
    memset(Buffer, 0, sizeof(Buffer));
    BytesRead = FileAPI->Read(ReadFile, Buffer, 5);
    EXPECT_EQ(BytesRead, 0) << "Should read 0 bytes from beyond file size";
    
    // Test setting position back to beginning and reading full content
    NewPosition = FileAPI->SetPosition(ReadFile, 0);
    EXPECT_EQ(NewPosition, 0);
    
    char FullBuffer[100] = {0};
    BytesRead = FileAPI->Read(ReadFile, FullBuffer, ContentLength);
    EXPECT_EQ(BytesRead, ContentLength);
    EXPECT_STREQ(FullBuffer, Content);
    
    FileAPI->Close(ReadFile);
}

TEST_F(PlatformTest, BasePath) {
    // Normal paths
    EXPECT_STREQ(PathAPI->BasePath("/home/user/file.txt"), "/home/user");
    EXPECT_STREQ(PathAPI->BasePath("C:\\Users\\John\\file.txt"), "C:\\Users\\John");

    // No separator
    EXPECT_STREQ(PathAPI->BasePath("file.txt"), ".");
    EXPECT_STREQ(PathAPI->BasePath("document"), ".");

    // Root paths
    EXPECT_STREQ(PathAPI->BasePath("/file.txt"), "/");
    EXPECT_STREQ(PathAPI->BasePath("\\file.txt"), "\\");

    // Edge cases
    EXPECT_STREQ(PathAPI->BasePath(""), ".");
    EXPECT_STREQ(PathAPI->BasePath(NULL), ".");
    EXPECT_STREQ(PathAPI->BasePath("/home/user/"), "/home");
    EXPECT_STREQ(PathAPI->BasePath("C:\\Users\\"), "C:");

    // Mixed separators and relative paths
    EXPECT_STREQ(PathAPI->BasePath("C:/Users\\John/file.txt"), "C:/Users\\John");
    EXPECT_STREQ(PathAPI->BasePath("./dir/file.txt"), "./dir");
    EXPECT_STREQ(PathAPI->BasePath("../dir/file.txt"), "../dir");
}

TEST_F(PlatformTest, FileName) {
    // Normal paths
    EXPECT_STREQ(PathAPI->FileName("/home/user/file.txt"), "file.txt");
    EXPECT_STREQ(PathAPI->FileName("C:\\Users\\John\\document.doc"), "document.doc");

    // No separator
    EXPECT_STREQ(PathAPI->FileName("file.txt"), "file.txt");
    EXPECT_STREQ(PathAPI->FileName("document"), "document");

    // Root paths
    EXPECT_STREQ(PathAPI->FileName("/file.txt"), "file.txt");
    EXPECT_STREQ(PathAPI->FileName("\\file.txt"), "file.txt");

    // Edge cases
    EXPECT_STREQ(PathAPI->FileName(""), "");
    EXPECT_STREQ(PathAPI->FileName(NULL), "");
    EXPECT_STREQ(PathAPI->FileName("/home/user/"), "");
    EXPECT_STREQ(PathAPI->FileName("C:\\Users\\"), "");
    EXPECT_STREQ(PathAPI->FileName("/"), "");
    EXPECT_STREQ(PathAPI->FileName("\\"), "");

    // Special cases
    EXPECT_STREQ(PathAPI->FileName("C:/Users\\John/file.txt"), "file.txt");
    EXPECT_STREQ(PathAPI->FileName("/home/user/archive.tar.gz"), "archive.tar.gz");
    EXPECT_STREQ(PathAPI->FileName("/home/user/Makefile"), "Makefile");
    EXPECT_STREQ(PathAPI->FileName("/home/user/file name.txt"), "file name.txt");
}

TEST_F(PlatformTest, BaseName) {
    // Normal paths
    EXPECT_STREQ(PathAPI->BaseName("C:\\temp\\foo.txt"), "foo");
    EXPECT_STREQ(PathAPI->BaseName("/home/user/file.txt"), "file");

    // Filename only
    EXPECT_STREQ(PathAPI->BaseName("foo.txt"), "foo");
    EXPECT_STREQ(PathAPI->BaseName("document.doc"), "document");

    // No extension
    EXPECT_STREQ(PathAPI->BaseName("C:\\temp\\file"), "file");
    EXPECT_STREQ(PathAPI->BaseName("Makefile"), "Makefile");

    // Multiple extensions
    EXPECT_STREQ(PathAPI->BaseName("/home/user/archive.tar.gz"), "archive");
    EXPECT_STREQ(PathAPI->BaseName("file.backup.old"), "file");

    // Dot files
    EXPECT_STREQ(PathAPI->BaseName(".bashrc"), ".bashrc");
    EXPECT_STREQ(PathAPI->BaseName(".gitignore"), ".gitignore");
    EXPECT_STREQ(PathAPI->BaseName("/home/user/.config"), ".config");

    // Edge cases
    EXPECT_STREQ(PathAPI->BaseName("file."), "file");
    EXPECT_STREQ(PathAPI->BaseName(""), "");
    EXPECT_STREQ(PathAPI->BaseName(NULL), "");
    EXPECT_STREQ(PathAPI->BaseName("C:\\temp\\"), "");
    EXPECT_STREQ(PathAPI->BaseName("/home/user/"), "");
    EXPECT_STREQ(PathAPI->BaseName("/"), "");
    EXPECT_STREQ(PathAPI->BaseName("\\"), "");

    // Mixed separators
    EXPECT_STREQ(PathAPI->BaseName("C:/temp\\foo.txt"), "foo");
}

TEST_F(PlatformTest, Normalize_SeparatorConversion) {
    // Convert backslashes to forward slashes
    EXPECT_STREQ(PathAPI->Normalize("foo\\bar\\tex.png"), "foo/bar/tex.png");
    EXPECT_STREQ(PathAPI->Normalize("C:\\Users\\John\\file.txt"), "c:/users/john/file.txt");
    EXPECT_STREQ(PathAPI->Normalize("a\\b\\c\\d"), "a/b/c/d");
}

TEST_F(PlatformTest, Normalize_CaseConversion) {
    // Convert to lowercase
    EXPECT_STREQ(PathAPI->Normalize("Textures/Diffuse.PNG"), "textures/diffuse.png");
    EXPECT_STREQ(PathAPI->Normalize("UPPERCASE/PATH/FILE.TXT"), "uppercase/path/file.txt");
    EXPECT_STREQ(PathAPI->Normalize("MixedCase.Txt"), "mixedcase.txt");
}

TEST_F(PlatformTest, Normalize_DotResolution) {
    // Resolve single dot (current directory)
    EXPECT_STREQ(PathAPI->Normalize("./textures/tex.png"), "textures/tex.png");
    EXPECT_STREQ(PathAPI->Normalize("textures/./tex.png"), "textures/tex.png");
    EXPECT_STREQ(PathAPI->Normalize("./a/./b/./c.txt"), "a/b/c.txt");

    // Resolve double dot (parent directory)
    EXPECT_STREQ(PathAPI->Normalize("models/../textures/tex.png"), "textures/tex.png");
    EXPECT_STREQ(PathAPI->Normalize("a/b/../c/d"), "a/c/d");
    EXPECT_STREQ(PathAPI->Normalize("a/b/c/../../d"), "a/d");
    EXPECT_STREQ(PathAPI->Normalize("a/b/../b/../b/c"), "a/b/c");
}

TEST_F(PlatformTest, Normalize_MultipleSlashes) {
    // Collapse multiple consecutive slashes
    EXPECT_STREQ(PathAPI->Normalize("foo//bar///tex.png"), "foo/bar/tex.png");
    EXPECT_STREQ(PathAPI->Normalize("a////b"), "a/b");
    EXPECT_STREQ(PathAPI->Normalize("//a//b//"), "a/b");
}

TEST_F(PlatformTest, Normalize_TrailingSlashes) {
    // Remove trailing slashes
    EXPECT_STREQ(PathAPI->Normalize("textures/"), "textures");
    EXPECT_STREQ(PathAPI->Normalize("a/b/c/"), "a/b/c");
    EXPECT_STREQ(PathAPI->Normalize("path///"), "path");
}

TEST_F(PlatformTest, Normalize_AbsolutePaths) {
    // Preserve drive letters but lowercase them
    EXPECT_STREQ(PathAPI->Normalize("C:/Assets/tex.png"), "c:/assets/tex.png");
    EXPECT_STREQ(PathAPI->Normalize("D:\\Game\\Models\\mesh.obj"), "d:/game/models/mesh.obj");
}

TEST_F(PlatformTest, Normalize_EdgeCases) {
    // Empty string
    EXPECT_STREQ(PathAPI->Normalize(""), "");

    // NULL pointer
    EXPECT_STREQ(PathAPI->Normalize(NULL), "");

    // Single file name
    EXPECT_STREQ(PathAPI->Normalize("file.txt"), "file.txt");

    // Just dots
    EXPECT_STREQ(PathAPI->Normalize("."), "");
    EXPECT_STREQ(PathAPI->Normalize(".."), "..");

    // Leading parent refs (can't resolve past root)
    EXPECT_STREQ(PathAPI->Normalize("../a/b"), "../a/b");
    EXPECT_STREQ(PathAPI->Normalize("../../a"), "../../a");
}

TEST_F(PlatformTest, Normalize_ComplexPaths) {
    // Combined normalization
    EXPECT_STREQ(PathAPI->Normalize("C:\\Users\\..\\Shared\\.\\Data\\file.TXT"), "c:/shared/data/file.txt");
    EXPECT_STREQ(PathAPI->Normalize("./Models/../Textures//diffuse.PNG"), "textures/diffuse.png");
    EXPECT_STREQ(PathAPI->Normalize("A\\\\B/./C\\..\\D"), "a/b/d");
}