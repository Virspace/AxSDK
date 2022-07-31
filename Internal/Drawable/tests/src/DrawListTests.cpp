#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"

extern "C"
{
    #include "Drawable/AxDrawList.h"
    #include "Drawable/AxDrawCommand.h"
}

#include "Foundation/AxArray.h"

class DrawListTest : public testing::Test
{
public:
    AxDrawList DrawList = {0};

protected:
    AxDrawVert *VertexBuffer = NULL;
    AxDrawIndex *IndexBuffer = NULL;

    void SetUp()
    {
        AxVert Vertices[] = {
            { -1, -1, 0 },
            { -1, 1, 0 },
            { 1, 1, 0 },
            { 1, -1, 0 }
        };

        for (int i = 0; i < 4; i++)
        {
            AxDrawVert Vertex;
            Vertex.Position = Vertices[i];
            //Vertex.UV = {0, 1};
            //Vertex.Color = 0;
            ArrayPush(VertexBuffer, Vertex);
        }

        ArrayPush(IndexBuffer, 0);
        ArrayPush(IndexBuffer, 1);
        ArrayPush(IndexBuffer, 2);
        ArrayPush(IndexBuffer, 0);
        ArrayPush(IndexBuffer, 2);
        ArrayPush(IndexBuffer, 3);

        DrawListAddDrawable(&DrawList, VertexBuffer, IndexBuffer, 0, 0);
        DrawListAddDrawable(&DrawList, VertexBuffer, IndexBuffer, 0, 0);
        DrawListAddDrawable(&DrawList, VertexBuffer, IndexBuffer, 0, 0);
    }

    void TearDown()
    {
        ArrayFree(DrawList.CommandBuffer);
        ArrayFree(DrawList.VertexBuffer);
        ArrayFree(DrawList.IndexBuffer);
    }
};

TEST_F(DrawListTest, DrawListBufferSizes)
{
    EXPECT_EQ(ArraySize(DrawList.CommandBuffer), 3);
    EXPECT_EQ(ArraySize(DrawList.VertexBuffer), 12);
    EXPECT_EQ(ArraySize(DrawList.IndexBuffer), 18);
}

TEST_F(DrawListTest, DrawCommandOffsets)
{
    AxDrawCommand *CommandBuffer = NULL;

    CommandBuffer = &DrawList.CommandBuffer[0];
    ASSERT_NE(CommandBuffer, nullptr);
    EXPECT_EQ(CommandBuffer->VertexOffset, 0);
    EXPECT_EQ(CommandBuffer->IndexOffset, 0);
    EXPECT_EQ(CommandBuffer->ElementCount, 6);

    CommandBuffer = &DrawList.CommandBuffer[1];
    ASSERT_NE(CommandBuffer, nullptr);
    EXPECT_EQ(CommandBuffer->VertexOffset, 4);
    EXPECT_EQ(CommandBuffer->IndexOffset, 6);
    EXPECT_EQ(CommandBuffer->ElementCount, 6);

    CommandBuffer = &DrawList.CommandBuffer[2];
    ASSERT_NE(CommandBuffer, nullptr);
    EXPECT_EQ(CommandBuffer->VertexOffset, 8);
    EXPECT_EQ(CommandBuffer->IndexOffset, 12);
    EXPECT_EQ(CommandBuffer->ElementCount, 6);
}