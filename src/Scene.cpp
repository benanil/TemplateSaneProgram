
#include <math.h> // sinf cosf
#include "Scene.hpp"

#include "../ASTL/Math/Matrix.hpp"
#include "../ASTL/Containers.hpp"
#include "../ASTL/IO.hpp"

#include "AssetManager.hpp"
#include "Camera.hpp"
#include "Platform.hpp"

namespace
{
    Camera camera;
}

static void WindowResizeCallback(int width, int height)
{
    camera.RecalculateProjection(width, height);
}

int ImportScene(Scene* scene, const char* inPath, float scale, bool LoadToGPU)
{
    int parsed = 1;
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

        APrimitive primitive = data.meshes[0].primitives[0];
        primitive.indices  = data.allIndices;
        primitive.vertices = data.allVertices;
        primitive.numIndices  = data.totalIndices;
        primitive.numVertices = data.totalVertices;
        primitive.indexType   = GraphicType_UnsignedInt;
        CreateMeshFromPrimitive(&primitive, &scene->bigMesh);
    }

    // init scene function ?
    Vector2i windowStartSize;
    GetMonitorSize(&windowStartSize.x, &windowStartSize.y);

    camera.Init(windowStartSize);
    SetWindowResizeCallback(WindowResizeCallback);

    return parsed;
}

void RenderOneMesh(GPUMesh mesh, Texture albedo, Texture normal, Texture metallic, Texture roughness)
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
    Vector3f lightPos = MakeVec3(0.0f, Abs(Cos(time)), Sin(time)) * 100.0f;

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

// from Renderer.cpp
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
    time += (float)GetDeltaTime() * 0.2f;
    Vector3f lightPos = MakeVec3(0.0f, Abs(cosf(time)) + 0.1f, sinf(time)) * 100.0f;
    
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
    Matrix4 model = Matrix4::CreateScale(0.02f,0.02f,0.02f);
    Matrix4 mvp = model * camera.view * camera.projection;

    SetModelViewProjection(mvp.GetPtr());
    SetModelMatrix(model.GetPtr());

    BindMesh(scene->bigMesh);

    for (int i = 0; i < numNodes; i++)
    {
        ANode node = hasScene ? data.nodes[defaultScene.nodes[i]] : data.nodes[i];
        // if node is not mesh skip (camera)
        if (node.type != 0 || node.index == -1) 
            continue;

        Matrix4 model = Matrix4::PositionRotationScale(node.translation, node.rotation, node.scale);
        Matrix4 mvp = model * camera.view * camera.projection;

        SetModelViewProjection(mvp.GetPtr());
        SetModelMatrix(model.GetPtr());

        AMesh mesh = data.meshes[node.index];

        for (int j = 0; j < mesh.numPrimitives; ++j)
        {
            APrimitive& primitive = mesh.primitives[j];
            
            if (primitive.numIndices == 0)
                continue;

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
            int offset = primitive.indexOffset;
            RenderMeshIndexOffset(scene->bigMesh, primitive.numIndices, offset);
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

    DeleteMesh(scene->bigMesh);
    
    if (scene->textures) {
        for (int i = 0; i < data.numTextures; i++) {
            DeleteTexture(scene->textures[i]);
        }
    }

    delete[] scene->textures;

    FreeParsedGLTF(&scene->data);
}