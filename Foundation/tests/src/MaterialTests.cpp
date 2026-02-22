#include "gtest/gtest.h"
#include "AxEngine/AxRenderTypes.h"

// Test AxPBRMaterial creation with default values
TEST(MaterialTests, PBRMaterialDefaultCreation)
{
    AxPBRMaterial material = {};

    // Verify default color factors are zero-initialized
    EXPECT_EQ(material.BaseColorFactor.X, 0.0f);
    EXPECT_EQ(material.BaseColorFactor.Y, 0.0f);
    EXPECT_EQ(material.BaseColorFactor.Z, 0.0f);
    EXPECT_EQ(material.BaseColorFactor.W, 0.0f);

    EXPECT_EQ(material.EmissiveFactor.X, 0.0f);
    EXPECT_EQ(material.EmissiveFactor.Y, 0.0f);
    EXPECT_EQ(material.EmissiveFactor.Z, 0.0f);

    // Verify default PBR factors
    EXPECT_EQ(material.MetallicFactor, 0.0f);
    EXPECT_EQ(material.RoughnessFactor, 0.0f);

    // Verify texture indices are zero-initialized
    EXPECT_EQ(material.BaseColorTexture, 0);
    EXPECT_EQ(material.MetallicRoughnessTexture, 0);
    EXPECT_EQ(material.NormalTexture, 0);
    EXPECT_EQ(material.EmissiveTexture, 0);
    EXPECT_EQ(material.OcclusionTexture, 0);

    // Verify alpha handling defaults
    EXPECT_EQ(material.AlphaMode, AX_ALPHA_MODE_OPAQUE);
    EXPECT_EQ(material.AlphaCutoff, 0.0f);

    // Verify double-sided flag
    EXPECT_EQ(material.DoubleSided, false);
}

// Test AxPBRMaterial with full texture set
TEST(MaterialTests, PBRMaterialWithTextures)
{
    AxPBRMaterial material = {};
    material.BaseColorFactor = (AxVec4){ 1.0f, 0.5f, 0.25f, 1.0f };
    material.EmissiveFactor = (AxVec3){ 0.1f, 0.2f, 0.3f };
    material.MetallicFactor = 0.8f;
    material.RoughnessFactor = 0.6f;
    material.BaseColorTexture = 0;
    material.MetallicRoughnessTexture = 1;
    material.NormalTexture = 2;
    material.EmissiveTexture = 3;
    material.OcclusionTexture = 4;
    material.AlphaMode = AX_ALPHA_MODE_BLEND;
    material.AlphaCutoff = 0.5f;
    material.DoubleSided = true;

    // Verify all properties are set correctly
    EXPECT_EQ(material.BaseColorFactor.X, 1.0f);
    EXPECT_EQ(material.BaseColorFactor.Y, 0.5f);
    EXPECT_EQ(material.BaseColorFactor.Z, 0.25f);
    EXPECT_EQ(material.BaseColorFactor.W, 1.0f);

    EXPECT_EQ(material.EmissiveFactor.X, 0.1f);
    EXPECT_EQ(material.EmissiveFactor.Y, 0.2f);
    EXPECT_EQ(material.EmissiveFactor.Z, 0.3f);

    EXPECT_EQ(material.MetallicFactor, 0.8f);
    EXPECT_EQ(material.RoughnessFactor, 0.6f);

    EXPECT_EQ(material.BaseColorTexture, 0);
    EXPECT_EQ(material.MetallicRoughnessTexture, 1);
    EXPECT_EQ(material.NormalTexture, 2);
    EXPECT_EQ(material.EmissiveTexture, 3);
    EXPECT_EQ(material.OcclusionTexture, 4);

    EXPECT_EQ(material.AlphaMode, AX_ALPHA_MODE_BLEND);
    EXPECT_EQ(material.AlphaCutoff, 0.5f);
    EXPECT_EQ(material.DoubleSided, true);
}

// Test texture index validation (negative values for unused textures)
TEST(MaterialTests, PBRMaterialTextureIndexValidation)
{
    AxPBRMaterial material = {};
    material.BaseColorTexture = -1;
    material.MetallicRoughnessTexture = -1;
    material.NormalTexture = -1;
    material.EmissiveTexture = -1;
    material.OcclusionTexture = -1;

    // Verify negative indices are stored correctly (indicating no texture)
    EXPECT_EQ(material.BaseColorTexture, -1);
    EXPECT_EQ(material.MetallicRoughnessTexture, -1);
    EXPECT_EQ(material.NormalTexture, -1);
    EXPECT_EQ(material.EmissiveTexture, -1);
    EXPECT_EQ(material.OcclusionTexture, -1);
}

