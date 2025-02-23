#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "AxWindow/AxWindow.h"
#include "AxOpenGL/AxOpenGL.h"

class OpenGLTest : public testing::Test
{
    protected:
        AxOpenGLAPI *RenderAPI;
        AxWindowAPI *WindowAPI;
        AxWindow *Window;
        void SetUp()
        {
            AxonInitGlobalAPIRegistry();
            AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

            // Load the main DLL
            PluginAPI->Load("libAxWindow.dll", false);
            PluginAPI->Load("libAxOpenGL.dll", false);

            WindowAPI = (AxWindowAPI *)AxonGlobalAPIRegistry->Get(AXON_WINDOW_API_NAME);
            ASSERT_NE(WindowAPI, nullptr);

            RenderAPI = (AxOpenGLAPI *)AxonGlobalAPIRegistry->Get(AXON_OPENGL_API_NAME);
            ASSERT_NE(RenderAPI, nullptr);

            const int32_t StyleFlags = AX_WINDOW_STYLE_VISIBLE | AX_WINDOW_STYLE_DECORATED | AX_WINDOW_STYLE_RESIZABLE;
            WindowAPI->Init();
            Window = WindowAPI->CreateWindow(
                "AxonSDK OpenGL Tests",
                1,
                1,
                800,
                600,
                (AxWindowStyle)StyleFlags
            );

            ASSERT_NE(Window, nullptr);

            RenderAPI->CreateContext(Window);
        }

        void TearDown()
        {
            RenderAPI->DestroyContext();
            WindowAPI->DestroyWindow(Window);
            AxonTermGlobalAPIRegistry();
        }
};

TEST_F(OpenGLTest, CreateTexture)
{
    AxTexture Texture = {};
    Texture.Width = 4;
    Texture.Height = 4;
    Texture.Channels = 4;

    uint8_t Pixels[4 * 4 * 4] = {
        // Row 1
        255,   0,   0, 255,   // Red
          0, 255,   0, 255,   // Green
        255,   0,   0, 255,   // Red
          0, 255,   0, 255,   // Green

        // Row 2
          0,   0, 255, 255,   // Blue
        255, 255,   0, 255,   // Yellow
          0,   0, 255, 255,   // Blue
        255, 255,   0, 255,   // Yellow

        // Row 3
        255,   0,   0, 255,   // Red
          0, 255,   0, 255,   // Green
        255,   0,   0, 255,   // Red
          0, 255,   0, 255,   // Green

        // Row 4
          0,   0, 255, 255,   // Blue
        255, 255,   0, 255,   // Yellow
          0,   0, 255, 255,   // Blue
        255, 255,   0, 255    // Yellow
    };

    RenderAPI->InitTexture(&Texture, &Pixels[0]);
    EXPECT_EQ(Texture.ID, 1);

    RenderAPI->DestroyTexture(&Texture);
}
