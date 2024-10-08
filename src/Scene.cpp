
#include "include/Scene.hpp"

#include "../ASTL/String.hpp" // StrCmp16
#include "../ASTL/IO.hpp"
#include "../ASTL/HashSet.hpp"
#include "../ASTL/Queue.hpp"
#include "../ASTL/Random.hpp"

#include "include/AssetManager.hpp"
#include "include/Platform.hpp"
#include "include/BVH.hpp"
#include "include/TLAS.hpp"

Scene g_CurrentScene{};
const int SceneVersion = 0;

void Scene::Init()
{
    m_SunAngle = -0.16f; // -0.32f;
}

void Scene::Destroy()
{
    for (int i = 0; i < m_LoadedPrefabs.Size(); i++)
    {
        Prefab* prefab = &m_LoadedPrefabs[i];
        rDeleteMesh(prefab->bigMesh);
        delete[] prefab->globalNodeTransforms;
        delete prefab->tlas;

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
            delete[] (Vector4x32f*)prefab->animations[0].samplers[0].output;
        }

        delete[] prefab->gpuTextures;

        FreeSceneBundle((SceneBundle*)prefab);
    }
    m_LoadedPrefabs.Clear();
}

void Scene::Save(const char* path)
{
    AFile file = AFileOpen(path, AOpenFlag_WriteBinary);
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
    AFile file = AFileOpen(path, AOpenFlag_WriteBinary);
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
    
    scene->firstTimeRender = 3;
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
            ASSERTR(!FileExist(path), return 0);
            parsed &= ParseGLTF(path, (SceneBundle*)scene, scale); ASSERT(parsed);
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

        if (scene->numSkins > 0) CreateVerticesIndicesSkined((SceneBundle*)scene);
        else                     CreateVerticesIndices((SceneBundle*)scene);

        ChangeExtension(path, StringLength(path), "abm");

        // BuildBVH((SceneBundle*)scene);

        parsed &= SaveGLTFBinary((SceneBundle*)scene, path); ASSERT(parsed);
        CompressSaveSceneImages(scene, path); // save textures as binary
    }
    else
    {
        parsed = LoadSceneBundleBinary(path, (SceneBundle*)scene);
        // BuildBVH(scene);
    }

    if (!parsed)
        return 0;

    // Load to GPU
    if (scene->numImages == 0) scene->gpuTextures = nullptr; 
    else scene->gpuTextures = new Texture[scene->numImages]{};

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

            Vector4x32f minv = VecSet1(FLT_MAX);
            Vector4x32f maxv = VecSet1(FLT_MIN);

            // todo: multi thread this
            for (int v = 0; v < primitive.numVertices; v++)
            {
                Vector4x32f l = VecLoad((float*)vertices); // at the begining of the vertex we have position
                minv = VecMin(minv, l);
                maxv = VecMax(maxv, l);
                vertices += vertexSize;
            }
            VecSetW(minv, 1.0f);
            VecSetW(maxv, 1.0f);
            VecStore(primitive.min, minv);
            VecStore(primitive.max, maxv);
        }
    }
    
    scene->globalNodeTransforms = new Matrix4[scene->numNodes];
    scene->UpdateGlobalNodeTransforms(scene->GetRootNodeIdx(), Matrix4::Identity());

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

void Scene::ShowUI()
{

}

void Scene::Update()
{
    // float time = (float)(3.15 + (sin(TimeSinceStartup() * 0.11) * 0.165)); // (float)(sin(TimeSinceStartup() * 0.11));
    // m_SunLight.dir = Vector3f::Normalize(Vec3(-0.20f, Abs(Cos(time)) + 0.1f, Sin(time)));
    m_SunLight.dir = Vector3f::NormalizeEst(Vec3(-0.20f, Abs(Cos(m_SunAngle)) + 0.1f, Sin(m_SunAngle)));
    ShowUI();
}

Prefab* Scene::GetPrefab(PrefabID scene)
{
    return &m_LoadedPrefabs[scene];
}

void Prefab::UpdateGlobalNodeTransforms(int nodeIndex, Matrix4 parentMat)
{
    ANode* node = &nodes[nodeIndex];
    globalNodeTransforms[nodeIndex] = Matrix4::PositionRotationScale(node->translation, node->rotation, node->scale) * parentMat;

    for (int i = 0; i < node->numChildren; i++)
    {
        UpdateGlobalNodeTransforms(node->children[i], globalNodeTransforms[nodeIndex]);
    }
}

int Prefab::FindAnimRootNodeIndex(Prefab* prefab)
{
    if (prefab->skins == nullptr)
        return 0;

    ASkin skin = prefab->skins[0];
    if (skin.skeleton != -1) 
        return skin.skeleton;
    
    // search for Armature name, and also record the node that has most children
    int armatureIdx = -1;
    int maxChilds   = 0;
    int maxChildIdx = 0;
    // maybe recurse to find max children
    for (int i = 0; i < prefab->numNodes; i++)
    {
        if (StrCMP16(prefab->nodes[i].name, "Armature")) {
            armatureIdx = i;
            break;
        }
    
        int numChildren = prefab->nodes[i].numChildren;
        if (numChildren > maxChilds) {
            maxChilds = numChildren;
            maxChildIdx = i;
        }
    }
    
    int skeletonNode = armatureIdx != -1 ? armatureIdx : maxChildIdx;
    return skeletonNode;
}

int Prefab::FindNodeFromName(Prefab* prefab, const char* name)
{
    int len = StringLength(name);
    for (int i = 0; i < prefab->numNodes; i++)
    {
        if (StringEqual(prefab->nodes[i].name, name, len))
            return i;
    }
    AX_WARN("couldn't find node from name %s", name);
    return 0;
}
