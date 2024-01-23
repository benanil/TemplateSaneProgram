
#pragma once

#include "Renderer.hpp"
#include "Camera.hpp"

#include "../ASTL/Array.hpp"
#include "../ASTL/Math/Matrix.hpp"
#include "../External/bitset.h"

//------------------------------------------------------------------------
// subscene is GLTF, FBX or OBJ
struct SubScene
{
    ParsedGLTF data;
    Texture* textures;
    GPUMesh bigMesh; // contains all of the vertices and indices of an SubScene
    char path[256];
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
    Vector3f color;
    float intensity;
    float innerCutoff; // < zero if this is point light
    float outerCutoff; // < zero if this is point light
};

struct DirectionalLight
{
    Vector3f lightDir;
    Vector3f lightColor;
    float    lightIntensity;
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
    Array<LightInstance> m_LightInstances;
    DirectionalLight     m_SunLight;

    Array<SubScene> m_LoadedSubScenes;

    Camera m_Camera;

public:

    Scene();
    
    ~Scene();

    void Save(const char* path);

    void Load(const char* path);
    
    //------------------------------------------------------------------------
    MeshId AddMesh(SubSceneID extScene, ushort sceneExtID, ushort meshIndex,
                   char bitmask, const Matrix4& transformation);
    
    void RemoveMesh(MeshId id);
    
    Vector3f GetMeshPosition(MeshId id);
    
    void SetMeshPosition(MeshId id, Vector3f position);
    
    //------------------------------------------------------------------------
    LightId AddSpotLight(
        Vector3f position,
        Vector3f direction,
        int color,
        float intensity,
        float innerCutoff,
        float outerCutoff
    );
    
    LightId AddPointLight(
        Vector3f position,
        Vector3f direction,
        int color,
        float intensity
    );

    void RemoveLight(MeshId id);

    // import GLTF, FBX, or OBJ file into scene
    int ImportSubScene(SubSceneID* subsceneID, const char* inPath, float scale);

    void RenderSubScene(SubSceneID sceneID);

    void UpdateSubScene(SubSceneID scene);
    
    SubScene* GetSubScene(SubSceneID scene);
};

// note: we might need scene system that stores subscenes so we don't have to destroy subscenes each time we change scene

extern Scene g_CurrentScene;



