#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "AxScene/AxScene.h"

// Scene Data Structure Tests
class SceneDataStructureTest : public testing::Test
{
    protected:
        AxSceneAPI *SceneAPI;
        AxPluginAPI *PluginAPI;

        uint64_t ScenePlugin;

        void SetUp()
        {
            AxonInitGlobalAPIRegistry();
            AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

            // Initialize PluginAPI from the global registry
            PluginAPI = (AxPluginAPI *)AxonGlobalAPIRegistry->Get(AXON_PLUGIN_API_NAME);
            ASSERT_NE(PluginAPI, nullptr);

            ScenePlugin = PluginAPI->Load("libAxScene.dll", false);
            SceneAPI = (AxSceneAPI *)AxonGlobalAPIRegistry->Get(AXON_SCENE_API_NAME);
            ASSERT_NE(SceneAPI, nullptr);
        }

        void TearDown()
        {
            PluginAPI->Unload(ScenePlugin);
        }
};

TEST_F(SceneDataStructureTest, AxSceneObjectBasicCreation)
{
    // Test basic scene object creation and initialization
    AxScene* Scene = SceneAPI->CreateScene("TestScene", Kilobytes(64));
    EXPECT_NE(Scene, nullptr);

    AxSceneObject* Object = SceneAPI->CreateObject(Scene, "TestObject", nullptr);
    EXPECT_NE(Object, nullptr);
    EXPECT_STREQ(Object->Name, "TestObject");
    EXPECT_EQ(Object->ObjectID, 1); // First object should have ID 1
    EXPECT_EQ(Object->Parent, nullptr);
    EXPECT_EQ(Object->FirstChild, nullptr);
    EXPECT_EQ(Object->NextSibling, nullptr);

    // Verify scene state
    EXPECT_EQ(Scene->ObjectCount, 1);
    EXPECT_EQ(Scene->RootObject, Object);

    SceneAPI->DestroyScene(Scene);
}

TEST_F(SceneDataStructureTest, AxSceneObjectHierarchy)
{
    // Test scene object parent-child relationships
    AxScene* Scene = SceneAPI->CreateScene("HierarchyTest", Kilobytes(64));
    EXPECT_NE(Scene, nullptr);

    // Create parent object
    AxSceneObject* Parent = SceneAPI->CreateObject(Scene, "Parent", nullptr);
    EXPECT_NE(Parent, nullptr);
    EXPECT_EQ(Parent->ObjectID, 1);

    // Create child objects
    AxSceneObject* Child1 = SceneAPI->CreateObject(Scene, "Child1", Parent);
    AxSceneObject* Child2 = SceneAPI->CreateObject(Scene, "Child2", Parent);

    EXPECT_NE(Child1, nullptr);
    EXPECT_NE(Child2, nullptr);
    EXPECT_EQ(Child1->ObjectID, 2);
    EXPECT_EQ(Child2->ObjectID, 3);

    // Verify hierarchy relationships
    EXPECT_EQ(Child1->Parent, Parent);
    EXPECT_EQ(Child2->Parent, Parent);
    EXPECT_EQ(Parent->FirstChild, Child1);
    EXPECT_EQ(Child1->NextSibling, Child2);
    EXPECT_EQ(Child2->NextSibling, nullptr);

    // Verify scene state
    EXPECT_EQ(Scene->ObjectCount, 3);
    EXPECT_EQ(Scene->RootObject, Parent);

    SceneAPI->DestroyScene(Scene);
}

TEST_F(SceneDataStructureTest, AxSceneObjectTransform)
{
    // Test scene object transform integration
    AxScene* Scene = SceneAPI->CreateScene("TransformTest", Kilobytes(64));
    EXPECT_NE(Scene, nullptr);

    AxSceneObject* Object = SceneAPI->CreateObject(Scene, "TransformObject", nullptr);
    EXPECT_NE(Object, nullptr);

    // Test setting transform
    AxTransform NewTransform = {0};
    NewTransform.Translation = (AxVec3){1.0f, 2.0f, 3.0f};
    NewTransform.Rotation = (AxQuat){0.0f, 0.0f, 0.0f, 1.0f};
    NewTransform.Scale = (AxVec3){2.0f, 2.0f, 2.0f};

    SceneAPI->SetObjectTransform(Object, &NewTransform);

    // Verify transform was set correctly
    EXPECT_EQ(Object->Transform.Translation.X, 1.0f);
    EXPECT_EQ(Object->Transform.Translation.Y, 2.0f);
    EXPECT_EQ(Object->Transform.Translation.Z, 3.0f);
    EXPECT_EQ(Object->Transform.Scale.X, 2.0f);
    EXPECT_EQ(Object->Transform.Scale.Y, 2.0f);
    EXPECT_EQ(Object->Transform.Scale.Z, 2.0f);

    SceneAPI->DestroyScene(Scene);
}

TEST_F(SceneDataStructureTest, AxSceneBasicCreation)
{
    // Test basic scene container creation
    AxScene* Scene = SceneAPI->CreateScene("BasicScene", Kilobytes(64));
    EXPECT_NE(Scene, nullptr);
    EXPECT_STREQ(Scene->Name, "BasicScene");
    EXPECT_EQ(Scene->ObjectCount, 0);
    EXPECT_EQ(Scene->NextObjectID, 1);
    EXPECT_EQ(Scene->RootObject, nullptr);
    EXPECT_NE(Scene->Allocator, nullptr);

    SceneAPI->DestroyScene(Scene);
}

TEST_F(SceneDataStructureTest, SceneAssetPaths)
{
    // Test setting mesh and material paths
    AxScene* Scene = SceneAPI->CreateScene("AssetTest", Kilobytes(64));
    EXPECT_NE(Scene, nullptr);

    AxSceneObject* Object = SceneAPI->CreateObject(Scene, "AssetObject", nullptr);
    EXPECT_NE(Object, nullptr);

    // Test setting mesh path
    SceneAPI->SetObjectMesh(Object, "assets/models/cube.mesh");
    EXPECT_STREQ(Object->MeshPath, "assets/models/cube.mesh");

    // Test setting material path
    SceneAPI->SetObjectMaterial(Object, "assets/materials/default.mat");
    EXPECT_STREQ(Object->MaterialPath, "assets/materials/default.mat");

    SceneAPI->DestroyScene(Scene);
}

TEST_F(SceneDataStructureTest, SceneFindObject)
{
    // Test finding objects by name
    AxScene* Scene = SceneAPI->CreateScene("FindTest", Kilobytes(64));
    EXPECT_NE(Scene, nullptr);

    AxSceneObject* Root = SceneAPI->CreateObject(Scene, "Root", nullptr);
    AxSceneObject* Child1 = SceneAPI->CreateObject(Scene, "Child1", Root);
    AxSceneObject* Child2 = SceneAPI->CreateObject(Scene, "Child2", Root);

    // Test finding existing objects
    AxSceneObject* FoundRoot = SceneAPI->FindObject(Scene, "Root");
    AxSceneObject* FoundChild1 = SceneAPI->FindObject(Scene, "Child1");
    AxSceneObject* FoundChild2 = SceneAPI->FindObject(Scene, "Child2");

    EXPECT_EQ(FoundRoot, Root);
    EXPECT_EQ(FoundChild1, Child1);
    EXPECT_EQ(FoundChild2, Child2);

    // Test finding non-existent object
    AxSceneObject* NotFound = SceneAPI->FindObject(Scene, "NonExistent");
    EXPECT_EQ(NotFound, nullptr);

    SceneAPI->DestroyScene(Scene);
}
