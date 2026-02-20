/**
 * AxComponentFactoryTests.cpp - Tests for Component base and ComponentFactory
 *
 * Tests the ComponentFactory: type registration, component creation,
 * duplicate detection, unregistered lookup, and type iteration.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAPIRegistry.h"
#include "AxEngine/AxComponent.h"
#include "AxEngine/AxComponentFactory.h"

/**
 * A test component type for factory registration tests.
 */
class TestHealthComponent : public Component
{
public:
  float MaxHealth;
  float CurrentHealth;

  TestHealthComponent()
    : MaxHealth(100.0f)
    , CurrentHealth(100.0f)
  {}

  size_t GetSize() const override { return (sizeof(TestHealthComponent)); }
};

/**
 * A second test component type for verifying multiple registrations.
 */
class TestTransformComponent : public Component
{
public:
  float X, Y, Z;

  TestTransformComponent()
    : X(0.0f), Y(0.0f), Z(0.0f)
  {}

  size_t GetSize() const override { return (sizeof(TestTransformComponent)); }
};

// Factory create/destroy functions for TestHealthComponent
static Component* CreateHealthComponent(void* Memory)
{
  return (new (Memory) TestHealthComponent());
}

static void DestroyHealthComponent(Component* Comp)
{
  static_cast<TestHealthComponent*>(Comp)->~TestHealthComponent();
}

// Factory create/destroy functions for TestTransformComponent
static Component* CreateTransformComponent(void* Memory)
{
  return (new (Memory) TestTransformComponent());
}

static void DestroyTransformComponent(Component* Comp)
{
  static_cast<TestTransformComponent*>(Comp)->~TestTransformComponent();
}

/**
 * Test fixture that initializes Foundation APIs and creates a ComponentFactory.
 */
class ComponentFactoryTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr) << "Failed to get HashTableAPI from registry";

    AllocatorAPI_ = static_cast<AxAllocatorAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_ALLOCATOR_API_NAME));
    ASSERT_NE(AllocatorAPI_, nullptr) << "Failed to get AllocatorAPI from registry";

    // Create a heap allocator for test component allocation
    TestAllocator_ = AllocatorAPI_->CreateHeap("TestAllocator", 1024 * 1024, 4 * 1024 * 1024);
    ASSERT_NE(TestAllocator_, nullptr) << "Failed to create test allocator";

    // Create the factory under test
    Factory_ = new ComponentFactory();
  }

  void TearDown() override
  {
    delete Factory_;
    Factory_ = nullptr;

    if (TestAllocator_) {
      TestAllocator_->Destroy(TestAllocator_);
      TestAllocator_ = nullptr;
    }

    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
  AxAllocatorAPI* AllocatorAPI_{nullptr};
  AxAllocator* TestAllocator_{nullptr};
  ComponentFactory* Factory_{nullptr};
};

//=============================================================================
// Test 1: Register a component type by name and retrieve its factory function
//=============================================================================
TEST_F(ComponentFactoryTest, RegisterTypeAndRetrieve)
{
  bool Result = Factory_->RegisterType(
    "HealthComponent",
    CreateHealthComponent,
    DestroyHealthComponent,
    sizeof(TestHealthComponent));

  EXPECT_TRUE(Result);

  // Verify the type can be found by name
  const ComponentTypeInfo* Info = Factory_->FindType("HealthComponent");
  ASSERT_NE(Info, nullptr);
  EXPECT_EQ(Info->TypeName, "HealthComponent");
  EXPECT_EQ(Info->CreateFunc, CreateHealthComponent);
  EXPECT_EQ(Info->DestroyFunc, DestroyHealthComponent);
  EXPECT_EQ(Info->DataSize, sizeof(TestHealthComponent));
  EXPECT_GT(Info->TypeID, 0u);
}

