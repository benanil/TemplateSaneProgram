
#include <math.h> // sinf cosf

#include "Scene.hpp"

#include "../ASTL/Math/Matrix.hpp"
#include "../ASTL/Containers.hpp"
#include "../ASTL/IO.hpp"
#include "../ASTL/HashSet.hpp"
#include "../ASTL/Random.hpp"

#include "AssetManager.hpp"
#include "Platform.hpp"

Scene g_CurrentScene{};
const int SceneVersion = 0;

void Scene::Init()
{
    //m_MatrixNeedsUpdate = bitset_create();
}

void Scene::Destroy()
{
    for (int i = 0; i < m_LoadedSubScenes.Size(); i++)
    {
        SubScene* scene = &m_LoadedSubScenes[i];
        ParsedGLTF& data = scene->data;

        rDeleteMesh(scene->bigMesh);
    
        if (scene->textures) {
            for (int i = 0; i < data.numTextures; i++) {
                rDeleteTexture(scene->textures[i]);
            }
        }

        delete[] scene->textures;

        FreeParsedGLTF(&scene->data);
    }
    m_LoadedSubScenes.Clear();
    // bitset_free(m_MatrixNeedsUpdate);
}

void Scene::Save(const char* path)
{
    AFile file = AFileOpen(path, AOpenFlag_Write);
    AFileWrite(&SceneVersion, sizeof(int), file);
    
    int numPrefabs = m_LoadedSubScenes.m_count;
    AFileWrite(&numPrefabs, sizeof(int), file);
    
    for (int i = 0; i < numPrefabs; i++)
    {
        AFileWrite(m_LoadedSubScenes[i].path, 256, file);
    }
    
    // note: if size of scene file is too big we can only use matrices to save transforms
    int numMeshes = m_MeshInstances.m_count;
    int numLights = m_LightInstances.m_count;
    
    AFileWrite(&numMeshes, sizeof(int), file);
    AFileWrite(&numLights, sizeof(int), file);

    AFileWrite(m_MeshInstances.Data(), sizeof(MeshInstance) * numMeshes, file);
    AFileWrite(m_Matrices.Data(), sizeof(Matrix4) * numMeshes, file);
    AFileWrite(m_Bitmasks.Data(), sizeof(uint8_t) * numMeshes, file);
    
    AFileWrite(m_LightInstances.Data(), sizeof(LightInstance) * numLights, file);
    AFileWrite(&m_SunLight, sizeof(DirectionalLight), file);

    AFileClose(file);
}

void Scene::Load(const char* path)
{
    // Todo:
    AFile file = AFileOpen(path, AOpenFlag_Write);
    int sceneVersion;
    AFileRead(&sceneVersion, sizeof(int), file);
    ASSERT(sceneVersion == SceneVersion);

    int numPrefabs;
    AFileRead(&numPrefabs, sizeof(int), file);
    m_LoadedSubScenes.Resize(numPrefabs);

    for (int i = 0; i < numPrefabs; i++)
    {
        AFileRead(m_LoadedSubScenes[i].path, 256, file);
    }
    
    int numMeshInstances = 0;
    int numLightInstances = 0;
    
    AFileRead(&numMeshInstances, sizeof(int), file);
    AFileRead(&numLightInstances, sizeof(int), file);

    m_MeshInstances.Resize(numMeshInstances);
    m_Matrices.Resize(numMeshInstances);
    m_Bitmasks.Resize(numMeshInstances);
    m_LightInstances.Resize(numLightInstances);
    
    AFileRead(m_MeshInstances.Data(), sizeof(MeshInstance) * numMeshInstances, file);
    AFileRead(m_Matrices.Data(), sizeof(Matrix4) * numMeshInstances, file);
    AFileRead(m_Bitmasks.Data(), sizeof(uint8_t) * numMeshInstances, file);
    
    AFileRead(m_LightInstances.Data(), sizeof(LightInstance) * numLightInstances, file);
    AFileRead(&m_SunLight, sizeof(DirectionalLight), file);
    AFileClose(file);

    m_ScaleRotations.Resize(numMeshInstances);
    // note: maybe multithread this
    for (int i = 0; i < numMeshInstances; i++)
    {
        m_ScaleRotations[i].scale = Matrix4::ExtractScale(m_Matrices[i]);
        m_ScaleRotations[i].rotation = Matrix4::ExtractRotation(m_Matrices[i]);
    }
}