// Test alpha mode and cutoff values
TEST(MaterialTests, PBRMaterialAlphaModes)
{
    AxPBRMaterial opaque = {};
    opaque.AlphaMode = AX_ALPHA_MODE_OPAQUE;
    EXPECT_EQ(opaque.AlphaMode, AX_ALPHA_MODE_OPAQUE);

    AxPBRMaterial mask = {};
    mask.AlphaMode = AX_ALPHA_MODE_MASK;
    mask.AlphaCutoff = 0.5f;
    EXPECT_EQ(mask.AlphaMode, AX_ALPHA_MODE_MASK);
    EXPECT_EQ(mask.AlphaCutoff, 0.5f);

    AxPBRMaterial blend = {};
    blend.AlphaMode = AX_ALPHA_MODE_BLEND;
    EXPECT_EQ(blend.AlphaMode, AX_ALPHA_MODE_BLEND);
}

// Test AxCustomMaterial structure
TEST(MaterialTests, CustomMaterialCreation)
{
    AxCustomMaterial material = {};

    // Set shader paths
    const char* vertexPath = "shaders/custom.vert";
    const char* fragmentPath = "shaders/custom.frag";

    strncpy(material.VertexShaderPath, vertexPath, sizeof(material.VertexShaderPath) - 1);
    strncpy(material.FragmentShaderPath, fragmentPath, sizeof(material.FragmentShaderPath) - 1);

    // Verify paths are set correctly
    EXPECT_STREQ(material.VertexShaderPath, vertexPath);
    EXPECT_STREQ(material.FragmentShaderPath, fragmentPath);

    // Verify custom data is initially null
    EXPECT_EQ(material.CustomData, nullptr);
    EXPECT_EQ(material.CustomDataSize, 0);
}

// Test AxMaterialDesc with PBR type
TEST(MaterialTests, MaterialDescPBRType)
{
    AxMaterialDesc desc = {};
    strncpy(desc.Name, "TestMaterial", sizeof(desc.Name) - 1);
    desc.Type = AX_MATERIAL_TYPE_PBR;
    desc.PBR.BaseColorFactor = (AxVec4){ 1.0f, 1.0f, 1.0f, 1.0f };
    desc.PBR.MetallicFactor = 0.5f;
    desc.PBR.RoughnessFactor = 0.5f;

    // Verify material description properties
    EXPECT_STREQ(desc.Name, "TestMaterial");
    EXPECT_EQ(desc.Type, AX_MATERIAL_TYPE_PBR);
    EXPECT_EQ(desc.PBR.BaseColorFactor.X, 1.0f);
    EXPECT_EQ(desc.PBR.MetallicFactor, 0.5f);
    EXPECT_EQ(desc.PBR.RoughnessFactor, 0.5f);
}

// Test AxMaterialDesc with Custom type
TEST(MaterialTests, MaterialDescCustomType)
{
    AxMaterialDesc desc = {};
    strncpy(desc.Name, "CustomShaderMaterial", sizeof(desc.Name) - 1);
    desc.Type = AX_MATERIAL_TYPE_CUSTOM;
    strncpy(desc.Custom.VertexShaderPath, "custom.vert", sizeof(desc.Custom.VertexShaderPath) - 1);
    strncpy(desc.Custom.FragmentShaderPath, "custom.frag", sizeof(desc.Custom.FragmentShaderPath) - 1);

    // Verify material description properties
    EXPECT_STREQ(desc.Name, "CustomShaderMaterial");
    EXPECT_EQ(desc.Type, AX_MATERIAL_TYPE_CUSTOM);
    EXPECT_STREQ(desc.Custom.VertexShaderPath, "custom.vert");
    EXPECT_STREQ(desc.Custom.FragmentShaderPath, "custom.frag");
}

// Test AxMaterialDesc type discriminator
TEST(MaterialTests, MaterialDescTypeDiscriminator)
{
    AxMaterialDesc pbrMaterial = {};
    pbrMaterial.Type = AX_MATERIAL_TYPE_PBR;

    AxMaterialDesc customMaterial = {};
    customMaterial.Type = AX_MATERIAL_TYPE_CUSTOM;

    // Verify type discriminator works correctly
    EXPECT_EQ(pbrMaterial.Type, AX_MATERIAL_TYPE_PBR);
    EXPECT_EQ(customMaterial.Type, AX_MATERIAL_TYPE_CUSTOM);
    EXPECT_NE(pbrMaterial.Type, customMaterial.Type);
}
