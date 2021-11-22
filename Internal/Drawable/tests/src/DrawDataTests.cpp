#include "gtest/gtest.h"
#include "Foundation/Types.h"

extern "C"
{
    #include "Drawable/AxDrawData.h"
    #include "Drawable/AxDrawList.h"
}

#define AXARRAY_IMPLEMENTATION
#include "Foundation/AxArray.h"

class DrawDataTest : public testing::Test
{
public:
    AxDrawList *DrawLists;
    AxDrawData DrawData;

protected:

    void SetUp()
    {
        DrawData = { 0 };
        DrawLists = NULL;

        for (int i = 0; i < 3; i++)
        {
            AxDrawVert *VertexBuffer = NULL;
            AxDrawIndex *IndexBuffer = NULL;

            AxVert Vertices[] = {
                { -1, -1, 0 },
                { -1, 1, 0 },
                { 1, 1, 0 },
                { 1, -1, 0 }
            };

            for (int j = 0; j < 4; j++)
            {
                AxDrawVert Vertex;
                Vertex.Position = Vertices[j];
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

            AxDrawList DrawList = { 0 };
            DrawListAddDrawable(&DrawList, VertexBuffer, IndexBuffer, 0, 0);
            DrawListAddDrawable(&DrawList, VertexBuffer, IndexBuffer, 0, 0);
            DrawListAddDrawable(&DrawList, VertexBuffer, IndexBuffer, 0, 0);

            ArrayPush(DrawLists, DrawList);
            ArrayFree(VertexBuffer);
            ArrayFree(IndexBuffer);
        }

        for (int i = 0; i < 3; i++) {
            DrawDataAddDrawList(&DrawData, &DrawLists[i]);
        }
    }

    void TearDown()
    {
        for (int i = 0; i < 3; i++) {
            DrawListClear(&DrawLists[i]);
        }

        DrawDataClear(&DrawData);
        ArrayFree(DrawLists);
    }
};

TEST_F(DrawDataTest, CommandListSize)
{
    EXPECT_EQ(ArraySize(DrawData.CommandList), 3);
}

TEST_F(DrawDataTest, TotalVertexCount)
{
    EXPECT_EQ((int32_t)DrawData.TotalVertexCount, 4 * 3 * 3);
}

TEST_F(DrawDataTest, TotalIndexCount)
{
    EXPECT_EQ((int32_t)DrawData.TotalIndexCount, 6 * 3 * 3);
}