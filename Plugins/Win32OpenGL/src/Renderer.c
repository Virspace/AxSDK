#include "Win32OpenGL/Renderer.h"

AXON_DLL_EXPORT void LoadPlugin(struct axon_api_registry *APIRegistry, bool Load)
{
    if (APIRegistry)
    {
        APIRegistry->Set(WIN32_OPENGL_NAME)
    }
}
