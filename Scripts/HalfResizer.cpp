
#include <stdio.h>
#include "../ASTL/Math/Vector.hpp"
#include "../ASTL/IO.hpp"
#include "../ASTL/Memory.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include "../External/stb_image.h"
#include "../External/stb_image_resize2.h"
#include "../External/stb_image_write.h"

template<int channelsBefore>
static void MakeRGBA(const unsigned char* RESTRICT from, unsigned char* rgba, int numPixels)
{
    static_assert(channelsBefore < 4, "image can't have more than 3 components before resizing");
    for (int i = 0; i < numPixels; i++)
    {
        MemsetZero(rgba, 4 * sizeof(char));
        for (int p = 0; p < channelsBefore; p++)
        {
            rgba[p] = from[p];
        }
        from += channelsBefore;
        rgba += 4;
    }
}

int main(int argc, const char* argv[])
{
    const char* path = argv[1];

    if (!FileExist(argv[1])) {
        printf("file is not exist!");
        return 0;
    }

    int width, height, numComp;
    unsigned char* diffuse = stbi_load(path, &width, &height, &numComp, 0);
    
    if (diffuse == nullptr) {
        printf("load failed! path: %s\n", path);
        return 0;
    }

    printf("source width: %i, height: %i, numComp: %i \n", width, height, numComp);
    
    if (numComp != 4) {
        unsigned char* buff = (unsigned char*)malloc(width * height * 4);
        if (numComp == 3) MakeRGBA<3>(diffuse, buff, width * height);
        if (numComp == 2) MakeRGBA<2>(diffuse, buff, width * height);
        stbi_image_free(diffuse);
        diffuse = buff;
        printf("corrected rgba from: %i \n", numComp);
    }
    numComp = 4;
    
    ScopedPtr<unsigned char> resizeBuffer = new unsigned char[width * height * numComp];
    void* resized = stbir_resize(diffuse, width, height, width * numComp, 
                                 resizeBuffer.ptr, width >> 1, height >> 1, (width >> 1) * numComp, 
                                 STBIR_RGBA, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL);
        
    if (resized == nullptr) {
        printf("stbir_resize failed");
        stbi_image_free(diffuse);
        return 0;
    }
    width >>= 1;
    height >>= 1;
    printf("success width: %i, height: %i \n", width, height);

    char path2[64] = {0};
    int pathLen = StringLength(path);
    SmallMemCpy(path2, path, pathLen);
        
    path2[pathLen + 2] = 'g';
    path2[pathLen + 1] = 'n';
    path2[pathLen    ] = 'p';
    path2[pathLen - 1] = '.';
    path2[pathLen - 2] = 'F';
    path2[pathLen - 3] = 'L';
    path2[pathLen - 4] = 'H';
    printf("write path: %s \n\n", path2);

    int writeRes = stbi_write_jpg(path, width, height, numComp, resizeBuffer.ptr, 100);
    if (writeRes == 0) printf("write failed!\n");

    printf("Write Success\n");
    stbi_image_free(diffuse);
    
    return 0;
}