#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "AxScene/AxScene.h"

#include <string.h>
#include <stdio.h>

// Scene File Parser Tests
class SceneParserTest : public testing::Test
{
    protected:
        struct AxAllocator* TestAllocator;
        AxSceneAPI *SceneAPI;
        AxPluginAPI *PluginAPI;
        struct AxAllocatorAPI *AllocatorAPI;

        void SetUp()
        {
            AxonInitGlobalAPIRegistry();
            AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

            // Initialize PluginAPI from the global registry
            PluginAPI = (AxPluginAPI *)AxonGlobalAPIRegistry->Get(AXON_PLUGIN_API_NAME);
            ASSERT_NE(PluginAPI, nullptr);

            // Initialize AllocatorAPI from the global registry
            AllocatorAPI = (struct AxAllocatorAPI *)AxonGlobalAPIRegistry->Get(AXON_ALLOCATOR_API_NAME);
            ASSERT_NE(AllocatorAPI, nullptr);

            PluginAPI->Load("libAxScene.dll", false);

            SceneAPI = (AxSceneAPI *)AxonGlobalAPIRegistry->Get(AXON_SCENE_API_NAME);
            ASSERT_NE(SceneAPI, nullptr);

            TestAllocator = AllocatorAPI->CreateLinear("SceneParserTest", Megabytes(1));
        }

        void TearDown()
        {
            TestAllocator->Destroy(TestAllocator);
        }
};

TEST_F(SceneParserTest, ParseValidSceneFile)
{
    // Test parsing a valid .ats scene file
    const char* SceneContent =
        "# Example scene file\n"
        "scene \"TestScene\" {\n"
        "    object \"RootObject\" {\n"
        "        transform {\n"
        "            translation: 1.0, 2.0, 3.0\n"
        "            rotation: 0.0, 0.0, 0.0, 1.0\n"
        "            scale: 1.0, 1.0, 1.0\n"
        "        }\n"
        "        mesh: \"assets/models/cube.mesh\"\n"
        "        material: \"assets/materials/default.mat\"\n"
        "    }\n"
        "}\n";

    AxScene* Scene = SceneAPI->ParseFromString(SceneContent, TestAllocator);
    EXPECT_NE(Scene, nullptr);
    EXPECT_STREQ(Scene->Name, "TestScene");
    EXPECT_EQ(Scene->ObjectCount, 1);
    EXPECT_NE(Scene->RootObject, nullptr);

    // Verify root object properties
    AxSceneObject* Root = Scene->RootObject;
    EXPECT_STREQ(Root->Name, "RootObject");
    EXPECT_EQ(Root->Transform.Translation.X, 1.0f);
    EXPECT_EQ(Root->Transform.Translation.Y, 2.0f);
    EXPECT_EQ(Root->Transform.Translation.Z, 3.0f);
    EXPECT_STREQ(Root->MeshPath, "assets/models/cube.mesh");
    EXPECT_STREQ(Root->MaterialPath, "assets/materials/default.mat");

    SceneAPI->DestroyScene(Scene);
}

TEST_F(SceneParserTest, ParseSceneWithHierarchy)
{
    // Test parsing scene with parent-child relationships
    const char* SceneContent =
        "scene \"HierarchyTest\" {\n"
        "    object \"Parent\" {\n"
        "        transform {\n"
        "            translation: 0.0, 0.0, 0.0\n"
        "            rotation: 0.0, 0.0, 0.0, 1.0\n"
        "            scale: 1.0, 1.0, 1.0\n"
        "        }\n"
        "        mesh: \"parent.mesh\"\n"
        "        \n"
        "        object \"Child1\" {\n"
        "            transform {\n"
        "                translation: 1.0, 0.0, 0.0\n"
        "                rotation: 0.0, 0.0, 0.0, 1.0\n"
        "                scale: 0.5, 0.5, 0.5\n"
        "            }\n"
        "            mesh: \"child1.mesh\"\n"
        "        }\n"
        "        \n"
        "        object \"Child2\" {\n"
        "            transform {\n"
        "                translation: -1.0, 0.0, 0.0\n"
        "                rotation: 0.0, 0.0, 0.0, 1.0\n"
        "                scale: 0.5, 0.5, 0.5\n"
        "            }\n"
        "            mesh: \"child2.mesh\"\n"
        "        }\n"
        "    }\n"
        "}\n";

    AxScene* Scene = SceneAPI->ParseFromString(SceneContent, TestAllocator);
    if (!Scene) {
        const char* Error = SceneAPI->GetLastError();
        printf("Parse error: %s\n", Error ? Error : "Unknown error");
    }
    EXPECT_NE(Scene, nullptr);
    EXPECT_EQ(Scene->ObjectCount, 3);

    AxSceneObject* Parent = Scene->RootObject;
    EXPECT_STREQ(Parent->Name, "Parent");
    EXPECT_NE(Parent->FirstChild, nullptr);

    AxSceneObject* Child1 = Parent->FirstChild;
    EXPECT_STREQ(Child1->Name, "Child1");
    EXPECT_EQ(Child1->Parent, Parent);
    EXPECT_NE(Child1->NextSibling, nullptr);

    AxSceneObject* Child2 = Child1->NextSibling;
    EXPECT_STREQ(Child2->Name, "Child2");
    EXPECT_EQ(Child2->Parent, Parent);
    EXPECT_EQ(Child2->NextSibling, nullptr);

    SceneAPI->DestroyScene(Scene);
}

