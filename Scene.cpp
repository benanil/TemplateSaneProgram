
#include "Scene.hpp"

#include <thread>

#define STB_DXT_IMPLEMENTATION

#include "External/stb_image.h"
#include "External/etcpak/ProcessDxtc.hpp"
#include "External/etcpak/ProcessRGB.hpp"
#include "External/zstd/zstd.h"

#ifndef __ANDROID__
#include "External/stb_dxt.h"
#include "External/astc-encoder/astcenccli_internal.h"
#endif

#include "ASTL/Math/Matrix.hpp"
#include "ASTL/Containers.hpp"
#include "ASTL/IO.hpp"

#include "AssetManager.hpp"
#include "Camera.hpp"
#include "Platform.hpp"

// from Renderer.cpp
extern unsigned int g_DefaultTexture;

namespace
{
    struct WaitingTexture
    {
        unsigned char* img;
        int width, height, channels;
    };

    Array<WaitingTexture> waitingTextures(100, 0);

    int numProcessedTextures = 0;
    std::thread textureLoadThread;
    Camera camera;
    
    struct ImageInfo
    {
        int width, height;
        int numComp;
    };
}

static void WindowResizeCallback(int width, int height)
{
    camera.RecalculateProjection(width, height);
}

static void ChangeExtension(char* path, int pathLen, const char* newExt)
{
    int lastDot = pathLen - 1;
    while (path[lastDot - 1] != '.')
        lastDot--;

    int i = lastDot;
    for (; *newExt; i++)
        path[i] = *newExt++;
    // clean the right with zeros
    while (i < pathLen)
        path[i++] = '\0';
}

extern unsigned char* g_TextureLoadBuffer;

typedef int (*ImageCompressFn)(const void* src, void* dst, int width, int height);
typedef int (*ImageSizeFn)(int width, int height);

static int WidthMulHeightFn    (int width, int height) { return width * height; }
static int WidthMulHeightDiv2Fn(int width, int height) { return (width * height) >> 1; }

extern uint64_t astcenc_main(const char* input_filename, unsigned char* currentCompression);

static void SaveSceneImagesGeneric(Scene* scene, char* path, const char* inPath, bool isMobile)
{
#ifndef __ANDROID__
    AFile file = AFileOpen(path, AOpenFlag_Write);
    Array<ImageInfo> imageInfos;
    
    uint64_t beforeCompressedSize = 0;

    // todo: make this multi threaded if texture count is greater than 4
    for (int i = 0; i < scene->data.numImages; i++)
    {
        ImageInfo info;
        info.width = 32, info.height = 32, info.numComp = 4;

        if (scene->data.images[i].path == nullptr || !FileExist(scene->data.images[i].path))
        {    
            AFileWrite(&info, sizeof(ImageInfo), file);
            imageInfos.Add(info);
            beforeCompressedSize += info.width * info.height;
            continue;
        }
     
        const char* imageFileName = scene->data.images[i].path;
        int res = stbi_info(imageFileName, &info.width, &info.height, &info.numComp);
        ASSERT(res);
        AFileWrite(&info, sizeof(ImageInfo), file);
        imageInfos.Add(info);

        int imageSize = info.width * info.height;
        // we have got to include mipmap sizes if mobile
        if (isMobile)
        {
            // 512->3, 1024->4, 2049->5
            int numMips = MAX(Log2((unsigned int)info.width) >> 1u, 1u) - 1;
            while (numMips--)
            {
                info.width  >>= 1;
                info.height >>= 1;
                imageSize += info.width * info.height;
            }
        }

        beforeCompressedSize += imageSize;
    }

    unsigned char* toCompressionBuffer = new unsigned char[beforeCompressedSize];
    unsigned char* currentCompression = toCompressionBuffer;

    for (int i = 0; i < imageInfos.Size(); i++)
    {
        ImageInfo info = imageInfos[i];
        const char* imagePath = scene->data.images[i].path;

        if (imagePath == nullptr || !FileExist(imagePath))
        {
            currentCompression += 32 * 32;
            continue;
        }

        if (!isMobile)
        {
            unsigned char* stbImage = stbi_load(imagePath, &info.width, &info.height, 0, STBI_rgb_alpha);
            ResizeTextureLoadBufferIfNecessarry(info.width * info.height);

            int imageSize = info.width * info.height;
            if (info.numComp == 4) {
                CompressDxt5((const uint32_t*)stbImage, (uint64_t*)g_TextureLoadBuffer, info.width, info.height);
            }
            else {
                rygCompress(g_TextureLoadBuffer, stbImage, info.width, info.height);
                imageSize >>= 1;
            }
            stbi_image_free(stbImage);

            SmallMemCpy(currentCompression, g_TextureLoadBuffer, imageSize);
            currentCompression += imageSize;
        }
        else
        {
            uint64_t numBytes = astcenc_main(imagePath, currentCompression);
            ASSERT(numBytes != 1);
            currentCompression += numBytes;
        }
    }
    
    unsigned char* end = (toCompressionBuffer + beforeCompressedSize);
    ASSERT(end == currentCompression);

    uint64_t compressedSize = beforeCompressedSize * 0.92;
    char* compressedBuffer = new char[compressedSize];
    
    compressedSize = ZSTD_compress(compressedBuffer, compressedSize, toCompressionBuffer, beforeCompressedSize, 9);
    ASSERT(!ZSTD_isError(compressedSize));

    uint64_t decompressedSize = beforeCompressedSize;
    AFileWrite(&decompressedSize, sizeof(uint64_t), file);
    AFileWrite(&compressedSize, sizeof(uint64_t), file);
    AFileWrite(compressedBuffer, compressedSize, file);

    AFileClose(file);

    delete[] toCompressionBuffer;
    delete[] compressedBuffer;
#endif
}

