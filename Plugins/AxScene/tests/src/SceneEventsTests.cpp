#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxLinearAllocator.h"
#include "Foundation/AxAllocatorRegistry.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "AxScene/AxScene.h"
#include "AxOpenGL/AxOpenGL.h"

// Test callback counters and state
static int LightCallbackCount = 0;
static int MaterialCallbackCount = 0;
static int ObjectCallbackCount = 0;
static int TransformCallbackCount = 0;
static int SceneCallbackCount = 0;
static void* LastUserData = nullptr;

// Test callback functions
static void TestLightParsed(const AxLight* Light, void* UserData)
{
    LightCallbackCount++;
    LastUserData = UserData;
}

static void TestMaterialParsed(const AxMaterial* Material, void* UserData)
{
    MaterialCallbackCount++;
    LastUserData = UserData;
}

static void TestObjectParsed(const AxSceneObject* Object, void* UserData)
{
    ObjectCallbackCount++;
    LastUserData = UserData;
}

static void TestTransformParsed(const AxTransform* Transform, void* UserData)
{
    TransformCallbackCount++;
    LastUserData = UserData;
}

static void TestSceneParsed(const AxScene* Scene, void* UserData)
{
    SceneCallbackCount++;
    LastUserData = UserData;
}

// Reset test state
static void ResetTestState()
{
    LightCallbackCount = 0;
    MaterialCallbackCount = 0;
    ObjectCallbackCount = 0;
    TransformCallbackCount = 0;
    SceneCallbackCount = 0;
    LastUserData = nullptr;
}

// Scene Events Tests
class SceneEventsTest : public testing::Test
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

            ResetTestState();
        }

        void TearDown()
        {
            PluginAPI->Unload(ScenePlugin);
        }
};

TEST_F(SceneEventsTest, AxSceneEventsStructureInitialization)
{
    // Test SceneEvents structure initialization with NULL callbacks
    AxSceneEvents Events = {0};

    // All callback pointers should be NULL after zero initialization
    EXPECT_EQ(Events.OnLightParsed, nullptr);
    EXPECT_EQ(Events.OnMaterialParsed, nullptr);
    EXPECT_EQ(Events.OnObjectParsed, nullptr);
    EXPECT_EQ(Events.OnTransformParsed, nullptr);
    EXPECT_EQ(Events.OnSceneParsed, nullptr);
    EXPECT_EQ(Events.UserData, nullptr);
}

TEST_F(SceneEventsTest, AxSceneEventsPartialCallbackImplementation)
{
    // Test SceneEvents with some callbacks NULL, some valid
    AxSceneEvents Events = {0};
    void* TestUserData = (void*)0x12345678;

    // Set only some callbacks
    Events.OnLightParsed = TestLightParsed;
    Events.OnObjectParsed = TestObjectParsed;
    Events.UserData = TestUserData;

    // Verify assigned callbacks
    EXPECT_EQ(Events.OnLightParsed, TestLightParsed);
    EXPECT_EQ(Events.OnObjectParsed, TestObjectParsed);
    EXPECT_EQ(Events.UserData, TestUserData);

    // Verify NULL callbacks remain NULL
    EXPECT_EQ(Events.OnMaterialParsed, nullptr);
    EXPECT_EQ(Events.OnTransformParsed, nullptr);
    EXPECT_EQ(Events.OnSceneParsed, nullptr);
}

TEST_F(SceneEventsTest, AxSceneEventsUserDataPreservation)
{
    // Test UserData field preservation through callback assignment
    AxSceneEvents Events = {0};
    void* TestUserData = (void*)0xDEADBEEF;

    Events.UserData = TestUserData;
    Events.OnLightParsed = TestLightParsed;
    Events.OnObjectParsed = TestObjectParsed;
    Events.OnMaterialParsed = TestMaterialParsed;

    // UserData should remain unchanged after callback assignment
    EXPECT_EQ(Events.UserData, TestUserData);

    // Callbacks should be properly assigned
    EXPECT_EQ(Events.OnLightParsed, TestLightParsed);
    EXPECT_EQ(Events.OnObjectParsed, TestObjectParsed);
    EXPECT_EQ(Events.OnMaterialParsed, TestMaterialParsed);
}