TEST_F(SceneParserTest, ParseWithComments)
{
    // Test parsing scene with comments
    const char* SceneContent =
        "# This is a comment\n"
        "scene \"CommentTest\" {\n"
        "    # Another comment\n"
        "    object \"TestObject\" {\n"
        "        transform {\n"
        "            translation: 0.0, 0.0, 0.0  # Inline comment\n"
        "            rotation: 0.0, 0.0, 0.0, 1.0\n"
        "            scale: 1.0, 1.0, 1.0\n"
        "        }\n"
        "        # Comment before mesh\n"
        "        mesh: \"test.mesh\"\n"
        "    }\n"
        "}\n";

    AxScene* Scene = SceneAPI->ParseFromString(SceneContent, TestAllocator);
    EXPECT_NE(Scene, nullptr);
    EXPECT_STREQ(Scene->Name, "CommentTest");
    EXPECT_EQ(Scene->ObjectCount, 1);

    SceneAPI->DestroyScene(Scene);
}

TEST_F(SceneParserTest, ParseInvalidSyntax)
{
    // Test parsing scene with syntax errors
    const char* InvalidScenes[] = {
        // Missing closing brace
        "scene \"Invalid\" {\n    object \"Test\" {\n",

        // Invalid transform values
        "scene \"Invalid\" {\n    object \"Test\" {\n        transform {\n            translation: invalid, 0.0, 0.0\n        }\n    }\n}\n",

        // Missing scene name
        "scene {\n    object \"Test\" {\n    }\n}\n",

        // Invalid object structure
        "scene \"Invalid\" {\n    invalid_keyword \"Test\" {\n    }\n}\n"
    };

    for (int i = 0; i < 4; i++)
    {
        AxScene* Scene = SceneAPI->ParseFromString(InvalidScenes[i], TestAllocator);
        EXPECT_EQ(Scene, nullptr) << "Invalid scene " << i << " should have failed to parse";
    }
}

TEST_F(SceneParserTest, ParseEmptyScene)
{
    // Test parsing empty scene
    const char* SceneContent =
        "scene \"EmptyScene\" {\n"
        "}\n";

    AxScene* Scene = SceneAPI->ParseFromString(SceneContent, TestAllocator);
    EXPECT_NE(Scene, nullptr);
    EXPECT_STREQ(Scene->Name, "EmptyScene");
    EXPECT_EQ(Scene->ObjectCount, 0);
    EXPECT_EQ(Scene->RootObject, nullptr);

    SceneAPI->DestroyScene(Scene);
}

TEST_F(SceneParserTest, ParseFromFile)
{
    // Test parsing from file path
    // Note: This test would require creating a temporary file
    // For now, test that the function exists and handles null path gracefully
    AxScene* Scene = SceneAPI->ParseFromFile(nullptr, TestAllocator);
    EXPECT_EQ(Scene, nullptr);

    Scene = SceneAPI->ParseFromFile("nonexistent.ats", TestAllocator);
    EXPECT_EQ(Scene, nullptr);
}

TEST_F(SceneParserTest, ParseOptionalFields)
{
    // Test parsing objects without mesh or material paths
    const char* SceneContent =
        "scene \"OptionalTest\" {\n"
        "    object \"NoAssets\" {\n"
        "        transform {\n"
        "            translation: 0.0, 0.0, 0.0\n"
        "            rotation: 0.0, 0.0, 0.0, 1.0\n"
        "            scale: 1.0, 1.0, 1.0\n"
        "        }\n"
        "    }\n"
        "    object \"OnlyMesh\" {\n"
        "        transform {\n"
        "            translation: 1.0, 0.0, 0.0\n"
        "            rotation: 0.0, 0.0, 0.0, 1.0\n"
        "            scale: 1.0, 1.0, 1.0\n"
        "        }\n"
        "        mesh: \"test.mesh\"\n"
        "    }\n"
        "}\n";

    AxScene* Scene = SceneAPI->ParseFromString(SceneContent, TestAllocator);
    EXPECT_NE(Scene, nullptr);
    EXPECT_EQ(Scene->ObjectCount, 2);

    AxSceneObject* NoAssets = Scene->RootObject;
    EXPECT_STREQ(NoAssets->Name, "NoAssets");
    EXPECT_EQ(NoAssets->MeshPath[0], '\0');  // Empty string
    EXPECT_EQ(NoAssets->MaterialPath[0], '\0');  // Empty string

    AxSceneObject* OnlyMesh = NoAssets->NextSibling;
    EXPECT_STREQ(OnlyMesh->Name, "OnlyMesh");
    EXPECT_STREQ(OnlyMesh->MeshPath, "test.mesh");
    EXPECT_EQ(OnlyMesh->MaterialPath[0], '\0');  // Empty string

    SceneAPI->DestroyScene(Scene);
}