//=============================================================================
// Test 2: Create a component instance via factory returns correct TypeID
//=============================================================================
TEST_F(ComponentFactoryTest, CreateComponentReturnsCorrectTypeID)
{
  Factory_->RegisterType(
    "HealthComponent",
    CreateHealthComponent,
    DestroyHealthComponent,
    sizeof(TestHealthComponent));

  // Get the expected TypeID
  const ComponentTypeInfo* Info = Factory_->FindType("HealthComponent");
  ASSERT_NE(Info, nullptr);
  uint32_t ExpectedTypeID = Info->TypeID;

  // Create an instance via factory
  Component* Comp = Factory_->CreateComponent("HealthComponent", TestAllocator_);
  ASSERT_NE(Comp, nullptr);

  // Verify TypeID and TypeName are set correctly
  EXPECT_EQ(Comp->GetTypeID(), ExpectedTypeID);
  EXPECT_EQ(Comp->GetTypeName(), "HealthComponent");

  // Verify the component is actually a TestHealthComponent with correct defaults
  TestHealthComponent* Health = static_cast<TestHealthComponent*>(Comp);
  EXPECT_FLOAT_EQ(Health->MaxHealth, 100.0f);
  EXPECT_FLOAT_EQ(Health->CurrentHealth, 100.0f);

  // Clean up
  Factory_->DestroyComponent(Comp, TestAllocator_);
}

//=============================================================================
// Test 3: Registering duplicate type name returns error
//=============================================================================
TEST_F(ComponentFactoryTest, RegisterDuplicateTypeReturnsError)
{
  bool FirstResult = Factory_->RegisterType(
    "HealthComponent",
    CreateHealthComponent,
    DestroyHealthComponent,
    sizeof(TestHealthComponent));

  EXPECT_TRUE(FirstResult);

  // Attempting to register the same name again should fail
  bool SecondResult = Factory_->RegisterType(
    "HealthComponent",
    CreateHealthComponent,
    DestroyHealthComponent,
    sizeof(TestHealthComponent));

  EXPECT_FALSE(SecondResult);

  // Only one type should be registered
  EXPECT_EQ(Factory_->GetTypeCount(), 1u);
}

//=============================================================================
// Test 4: Looking up unregistered type returns nullptr
//=============================================================================
TEST_F(ComponentFactoryTest, LookupUnregisteredTypeReturnsNullptr)
{
  // Factory is empty - looking up any type should return nullptr
  const ComponentTypeInfo* Info = Factory_->FindType("NonexistentComponent");
  EXPECT_EQ(Info, nullptr);

  // Creating an unregistered type should also return nullptr
  Component* Comp = Factory_->CreateComponent("NonexistentComponent", TestAllocator_);
  EXPECT_EQ(Comp, nullptr);
}

//=============================================================================
// Test 5: Iterating registered component types
//=============================================================================
TEST_F(ComponentFactoryTest, IterateRegisteredTypes)
{
  // Register two types
  Factory_->RegisterType(
    "HealthComponent",
    CreateHealthComponent,
    DestroyHealthComponent,
    sizeof(TestHealthComponent));

  Factory_->RegisterType(
    "TransformComponent",
    CreateTransformComponent,
    DestroyTransformComponent,
    sizeof(TestTransformComponent));

  // Verify count
  EXPECT_EQ(Factory_->GetTypeCount(), 2u);

  // Iterate and verify both types are present
  bool FoundHealth = false;
  bool FoundTransform = false;

  for (uint32_t i = 0; i < Factory_->GetTypeCount(); ++i) {
    const ComponentTypeInfo* Info = Factory_->GetTypeAtIndex(i);
    ASSERT_NE(Info, nullptr);

    if (Info->TypeName == "HealthComponent") {
      FoundHealth = true;
      EXPECT_EQ(Info->DataSize, sizeof(TestHealthComponent));
    } else if (Info->TypeName == "TransformComponent") {
      FoundTransform = true;
      EXPECT_EQ(Info->DataSize, sizeof(TestTransformComponent));
    }
  }

  EXPECT_TRUE(FoundHealth) << "HealthComponent not found during iteration";
  EXPECT_TRUE(FoundTransform) << "TransformComponent not found during iteration";

  // TypeIDs should be different
  const ComponentTypeInfo* HealthInfo = Factory_->FindType("HealthComponent");
  const ComponentTypeInfo* TransformInfo = Factory_->FindType("TransformComponent");
  ASSERT_NE(HealthInfo, nullptr);
  ASSERT_NE(TransformInfo, nullptr);
  EXPECT_NE(HealthInfo->TypeID, TransformInfo->TypeID);

  // Out-of-range index should return nullptr
  const ComponentTypeInfo* OutOfRange = Factory_->GetTypeAtIndex(999);
  EXPECT_EQ(OutOfRange, nullptr);
}
