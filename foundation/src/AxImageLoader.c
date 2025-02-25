#include "AxImageLoader.h"

#define STBI_NO_JPEG
#define STBI_NO_BMP
#define STBI_NO_PSD

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.inl"

// AxonImageLoaderAPI ////////////////////////////////
static bool LoadImage(const char *Path, AxImage *Image)
{
    int Width, Height, Channels;
    uint8_t *Data = stbi_load(Path, &Width, &Height, &Channels, 0);

    if (Data)
    {
        Image->Width = Width;
        Image->Height = Height;
        Image->Channels = Channels;
        Image->Size = Width * Height * Channels;
        Image->Pixels = Data;
    }

    return (true);
}

struct AxImageAPI *AxImageAPI = &(struct AxImageAPI) {
    .LoadImage = LoadImage,
};