TEST_F(SceneEventsTest, AxSceneEventsCallbackFunctionPointerAssignment)
{
    // Test callback function pointer assignment and retrieval
    AxSceneEvents Events = {0};

    // Assign all callback types to verify structure layout
    Events.OnLightParsed = TestLightParsed;
    Events.OnMaterialParsed = TestMaterialParsed;
    Events.OnObjectParsed = TestObjectParsed;
    Events.OnTransformParsed = TestTransformParsed;
    Events.OnSceneParsed = TestSceneParsed;

    // Verify all assignments
    EXPECT_EQ(Events.OnLightParsed, TestLightParsed);
    EXPECT_EQ(Events.OnMaterialParsed, TestMaterialParsed);
    EXPECT_EQ(Events.OnObjectParsed, TestObjectParsed);
    EXPECT_EQ(Events.OnTransformParsed, TestTransformParsed);
    EXPECT_EQ(Events.OnSceneParsed, TestSceneParsed);
}

TEST_F(SceneEventsTest, EventHandlerRegistrationWithNullEvents)
{
    // Test event handler registration with NULL AxSceneEvents pointer
    AxSceneResult Result = SceneAPI->RegisterEventHandler(nullptr);
    EXPECT_EQ(Result, AX_SCENE_ERROR_INVALID_ARGUMENTS);
}

TEST_F(SceneEventsTest, EventHandlerUnregistrationWithNullEvents)
{
    // Test event handler unregistration with NULL AxSceneEvents pointer
    AxSceneResult Result = SceneAPI->UnregisterEventHandler(nullptr);
    EXPECT_EQ(Result, AX_SCENE_ERROR_INVALID_ARGUMENTS);
}

TEST_F(SceneEventsTest, EventHandlerSingleRegistration)
{
    // Test single event handler registration and unregistration
    AxSceneEvents Events = {0};
    Events.OnLightParsed = TestLightParsed;
    Events.OnObjectParsed = TestObjectParsed;
    Events.UserData = (void*)0x12345678;

    // Registration should succeed
    AxSceneResult Result = SceneAPI->RegisterEventHandler(&Events);
    EXPECT_EQ(Result, AX_SCENE_SUCCESS);

    // Unregistration should succeed
    Result = SceneAPI->UnregisterEventHandler(&Events);
    EXPECT_EQ(Result, AX_SCENE_SUCCESS);
}

TEST_F(SceneEventsTest, EventHandlerUnregisterNonExistent)
{
    // Test unregistration of non-existent handler
    AxSceneEvents Events = {0};
    Events.OnLightParsed = TestLightParsed;

    // Unregistering without registration should return error
    AxSceneResult Result = SceneAPI->UnregisterEventHandler(&Events);
    EXPECT_EQ(Result, AX_SCENE_ERROR_NOT_FOUND);
}

TEST_F(SceneEventsTest, EventHandlerDuplicateRegistration)
{
    // Test duplicate event handler registration
    AxSceneEvents Events = {0};
    Events.OnLightParsed = TestLightParsed;
    Events.UserData = (void*)0x12345678;

    // First registration should succeed
    AxSceneResult Result = SceneAPI->RegisterEventHandler(&Events);
    EXPECT_EQ(Result, AX_SCENE_SUCCESS);

    // Duplicate registration should fail
    Result = SceneAPI->RegisterEventHandler(&Events);
    EXPECT_EQ(Result, AX_SCENE_ERROR_ALREADY_EXISTS);

    // Clean up
    SceneAPI->UnregisterEventHandler(&Events);
}

TEST_F(SceneEventsTest, ClearAllEventHandlers)
{
    // Test clear all event handlers functionality
    AxSceneEvents Events1 = {0};
    AxSceneEvents Events2 = {0};
    Events1.OnLightParsed = TestLightParsed;
    Events2.OnObjectParsed = TestObjectParsed;

    // Register multiple handlers
    SceneAPI->RegisterEventHandler(&Events1);
    SceneAPI->RegisterEventHandler(&Events2);

    // Clear all handlers should not fail
    SceneAPI->ClearEventHandlers();

    // Subsequent unregistration should fail (handlers already cleared)
    AxSceneResult Result = SceneAPI->UnregisterEventHandler(&Events1);
    EXPECT_EQ(Result, AX_SCENE_ERROR_NOT_FOUND);
}