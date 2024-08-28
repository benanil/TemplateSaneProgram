
#pragma once

#include "../../ASTL/Math/Matrix.hpp"

AX_NAMESPACE

struct alignas(16) BVHNode
{
    union { struct { float3 aabbMin; uint leftFirst; }; Vector4x32f minv; };
    union { struct { float3 aabbMax; uint triCount; };  Vector4x32f maxv; };
};

struct RGBA8 
{
    unsigned char r, g, b, a;
};

struct MeshInfo {
    uint numTriangles;
    uint triangleStart;
    ushort materialStart;
    ushort numMaterials;
    const char* path;
};

struct alignas(16) Tri
{
    uint v0, v1, v2, padd;
    float centeroid[4]; // 4th is padding
};

struct Triout {
    float t; // ray distance
    union {
        struct {
            float u, v;
        };
        Vector2f uv;
    };
    uint color;
    uint nodeIndex;
    uint primitiveIndex;
    uint triIndex;
    uint padd;
    Vector4x32f position;
    Vector4x32f normal;
};

constexpr int BINS = 8;

constexpr float RayacastMissDistance = 1e30f;

void InitBVH();

void DestroyBVH();

uint BuildBVH(struct SceneBundle_* prefab);

bool VECTORCALL IntersectTriangle(const Ray& ray, Vector4x32f v0, Vector4x32f v1, Vector4x32f v2, Triout* o, int i);

bool IntersectBVH(const Ray& ray, struct GPUMesh* mesh, uint rootNode, Triout* out);

// todo ignore mask, animated meshes, returning hit color
Triout RayCastFromCamera(struct CameraBase* camera, 
                         Vector2f uv, 
                         struct Scene* scene,
                         ushort prefabID, 
                         struct AnimationController* animSystem);

Triout RayCastScene(Ray ray, 
                    struct Scene* scene,
                    ushort prefabID, 
                    struct AnimationController* animSystem);

struct AABB
{ 
    union { Vector4x32f bmin; float3 bmin3; };
    union { Vector4x32f bmax; float3 bmax3; };
    
    AABB() : bmin(VecSet1(1e30f)), bmax(VecSet1(-1e30f)) {}
    
    void grow(Vector4x32f p)
    { 
        bmin = VecMin(bmin, p);
        bmax = VecMax(bmax, p);
    }

    void VECTORCALL grow(AABB other)
    {
        if (VecGetX(other.bmin) != 1e30f)
        {
            bmin = VecMin(bmin, other.bmin);
            bmax = VecMax(bmax, other.bmin);
            bmin = VecMin(bmin, other.bmax);
            bmax = VecMax(bmax, other.bmax);
        }
    }
    
    float area() 
    { 
        // Vector4x32f e = VecMask(VecSub(bmax, bmin), VecMask3); // box extent
        Vector4x32f e = VecSub(bmax, bmin); // box extent
        VecSetW(e, 0.0f);
        return VecDotf(e, VecSwizzle(e, 1, 2, 0, 3));
    }
};

forceinline void GetAABBCorners(Vector3f res[8], Vector4x32f minv, Vector4x32f maxv)
{
    Vector3f min; Vec3Store(min.arr, minv);
    Vector3f max; Vec3Store(max.arr, maxv);

    res[0] = { min.x, min.y, min.z };
    res[1] = { max.x, max.y, max.z };
    res[2] = { max.x, max.y, min.z };
    res[3] = { min.x, min.y, max.z };
    res[4] = { min.x, max.y, min.z };
    res[5] = { max.x, min.y, max.z };
    res[6] = { max.x, min.y, min.z };
    res[7] = { min.x, max.y, max.z };
}

purefn float VECTORCALL CalculateNodeCost(Vector4x32f min, Vector4x32f max, int triCount)
{ 
    Vector4x32f e = VecMask(VecSub(max, min), VecMask3); // box extent
    return triCount * VecDotf(e, VecSwizzle(e, 1, 2, 0, 3));
}

AX_END_NAMESPACE