static void LoadSceneImagesGeneric(const char* texturePath, Texture* textures, int numImages,
                                   ImageSizeFn rgbaFn, ImageSizeFn rgbFn, bool isMobile)
{
    AFile file = AFileOpen(texturePath, AOpenFlag_Read);
    
    Array<ImageInfo> imageInfos(numImages);

    for (int i = 0; i < numImages; i++)
    {
        ImageInfo info;
        AFileRead(&info, sizeof(ImageInfo), file);
        imageInfos.PushBack(info);
    }

    uint64_t decompressedSize, compressedSize;
    AFileRead(&decompressedSize, sizeof(uint64_t), file);
    AFileRead(&compressedSize, sizeof(uint64_t), file);
    
    unsigned char* compressedBuffer = new unsigned char[compressedSize];
    AFileRead(compressedBuffer, compressedSize, file);
    
    unsigned char* decompressedBuffer = new unsigned char[decompressedSize];
    decompressedSize = ZSTD_decompress(decompressedBuffer, decompressedSize, compressedBuffer, compressedSize);
    ASSERT(!ZSTD_isError(decompressedSize));

    unsigned char* currentImage = decompressedBuffer;
    ImageSizeFn imageSizeFns[] = { rgbFn, rgbaFn };

    for (int i = 0; i < numImages; i++)
    {
        ImageInfo info = imageInfos[i];
        if (info.width == 32)
        {
            currentImage += 32 * 32;
            continue;
        }
        bool hasAlpha = info.numComp == 4;
        int imageSize = imageSizeFns[hasAlpha](info.width, info.height);
        textures[i] = CreateTexture(info.width, info.height, currentImage, hasAlpha, true, true);
        currentImage += imageSize;
        if (isMobile)
        {
            int mip = MAX((int)Log2((unsigned int)info.width) >> 1, 1) - 1;
            while (mip-- > 0)
            {
                info.width >>= 1;
                info.height >>= 1;
                currentImage += info.width * info.height;
            } 
        }
    }
    
    delete[] decompressedBuffer;
    delete[] compressedBuffer;
    AFileClose(file);
}

