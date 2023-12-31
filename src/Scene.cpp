
#include "Scene.hpp"

#include <thread>

#define STB_DXT_IMPLEMENTATION

#include "../External/stb_image.h"
#include "../External/ProcessDxtc.hpp"
#include "../External/zstd.h"

#ifndef __ANDROID__
#include "../External/stb_dxt.h"
#include "../External/astc-encoder/astcenccli_internal.h"
#endif

#include "../ASTL/Math/Matrix.hpp"
#include "../ASTL/Containers.hpp"
#include "../ASTL/IO.hpp"

#include "AssetManager.hpp"
#include "Camera.hpp"
#include "Platform.hpp"

// from Renderer.cpp
extern unsigned int g_DefaultTexture;
extern unsigned char* g_TextureLoadBuffer;

namespace
{
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

extern uint64_t astcenc_main(const char* input_filename, unsigned char* currentCompression);

static void SaveSceneImagesGeneric(Scene* scene, char* path, bool isMobile)
{
#ifndef __ANDROID__
    AFile file = AFileOpen(path, AOpenFlag_Write);
    int numImages = scene->data.numImages;
    int currentInfo = 0;
    
    if (numImages == 0) {
        AFileClose(file);
        return;
    }

    ImageInfo* imageInfos = new ImageInfo[numImages];
    uint64_t* currentCompressions = new uint64_t[numImages];

    uint64_t beforeCompressedSize = 0;

    // todo: make this multi threaded if texture count is greater than 4
    for (int i = 0; i < numImages; i++)
    {
        ImageInfo info;
        info.width = 32, info.height = 32, info.numComp = 4;

        if (scene->data.images[i].path == nullptr || !FileExist(scene->data.images[i].path))
        {
            AFileWrite(&info, sizeof(ImageInfo), file);
            imageInfos[currentInfo] = info;
            currentCompressions[currentInfo++] = beforeCompressedSize;
            beforeCompressedSize += info.width * info.height;
            continue;
        }

        const char* imageFileName = scene->data.images[i].path;
        int res = stbi_info(imageFileName, &info.width, &info.height, &info.numComp);
        ASSERT(res);
        AFileWrite(&info, sizeof(ImageInfo), file);
        currentCompressions[currentInfo] = beforeCompressedSize;
        imageInfos[currentInfo++] = info;
        bool isDxt1 = (isMobile == false) && (info.numComp == 3);
        int imageSize = (info.width * info.height) >> isDxt1;
        // we have got to include mipmap sizes if mobile
        if (isMobile)
        {
            // 512->3, 1024->4, 2049->5
            int numMips = MAX(Log2((unsigned int)info.width) >> 1u, 1u) - 1;
            while (numMips--)
            {
                info.width >>= 1;
                info.height >>= 1;
                imageSize += info.width * info.height;
            }
        }

        beforeCompressedSize += imageSize;
    }

    unsigned char* toCompressionBuffer = new unsigned char[beforeCompressedSize];

    auto execFn = [imageInfos, toCompressionBuffer, scene, isMobile](int numImages, uint64_t compressionStart, int i) -> void
        {
            unsigned char* textureLoadBuffer = !isMobile ? new unsigned char[1024 * 1024] : nullptr;
            uint64_t loadBufferSize = 1024 * 1024;
            unsigned char* currentCompression = toCompressionBuffer + compressionStart;

            for (int end = i + numImages; i < end; i++)
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
                    int imageSize = info.width * info.height;
                    assert(stbImage);

                    if (loadBufferSize < imageSize)
                    {
                        delete[] textureLoadBuffer;
                        textureLoadBuffer = new unsigned char[imageSize];
                        loadBufferSize = imageSize;
                    }

                    if (info.numComp == 4)
                    {
                        uint32_t numBlocks = (info.width >> 2) * (info.height >> 2);
                        CompressDxt5((const uint32_t*)stbImage, (uint64_t*)textureLoadBuffer, numBlocks, info.width);
                    }
                    else {
                        rygCompress(textureLoadBuffer, stbImage, info.width, info.height);
                        imageSize >>= 1;
                    }

                    SmallMemCpy(currentCompression, textureLoadBuffer, imageSize);
                    currentCompression += imageSize;
                }
                else
                {
                    uint64_t numBytes = astcenc_main(imagePath, currentCompression);
                    ASSERT(numBytes != 1);
                    currentCompression += numBytes;
                }
            }
            delete[] textureLoadBuffer;
        };
    
    int numTask = numImages;
    int taskPerThread = MAX(numImages / 8, 1);
    std::thread threads[9];

    int numThreads = 0;
    for (int i = 0; i <= numTask - taskPerThread; i += taskPerThread)
    {
        int start = i;
        int end = taskPerThread;
        new (threads + numThreads)std::thread(execFn, taskPerThread, currentCompressions[start], start);
        numThreads++;
    }

    int remainingTasks = numTask % taskPerThread;
    if (remainingTasks > 0)
    {
        int start = numImages - remainingTasks;
        new (threads + numThreads)std::thread(execFn, remainingTasks, currentCompressions[start], start);
        numThreads++;
    }

    for (int i = 0; i < numThreads; i++)
    {
        threads[i].join();
    }

    // unsigned char* end = (toCompressionBuffer + beforeCompressedSize);
    // ASSERT(end == currentCompression);

    uint64_t compressedSize = beforeCompressedSize * 0.92;
    char* compressedBuffer = new char[compressedSize];

    compressedSize = ZSTD_compress(compressedBuffer, compressedSize, toCompressionBuffer, beforeCompressedSize, 9);
    ASSERT(!ZSTD_isError(compressedSize));

    uint64_t decompressedSize = beforeCompressedSize;
    AFileWrite(&decompressedSize, sizeof(uint64_t), file);
    AFileWrite(&compressedSize, sizeof(uint64_t), file);
    AFileWrite(compressedBuffer, compressedSize, file);

    AFileClose(file);

    delete[] toCompressionBuffer; delete[] compressedBuffer;
    delete[] imageInfos;          delete[] currentCompressions;
