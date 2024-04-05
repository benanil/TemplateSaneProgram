
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
    m_MatrixNeedsUpdate = bitset_create();
}

void Scene::Destroy()
{
    for (int i = 0; i < m_LoadedPrefabs.Size(); i++)
    {
        Prefab* prefab = &m_LoadedPrefabs[i];
        rDeleteMesh(prefab->bigMesh);
    
        if (prefab->gpuTextures) {
            for (int i = 0; i < prefab->numTextures; i++) {
                rDeleteTexture(prefab->gpuTextures[i]);
            }
        }

        for (int s = 0; s < prefab->numSkins; s++)
        {
            delete[] prefab->skins[s].inverseBindMatrices;
        }

        if (prefab->numAnimations > 0)
        {
            // all of the sampler input and outputs are allocated in one buffer.
            // at the end of the CreateVerticesIndicesSkined function
            delete[] prefab->animations[0].samplers[0].input;
            delete[] (vec_t*)prefab->animations[0].samplers[0].output;
        }

        delete[] prefab->gpuTextures;

        FreeSceneBundle((SceneBundle*)prefab);
    }
    m_LoadedPrefabs.Clear();
    if (m_MatrixNeedsUpdate) bitset_free(m_MatrixNeedsUpdate);
}

void Scene::Save(const char* path)
{
    AFile file = AFileOpen(path, AOpenFlag_Write);
    AFileWrite(&SceneVersion, sizeof(int), file);
    
    int numPrefabs = m_LoadedPrefabs.m_count;
    AFileWrite(&numPrefabs, sizeof(int), file);
    
    for (int i = 0; i < numPrefabs; i++)
    {
        AFileWrite(m_LoadedPrefabs[i].path, 256, file);
    }
    
    // note: if size of scene file is too big we can only use matrices to save transforms
    int numMeshes = m_MeshInstances.m_count;
    int numLights = m_PointLights.m_count;
    
    AFileWrite(&numMeshes, sizeof(int), file);
    AFileWrite(&numLights, sizeof(int), file);

    AFileWrite(m_MeshInstances.Data(), sizeof(MeshInstance) * numMeshes, file);
    AFileWrite(m_Matrices.Data(), sizeof(Matrix4) * numMeshes, file);
    AFileWrite(m_Bitmasks.Data(), sizeof(uint8_t) * numMeshes, file);
    
    AFileWrite(m_PointLights.Data(), sizeof(LightInstance) * numLights, file);
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
    m_LoadedPrefabs.Resize(numPrefabs);

    for (int i = 0; i < numPrefabs; i++)
    {
        AFileRead(m_LoadedPrefabs[i].path, 256, file);
    }
    
    int numMeshInstances = 0;
    int numLightInstances = 0;
    
    AFileRead(&numMeshInstances, sizeof(int), file);
    AFileRead(&numLightInstances, sizeof(int), file);

    m_MeshInstances.Resize(numMeshInstances);
    m_Matrices.Resize(numMeshInstances);
    m_Bitmasks.Resize(numMeshInstances);
    m_PointLights.Resize(numLightInstances);
    
    AFileRead(m_MeshInstances.Data(), sizeof(MeshInstance) * numMeshInstances, file);
    AFileRead(m_Matrices.Data(), sizeof(Matrix4) * numMeshInstances, file);
    AFileRead(m_Bitmasks.Data(), sizeof(uint8_t) * numMeshInstances, file);
    
    AFileRead(m_PointLights.Data(), sizeof(LightInstance) * numLightInstances, file);
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

MeshId Scene::AddMesh(PrefabID prefabId, ushort meshIndex, char bitmask, const Matrix4& transformation)
{
    MeshInstance instance = { prefabId, meshIndex };
    m_MeshInstances.Add(instance);
    m_Matrices.Add(transformation);
    m_Bitmasks.Add(bitmask);
    m_ScaleRotations.Add({ Matrix4::ExtractScale(transformation), Matrix4::ExtractRotation(transformation) });
    return m_MeshInstances.Size()-1;
}

void Scene::AddPrefab(PrefabID prefabId, const Matrix4& transformation)
{
    // todo: NOT DONE Scene::AddPrefab
 
    // maybe return added root node index.
}

void Scene::RemoveMesh(MeshId id)
{
    m_MeshInstances.RemoveUnordered(id);
    m_Matrices.RemoveUnordered(id);
    m_Bitmasks.RemoveUnordered(id);
    m_ScaleRotations.RemoveUnordered(id);
}

LightId Scene::AddLight(Array<LightInstance>& array, Vector3f position, Vector3f direction,
                        int color, float intensity, float cutoff, float range)
{
    array.AddUninitialized(1);
    LightInstance& instance = array.Back();
    instance.position  = position;
    instance.direction = direction;
    instance.color     = color;
    instance.intensity = intensity;
    instance.cutoff    = cutoff;
    instance.range     = range;
    int isPointBit = (int)(cutoff == 0.0f) * IsPointMask;
    return (array.Size() -1) | isPointBit;
}

LightId Scene::AddLight(LightInstance& instance)
{
    bool isPointLight = instance.cutoff == 0.0f;
    Array<LightInstance>& array = isPointLight ? m_PointLights : m_SpotLights;
    int isPointBit = (int)isPointLight * IsPointMask;
    array.Add(instance);
    return (array.Size() -1) | isPointBit;
}

LightId Scene::AddSpotLight(Vector3f position, Vector3f direction, int color, float intensity, float cutoff, float range)
{
    return AddLight(m_SpotLights, position, direction, color, intensity, cutoff, range);
}

LightId Scene::AddPointLight(Vector3f position, int color, float intensity, float range)
{
    return AddLight(m_PointLights, position, Vector3f::Zero(), color, intensity, 0.0f, range);
}

void Scene::UpdateLight(LightId id)
{
    // todo: implement Scene::UpdateLight
}

void Scene::RemoveLight(MeshId id)
{
    Array<LightInstance>& lights = !!(id & IsPointMask) ? m_PointLights : m_SpotLights;
    // remove sign bit
    lights.RemoveUnordered(id & 0x7FFFFFFF);
}

int Scene::ImportPrefab(PrefabID* sceneID, const char* inPath, float scale)
{
    // There will be many mesh instances they are going to use ushort
    ASSERT(m_LoadedPrefabs.Size() < UINT16_MAX); 
    *sceneID = m_LoadedPrefabs.Size();
    m_LoadedPrefabs.AddUninitialized(1);

    Prefab* scene = &m_LoadedPrefabs[*sceneID];
    MemsetZero(scene, sizeof(Prefab));
    
    scene->firstTimeRender = true;
    int parsed = 1;
    char* path = scene->path;
    int pathLen = StringLength(inPath);
    SmallMemCpy(path, inPath, pathLen);
    
    bool isGLTF = FileHasExtension(path, pathLen, "gltf");
    bool isFBX  = FileHasExtension(path, pathLen, "fbx");
    bool isOBJ  = FileHasExtension(path, pathLen, "obj");

    ChangeExtension(path, pathLen, "abm");
    bool firstLoad = !IsABMLastVersion(path);

    if (firstLoad)
    {
        if_constexpr (IsAndroid()) {
            AX_ERROR("file is not exist, or version does not match: %s", path);
        }

        pathLen = StringLength(path);

        if (isGLTF) {
            ChangeExtension(path, pathLen, "gltf");
            parsed &= ParseGLTF(path, (SceneBundle*)scene, scale); ASSERT(parsed);
            
            if (scene->numSkins > 0) CreateVerticesIndicesSkined((SceneBundle*)scene);
            else                     CreateVerticesIndices((SceneBundle*)scene);
        }
        else if (isOBJ) {
            // todo: make scene bundle from obj
            ChangeExtension(path, pathLen, "obj");
            ASSERT(0);
        }
        else if (isFBX) {
            ChangeExtension(path, pathLen, "fbx");
            parsed &= LoadFBX(path, (SceneBundle*)scene, scale);
        }

        ChangeExtension(path, StringLength(path), "abm");
        parsed &= SaveGLTFBinary((SceneBundle*)scene, path); ASSERT(parsed);
        SaveSceneImages(scene, path); // save textures as binary
    }
    else
    {
        parsed = LoadGLTFBinary(path, (SceneBundle*)scene);
    }

    if (!parsed)
        return 0;

    // Load to GPU
    LoadSceneImages(path, scene->gpuTextures, scene->numImages);
    
    // Load AABB's
    for (int i = 0; i < scene->numMeshes; i++)
    {
        AMesh mesh = scene->meshes[i];
        for (int j = 0; j < mesh.numPrimitives; j++)
        {
            APrimitive& primitive = mesh.primitives[j];
            bool hasSkin = EnumHasBit(primitive.attributes, AAttribType_JOINTS) && 
                           EnumHasBit(primitive.attributes, AAttribType_WEIGHTS);

            uint64_t vertexSize = hasSkin ? sizeof(ASkinedVertex) : sizeof(AVertex);
            char* vertices = (char*)primitive.vertices;

            vec_t minv = VecSet1(FLT_MAX);
            vec_t maxv = VecSet1(FLT_MIN);

            for (int v = 0; v < primitive.numVertices; v++)
            {
                vec_t l = VecLoad((float*)vertices); // at the begining of the vertex we have position
                minv = VecMin(minv, l);
                maxv = VecMax(maxv, l);
                vertices += vertexSize;
            }
            VecStore(primitive.min, minv);
            VecStore(primitive.max, maxv);
        }
    }
    
    // create big mesh that contains all of the vertices and indices of an scene
    APrimitive primitive  = scene->meshes[0].primitives[0];
    primitive.indices     = scene->allIndices;
    primitive.vertices    = scene->allVertices;
    primitive.numIndices  = scene->totalIndices;
    primitive.numVertices = scene->totalVertices;
    primitive.indexType   = GraphicType_UnsignedInt;
    bool isSkined = (bool)(scene->skins != nullptr);
    rCreateMeshFromPrimitive(&primitive, &scene->bigMesh, isSkined);
    return parsed;
}

void Scene::Update()
{
    float time = (float)(3.15 + (sin(TimeSinceStartup() * 0.11) * 0.165)); // (float)(sin(TimeSinceStartup() * 0.11)); 
    m_SunLight.dir = Vector3f::Normalize(MakeVec3(-0.20f, Abs(Cos(time)) + 0.1f, Sin(time)));
    // m_SunLight.dir = MakeVec3(0.1f, 0.8f, 0.05f);
}

Prefab* Scene::GetPrefab(PrefabID scene)
{
    return &m_LoadedPrefabs[scene];
}