static void SaveSceneImages(Scene* scene, char* path, const char* inPath)
{
    // // save dxt textures for desktop
    // ChangeExtension(path, StringLength(path), "dxt");
    // SaveSceneImagesGeneric(scene, path, inPath, CompressDxt5fn, CompressDxt1fn);
    
    // save astc textures for android
    ChangeExtension(path, StringLength(path), "astc");
    SaveSceneImagesGeneric(scene, path, inPath, true);
}

static void LoadSceneImages(char* path, Texture*& textures, int numImages)
{
    textures = new Texture[numImages]();
#ifdef __ANDROID__
    ChangeExtension(path, StringLength(path), "astc");
    LoadSceneImagesGeneric(path, textures, numImages, WidthMulHeightFn, WidthMulHeightFn, true);
#else
    ChangeExtension(path, StringLength(path), "dxt");
    LoadSceneImagesGeneric(path, textures, numImages, WidthMulHeightFn, WidthMulHeightDiv2Fn, false);
#endif
}


int ImportScene(Scene* scene, const char* inPath, float scale, bool LoadToGPU)
{
    bool parsed = true;
    char path[256]{};
    
    // ParseGLTF(inPath, &scene->data, scale);
    
    int pathLen = StringLength(inPath);
    SmallMemCpy(path, inPath, pathLen);

    // save scene and the textures as binary
    
    ChangeExtension(path, pathLen, "abm");
    // SaveGLTFBinary(&scene->data, path);
    
    parsed = LoadGLTFBinary(path, &scene->data);

    if (!parsed)
        return 0;

    SaveSceneImages(scene, path, inPath);

    if (LoadToGPU)
    {
        LoadSceneImages(path, scene->textures, scene->data.numImages);
    
        ParsedGLTF& data = scene->data;

        int numMeshes = 0;
        for (int i = 0; i < data.numMeshes; i++)
        {
            numMeshes += data.meshes[i].numPrimitives;
        }

        scene->meshes = numMeshes ? new Mesh[numMeshes]{} : nullptr;

        for (int i = 0; i < numMeshes; i++)
        {
            scene->meshes[i] = CreateMeshFromPrimitive(&data.meshes[i].primitives[0]);
        }
    }

    // init scene function ?
    Vector2i windowStartSize;
    GetMonitorSize(&windowStartSize.x, &windowStartSize.y);

    camera.Init(windowStartSize);
    SetWindowResizeCallback(WindowResizeCallback);

    return parsed;
}

void RenderScene(Scene* scene)
{
    ParsedGLTF& data = scene->data;
    for (int i = 0; i < data.numNodes; i++)
    {
        ANode node = data.nodes[i];
        // if node is not mesh skip (camera)
        if (node.type != 0 || node.index == -1) continue;

        Matrix4 model = Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale);
        Matrix4 mvp = model * camera.view * camera.projection;

        SetModelViewProjection(mvp.GetPtr());
        SetModelMatrix(model.GetPtr());

        AMesh mesh = data.meshes[node.index];
        
        for (int j = 0; j < mesh.numPrimitives; ++j)
        {
            if (scene->meshes[node.index].numIndex == 0)
                continue;

            AMaterial material = data.materials[mesh.primitives[j].material];
            SetMaterial(&material);
            
            if (scene->textures && material.baseColorTexture.index != -1)
                SetTexture(scene->textures[material.baseColorTexture.index], 0);
    
            RenderMesh(scene->meshes[node.index]);
        }
    }
}

void UpdateScene(Scene* scene)
{
    // note: maybe load, textures one by one each frame like we do before.
    camera.Update();
}

void DestroyScene(Scene* scene)
{
    ParsedGLTF& data = scene->data;

    if (scene->meshes)   for (int i = 0; i < data.numMeshes; i++) DeleteMesh(scene->meshes[i]);
    if (scene->textures) for (int i = 0; i < data.numTextures; i++) DeleteTexture(scene->textures[i]);

    delete[] scene->meshes;
    delete[] scene->textures;

    FreeParsedGLTF(&scene->data);
}