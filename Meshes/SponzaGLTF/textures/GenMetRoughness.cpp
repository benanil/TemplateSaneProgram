
#include <stdio.h>
#include "../../../ASTL/Math/Vector.hpp"
#include "../../../ASTL/IO.hpp"
#include "../../../ASTL/Memory.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include "../../../External/stb_image.h"
#include "../../../External/stb_image_write.h"

float rgb2vibrance(Vector3f c)
{
    float p = Lerp(c.z, c.y, Step(c.z, c.y));
    float q = Lerp(c.x, p, Step(p, c.x));
    return q * q;
}

int main(int argc, const char* argv[])
{
    if (!FileExist(argv[1])) {
        printf("file is not exist!");
        return 0;
    }

    int width, height, numComp;
    unsigned char* diffuse = stbi_load(argv[1], &width, &height, &numComp, 0);
    
    if (diffuse == nullptr) {
        printf("load failed! path: %s", argv[1]);
        return 0;
    }

    printf("width: %i, height: %i \n", width, height);
    
    ScopedPtr<unsigned char> result = new unsigned char[width * height * 4];
    
    unsigned char* curr = result.ptr;
    
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            int index = (width * numComp * i) + (j * numComp);
            Vector3f albedo = {
                float(diffuse[index + 0]) / 255.0f,
                float(diffuse[index + 1]) / 255.0f,
                float(diffuse[index + 2]) / 255.0f
            };

            float luminance = Vector3f::Dot(albedo, MakeVec3(0.2126f, 0.7152f, 0.0722f));
            float equality  = luminance / MAX(MAX(albedo.x, albedo.y), albedo.z);
            float vibrance  = rgb2vibrance(albedo);
    
            float metallic, roughness;
            metallic  = Lerp(equality, 1.0f-vibrance, 0.5f);
            metallic -= 0.2f;
            metallic *= 1.2f;
            metallic = Clamp(metallic, 0.001f, 1.0f);

            // luminance = (1.0 - luminance); //1.0f - (roughness);
            roughness = Lerp(1.0f - luminance,  equality, 0.5f);
            roughness = 1.0f - (roughness);
            roughness = Lerp(roughness, (1.0f - metallic), 0.35f);
            roughness = Clamp(roughness, 0.0f, 1.0f);
            curr[0] = (unsigned char)(metallic * 255.0f);
            curr[1] = (unsigned char)(roughness * 255.0f);
            curr[2] = 0;
            curr[3] = 255;
            curr += 4;
        }
    }
    char path[256] = {0};
    char fileName[64] = {0};
    
    int pathLen = StringLength(argv[1]);
    SmallMemCpy(path, argv[1], pathLen);
    
    path[pathLen + 2] = 'g';
    path[pathLen + 1] = 'n';
    path[pathLen    ] = 'p';
    path[pathLen - 1] = '.';
    path[pathLen - 2] = 'T';
    path[pathLen - 3] = 'R';
    path[pathLen - 4] = 'M';
    
    stbi_write_png(path, width, height, 4, result.ptr, width * 4);
    stbi_image_free(diffuse);
    
    return 0;
}