#pragma once

// game build doesn't have astc encoder, ufbx, dxt encoder. 
// because we are only decoding when we release the game
// if true, reduces exe size and you will have faster compile times.
// also it uses zstddeclib instead of entire zstd. (only decompression in game builds) go to CMakeLists.txt for more details
#if defined(__ANDROID__)
    #define AX_GAME_BUILD 1
#else
    #define AX_GAME_BUILD 1 /* make zero for editor build */
#endif

#include "../ASTL/Array.hpp"
#include "../ASTL/Math/Matrix.hpp"
#include "../External/bitset.h"
#include "Renderer.hpp"

//------------------------------------------------------------------------
// subscene is GLTF, FBX or OBJ
struct SubScene
{
    ParsedGLTF data;
    Texture* textures;
    GPUMesh bigMesh; // contains all of the vertices and indices of an SubScene
    char path[128]; // relative path
};

typedef ushort SubSceneID;

//------------------------------------------------------------------------

struct MeshInstance
{
    ushort sceneExtIndex; // index of SubScene that contained this mesh
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
public:
    // Transformations of the meshes
    // one of these[meshId] gives the data
    Array<Matrix4>       m_Matrices; // matrices are seperate because we can use with instancing
    Array<ScaleRotation> m_ScaleRotations;
    bitset_t*            m_MatrixNeedsUpdate;

    // Meshes
    Array<uint8_t>       m_Bitmasks; // indicates to entitys mask. user can use this as wood, enemy, stone, metal etc.
    Array<MeshInstance>  m_MeshInstances;
    // lights
    Array<LightInstance> m_PointLights;
    Array<LightInstance> m_SpotLights;
    DirectionalLight     m_SunLight;

    Array<SubScene> m_LoadedSubScenes;
    static const int IsPointMask = 0x80000000;

public:
    
    void Init();

    void Destroy();

    void Save(const char* path);

    void Load(const char* path);
    
    //------------------------------------------------------------------------
    MeshId AddMesh(SubSceneID extScene, ushort sceneExtID, ushort meshIndex,
                   char bitmask, const Matrix4& transformation);
    
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
    int ImportSubScene(SubSceneID* subsceneID, const char* inPath, float scale);

    void UpdateSubScene(SubSceneID scene);
    
    SubScene* GetSubScene(SubSceneID scene);

private:

    LightId AddLight(Array<LightInstance>& array, Vector3f position, Vector3f direction, int color, float intensity, float cutoff, float range);
};

// note: we might need scene system that stores subscenes so we don't have to destroy subscenes each time we change scene

extern Scene g_CurrentScene;