Vector3f Scene::GetMeshPosition(MeshId id)
{
    return m_Matrices[id].GetPosition();
}

void Scene::SetMeshPosition(MeshId id,  Vector3f position)
{
    return m_Matrices[id].SetPosition(position);
}

MeshId Scene::AddMesh(SubSceneID extScene, ushort sceneExtID, ushort meshIndex,
               char bitmask, const Matrix4& transformation)
{
    MeshInstance instance = { sceneExtID, meshIndex };
    m_MeshInstances.Add(instance);
    m_Matrices.Add(transformation);
    m_Bitmasks.Add(bitmask);
    m_ScaleRotations.Add({ Matrix4::ExtractScale(transformation), Matrix4::ExtractRotation(transformation) });
    return m_MeshInstances.Size()-1;
}

void Scene::RemoveMesh(MeshId id)
{
    m_MeshInstances.RemoveUnordered(id);
    m_Matrices.RemoveUnordered(id);
    m_Bitmasks.RemoveUnordered(id);
    m_ScaleRotations.RemoveUnordered(id);
}

LightId Scene::AddSpotLight(Vector3f position, Vector3f direction, int color, float intensity, float innerCutoff, float outerCutoff)
{
    m_LightInstances.AddUninitialized(1);
    LightInstance& instance = m_LightInstances.Back();
    
    instance.position  = position;
    instance.direction = direction;
    UnpackColorRGBf(color, &instance.color.x);
    instance.intensity   = intensity;
    instance.innerCutoff = innerCutoff;
    instance.outerCutoff = outerCutoff;
    return m_LightInstances.Size() -1;
}

LightId Scene::AddPointLight(Vector3f position, Vector3f direction, int color, float intensity)
{
    return AddSpotLight(position, direction, color, intensity, 0.0, 0.0);
}

void Scene::RemoveLight(MeshId id)
{
    m_LightInstances.RemoveUnordered(id);
}

int Scene::ImportSubScene(SubSceneID* sceneID, const char* inPath, float scale)
{
    // there will be many mesh instances they are going to use ushort
    ASSERT(m_LoadedSubScenes.Size() < UINT16_MAX); 
    *sceneID = m_LoadedSubScenes.Size();
    m_LoadedSubScenes.AddUninitialized(1);

    SubScene* scene = &m_LoadedSubScenes[*sceneID];

    int parsed = 1;
    char* path = scene->path;
    MemsetZero(path, sizeof(SubScene::path));
    int pathLen = StringLength(inPath);
    SmallMemCpy(path, inPath, pathLen);

    ChangeExtension(path, pathLen, "abm");
    bool firstLoad = !FileExist(path) || !IsABMLastVersion(path);
    if (firstLoad)
    {
        ASSERT(!IsAndroid());
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

    // load to GPU
    LoadSceneImages(path, scene->textures, scene->data.numImages);
    
    ParsedGLTF& data = scene->data;
    
    // create big mesh that contains all of the vertices and indices of an scene
    APrimitive primitive = data.meshes[0].primitives[0];
    primitive.indices     = data.allIndices;
    primitive.vertices    = data.allVertices;
    primitive.numIndices  = data.totalIndices;
    primitive.numVertices = data.totalVertices;
    primitive.indexType   = GraphicType_UnsignedInt;
    rCreateMeshFromPrimitive(&primitive, &scene->bigMesh);

    return parsed;
}

void Scene::UpdateSubScene(SubSceneID scene)
{
    // Scene* scene = &loadedScenes[sceneID];
    float time = (float)(2.85 + (sin(TimeSinceStartup() * 0.11) * 0.165));
    m_SunLight.dir = Vector3f::Normalize(MakeVec3(-0.20f, Abs(cosf(time)) + 0.1f, sinf(time)));
}

SubScene* Scene::GetSubScene(SubSceneID scene)
{
    return &m_LoadedSubScenes[scene];
}