#endif
}

static void LoadSceneImagesGeneric(const char* texturePath, Texture* textures, int numImages, bool isMobile)
{
    if (numImages == 0) {
        return;
    }
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

    for (int i = 0; i < numImages; i++)
    {
        ImageInfo info = imageInfos[i];
        if (info.width == 32)
        {
            currentImage += 32 * 32;
            continue;
        }
        int imageSize = info.width * info.height;
        bool hasAlpha = info.numComp == 4;
        bool isDxt1 = (hasAlpha == false) && (isMobile == false);
        textures[i] = CreateTexture(info.width, info.height, currentImage, hasAlpha, true, true);
        currentImage += imageSize >> isDxt1;

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

static std::thread CompressASTCImagesThread;

void SaveAndroidCompressedImagesFn(Scene* scene, char* astcPath)
{
    SaveSceneImagesGeneric(scene, astcPath, true); // is mobile true
    delete[] astcPath;
}

static void SaveSceneImages(Scene* scene, char* path)
{
    // // save dxt textures for desktop
    ChangeExtension(path, StringLength(path), "dxt");
    SaveSceneImagesGeneric(scene, path, false); // is mobile false
    return;
    // save astc textures for android
    int len = StringLength(path);
    ChangeExtension(path, len, "astc");
    char* astcPath = new char[len + 2] {};
    SmallMemCpy(astcPath, path, len + 1);

    // save textures in other thread because we don't want to wait android textures while on windows platform
    new(&CompressASTCImagesThread)std::thread(SaveAndroidCompressedImagesFn, scene, astcPath);
}

static void LoadSceneImages(char* path, Texture*& textures, int numImages)
{
    textures = new Texture[numImages]();
#ifdef __ANDROID__
    ChangeExtension(path, StringLength(path), "astc");
    LoadSceneImagesGeneric(path, textures, numImages, true);
#else
    ChangeExtension(path, StringLength(path), "dxt");
    LoadSceneImagesGeneric(path, textures, numImages, false);
#endif
}

int ImportScene(Scene* scene, const char* inPath, float scale, bool LoadToGPU)
{
    bool parsed = true;
    char path[256]{};

    int pathLen = StringLength(inPath);
    SmallMemCpy(path, inPath, pathLen);

    ChangeExtension(path, pathLen, "abm");
    bool firstLoad = !FileExist(path) || !IsABMLastVersion(path);
    if (firstLoad)
    {
        ChangeExtension(path, StringLength(path), "gltf");
        parsed &= ParseGLTF(path, &scene->data, scale); ASSERT(parsed);
        CreateVerticesIndices(&scene->data);

        ChangeExtension(path, StringLength(path), "abm");
        parsed &= SaveGLTFBinary(&scene->data, path); ASSERT(parsed);
        SaveSceneImages(scene, path); // save textures as binary
    }
    else
    {
        parsed = LoadGLTFBinary(path, &scene->data);
    }

    if (!parsed)
        return 0;

    if (LoadToGPU)
    {
        LoadSceneImages(path, scene->textures, scene->data.numImages);

        // load scene meshes
        ParsedGLTF& data = scene->data;

        int numMeshes = 0;
        for (int i = 0; i < data.numMeshes; i++)
        {
            numMeshes += data.meshes[i].numPrimitives;
        }

        scene->meshes = numMeshes ? new Mesh[numMeshes]{} : nullptr;

        for (int i = 0, k = 0; i < data.numMeshes; i++)
        {
            AMesh& mesh = data.meshes[i];
            for (int j = 0; j < mesh.numPrimitives; j++, k++)
            {
                CreateMeshFromPrimitive(&data.meshes[i].primitives[j], &scene->meshes[k]);
            }
        }
    }

    // init scene function ?
    Vector2i windowStartSize;
    GetMonitorSize(&windowStartSize.x, &windowStartSize.y);

    camera.Init(windowStartSize);
    SetWindowResizeCallback(WindowResizeCallback);

    return parsed;
}

void RenderOneMesh(Mesh mesh, Texture albedo, Texture normal, Texture metallic, Texture roughness)
{ 
    Shader currentShader = GetCurrentShader();
    
    unsigned int viewPosLoc      = GetUniformLocation(currentShader, "viewPos");
    unsigned int lightPosLoc     = GetUniformLocation(currentShader, "lightPos");
    unsigned int hasNormalMapLoc = GetUniformLocation(currentShader, "hasNormalMap");
    
    // textures
    unsigned int albedoLoc        = GetUniformLocation(currentShader, "albedo");
    unsigned int normalMapLoc     = GetUniformLocation(currentShader, "normalMap");
    unsigned int metallicMapLoc   = GetUniformLocation(currentShader, "metallicMap");
    unsigned int roughnessMapLoc  = GetUniformLocation(currentShader, "roughnessMap");

    static float time = 5.2f;
    // time += GetDeltaTime() * 0.2f;
    Vector3f lightPos = MakeVec3(0.0f, cosf(time), sinf(time)) * 100.0f;

    SetShaderValue(&camera.position.x, viewPosLoc, GraphicType_Vector3f);
    SetShaderValue(&lightPos.x, lightPosLoc, GraphicType_Vector3f);
    SetShaderValue(true, hasNormalMapLoc);
    
    Matrix4 model = Matrix4::Identity();
    Matrix4 mvp   = model * camera.view * camera.projection;

    SetModelViewProjection(mvp.GetPtr());
    SetModelMatrix(model.GetPtr());
        
    SetTexture(albedo   , 0, albedoLoc);
    SetTexture(normal   , 1, normalMapLoc);
    SetTexture(metallic , 2, metallicMapLoc);
    SetTexture(roughness, 3, roughnessMapLoc);

    RenderMesh(mesh);
}

extern unsigned int g_DefaultTexture;

void RenderScene(Scene* scene)
{
    ParsedGLTF& data = scene->data;
    Shader currentShader = GetCurrentShader();
    
    unsigned int viewPosLoc      = GetUniformLocation(currentShader, "viewPos");
    unsigned int lightPosLoc     = GetUniformLocation(currentShader, "lightPos");
    unsigned int albedoLoc       = GetUniformLocation(currentShader, "albedo");
    unsigned int normalMapLoc    = GetUniformLocation(currentShader, "normalMap");
    unsigned int hasNormalMapLoc = GetUniformLocation(currentShader, "hasNormalMap");
    unsigned int metallicMapLoc  = GetUniformLocation(currentShader, "metallicRoughnessMap");

    static float time = 5.2f;
    //time += GetDeltaTime() * 0.2f;
    Vector3f lightPos = MakeVec3(0.0f, cosf(time), Abs(sinf(time))) * 100.0f;
    
    SetShaderValue(&camera.position.x, viewPosLoc, GraphicType_Vector3f);
    SetShaderValue(&lightPos.x, lightPosLoc, GraphicType_Vector3f);

    int numNodes  = data.numNodes;
    bool hasScene = data.numScenes > 0 && data.scenes[data.defaultSceneIndex].numNodes > 1;
    AScene defaultScene;
    if (hasScene)
    {
        defaultScene = data.scenes[data.defaultSceneIndex];
        numNodes = defaultScene.numNodes;
    }

    for (int i = 0; i < numNodes; i++)
    {
        ANode node = hasScene ? data.nodes[defaultScene.nodes[i]] : data.nodes[i];
        // if node is not me    sh skip (camera)
        if (node.type != 0 || node.index == -1) continue;

        float identity[4] = {0.0f, 0.0f, 0.0f, 1.0f };
        Matrix4 model = Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale);
        Matrix4 mvp = model * camera.view * camera.projection;

        SetModelViewProjection(mvp.GetPtr());
        SetModelMatrix(model.GetPtr());

        AMesh mesh = data.meshes[node.index];

        for (int j = 0; j < mesh.numPrimitives; ++j)
        {
            if (scene->meshes[node.index].numIndex == 0)
                continue;

            APrimitive& primitive = mesh.primitives[j];
            AMaterial material = data.materials[primitive.material];
            // SetMaterial(&material);

            int baseColorIndex = material.baseColorTexture.index;
            if (scene->textures && baseColorIndex != -1)
                SetTexture(scene->textures[baseColorIndex], 0, albedoLoc);
            
            int normalIndex = material.GetNormalTexture().index;
            int hasNormalMap = !!(primitive.attributes & AAttribType_TANGENT) && normalIndex != -1;

            if (scene->textures && hasNormalMap)
                SetTexture(scene->textures[normalIndex], 1, normalMapLoc);
            
            SetShaderValue(hasNormalMap, hasNormalMapLoc);

            int metalicRoughnessIndex = material.metallicRoughnessTexture.index;
            
            // if (scene->textures && metalicRoughnessIndex != -1 && scene->textures[metalicRoughnessIndex].width != 0)
            //     SetTexture(scene->textures[metalicRoughnessIndex], 2, metallicMapLoc);
            // else
            {
                Texture texture;
                texture.handle = g_DefaultTexture;
                SetTexture(texture, 2, metallicMapLoc);
            }

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