#pragma once

#include "../../ASTL/Array.hpp"
#include "../../ASTL/Math/Matrix.hpp"
#include "Renderer.hpp"

//------------------------------------------------------------------------
// prefab is GLTF, FBX or OBJ
struct Prefab : public SceneBundle 
{
    Texture* gpuTextures;
    GPUMesh  bigMesh; // contains all of the vertices and indices of an prefab
    Matrix4* globalNodeTransforms; // pre calculated global transforms, accumulated with parents
    struct TLAS* tlas;
    char path[256]; // relative path
    int firstTimeRender; // starts with 4 and decreases until its 0 we draw first time and set this to-1

    Texture GetGPUTexture(int index)
    {
        return gpuTextures[textures[index].source];
    }

    ANode* GetNodePtr(int index)
    {
        return &nodes[index];
    }

    int GetRootNodeIdx()
    {
        int node = 0;
        if (numScenes > 0) {
            AScene defaultScene = scenes[defaultSceneIndex];
            node = defaultScene.nodes[0];
        }
        return node;
    }

    void UpdateGlobalNodeTransforms(int rootNodeIdx, Matrix4 parentMat);
    
    static int FindAnimRootNodeIndex(Prefab* prefab);
    static int FindNodeFromName(Prefab* prefab, const char* name);
};

typedef ushort PrefabID;

//------------------------------------------------------------------------

struct MeshInstance
{
    ushort sceneExtIndex; // index of prefab that contained this mesh
    ushort meshIndex; // mesh index in gltf scene
};

// point and spot light in same structure
struct LightInstance
{
    Vector3f position;
    Vector3f direction; 
    uint  color;
    float intensity;
    float cutoff; // < angle of SpotLight between 0.01-1.0, zero if this is point light.
    float range; // < how far light can reach
};

struct DirectionalLight
{
    Vector3f dir;
    Vector3f color;
    float    intensity;
};

typedef int MeshId;
typedef int LightId;

struct ScaleRotation
{
    Vector3f scale;
    Quaternion rotation;
};

struct Scene
{
    // Transformations of the meshes
    // one of these[meshId] gives the data
    Array<Matrix4>       m_Matrices; // matrices are seperate because we can use with instancing
    Array<ScaleRotation> m_ScaleRotations;

    // Meshes
    Array<uint8_t>       m_Bitmasks; // indicates to entity mask. user can use this as wood, enemy, stone, metal etc.
    Array<MeshInstance>  m_MeshInstances;
    // lights
    Array<LightInstance> m_PointLights;
    Array<LightInstance> m_SpotLights;
    DirectionalLight     m_SunLight;
    float m_SunAngle;

    Array<Prefab> m_LoadedPrefabs;
    static const int IsPointMask = 0x80000000;

    //------------------------------------------------------------------------
    void Init();

    void Destroy();

    void Save(const char* path);

    void Load(const char* path);
    
    //------------------------------------------------------------------------
    MeshId AddMesh(PrefabID prefabId, ushort meshIndex,
                   char bitmask, const Matrix4& transformation);
    
    // add copy of prefab into the scene, this will convert AScene and ANode's to Node and Scene's
    void AddPrefab(PrefabID prefabId, const Matrix4& transformation);

    void RemoveMesh(MeshId id);
    
    Vector3f GetMeshPosition(MeshId id);
    
    void SetMeshPosition(MeshId id, Vector3f position);
    
    //------------------------------------------------------------------------
    LightId AddLight(LightInstance& instance);

    LightId AddSpotLight(
        Vector3f position,
        Vector3f direction,
        int color,
        float intensity,
        float cutoff,
        float range
    );
    
    LightId AddPointLight(
        Vector3f position,
        int color,
        float intensity,
        float range
    );

    void UpdateLight(LightId id);

    void RemoveLight(LightId id);

    // import GLTF, FBX, or OBJ file into scene
    int ImportPrefab(PrefabID* prefabID, const char* inPath, float scale);

    void Update();

    Prefab* GetPrefab(PrefabID prefab);

private:

    void ShowUI();

    LightId AddLight(Array<LightInstance>& array, Vector3f position, Vector3f direction, int color, float intensity, float cutoff, float range);
};

// note: we might need scene system that stores subscenes so we don't have to destroy subscenes each time we change scene

extern Scene g_CurrentScene;
















