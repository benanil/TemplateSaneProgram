
#include "Scene.hpp"

#include <thread>
#include "External/stb_image.h"

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

    int numWaitingTextures = 0;
    int numProcessedTextures = 0;
    std::thread textureLoadThread;
    Camera camera;
}

static void WindowResizeCallback(int width, int height)
{
    camera.RecalculateProjection(width, height);
}

static void MultiThreadedTextureLoad2(Texture* textures, AImage* images, int numImages)
{
    // use default texture first
    for (int i = 0; i < numImages; i++)
    {
        textures[i].handle = g_DefaultTexture;
    }

    const int textureSize = 1024 * 1024 * 4;
    unsigned char* textureBuffer = new unsigned char[textureSize];

    for (int i = 0; i < numImages; i++)
    {
        if (images[i].path)
        {
            WaitingTexture waitingTexture;
            AFile file = AFileOpen(images[i].path, AOpenFlag_Read);

            if (!AFileExist(file))
            {
                waitingTexture.img = nullptr;
                waitingTextures.Add(waitingTexture);
                numWaitingTextures++;
                continue;
            }

            int bufferSize = AFileSize(file);
            AFileRead(textureBuffer, bufferSize, file);

            waitingTexture.img = stbi_load_from_memory(textureBuffer, bufferSize, &waitingTexture.width, &waitingTexture.height, &waitingTexture.channels, 0);
            waitingTextures.Add(waitingTexture);
            numWaitingTextures++;
            AFileClose(file);
        }
    }

    delete[] textureBuffer;
}

static void LoadSceneMeshesAndTexturesToGPU(Scene* scene)
{
    ASSERT(scene != nullptr);
    ParsedGLTF& data = scene->data;

    int numMeshes = 0;
    for (int i = 0; i < data.numMeshes; i++)
    numMeshes += data.meshes[i].numPrimitives;

    scene->meshes = numMeshes ? new Mesh[numMeshes]{} : nullptr;

    for (int i = 0; i < numMeshes; i++)
    {
        scene->meshes[i] = CreateMeshFromPrimitive(&data.meshes[i].primitives[0]);
    }

    scene->textures = data.numImages ? new Texture[data.numImages]{} : nullptr;
    if (data.numImages > 6)
    {
        // MultiThreadedTextureLoad(scene->textures, data.images, data.numImages);
        textureLoadThread = std::thread(MultiThreadedTextureLoad2, scene->textures, data.images, data.numImages);
    }
    else
    {
        for (int i = 0; i < data.numImages; i++)
        {
            if (data.images[i].path)
            {
                scene->textures[i] = LoadTexture(data.images[i].path, true);
            }
        }
    }
}

int ImportScene(Scene* scene, const char* path, float scale, bool LoadToGPU)
{
    int length = StringLength(path);
    bool parsed = false;
#ifndef __ANDROID__
    if (FileHasExtension(path, length, "fbx"))  LoadFBX(path, &scene->data, scale), parsed = true;
#endif
    if (FileHasExtension(path, length, "abm"))  LoadGLTFBinary(path, &scene->data), parsed = true;
    if (FileHasExtension(path, length, "gltf")) ParseGLTF(path, &scene->data, scale), parsed = true;
    
    if (LoadToGPU && parsed) 
        LoadSceneMeshesAndTexturesToGPU(scene);
    
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
    if (numProcessedTextures < numWaitingTextures)
    {
        int i = numProcessedTextures;
        if (waitingTextures[i].img != nullptr)
        {
            Texture* textures = scene->textures;
            const TextureType numCompToFormat[5] = { 0, TextureType_R8, TextureType_RG8, TextureType_RGB8, TextureType_RGBA8 };
            textures[i] = CreateTexture(waitingTextures[i].width, waitingTextures[i].height, waitingTextures[i].img, true, numCompToFormat[waitingTextures[i].channels]);
            stbi_image_free(waitingTextures[i].img);
        }
        numProcessedTextures++;
    }

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