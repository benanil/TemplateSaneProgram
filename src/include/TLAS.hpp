#pragma once

#include "BVH.hpp"

struct TLASNode // | tlasNodes
{
    union { Vector4x32f minv; struct { float3 aabbMin; uint leftFirst;     }; };
    union { Vector4x32f maxv; struct { float3 aabbMax; uint instanceCount; }; };
};

// instance of a BVH, with transform and world bounds
struct alignas(16) BVHInstance // Tri
{
	AABB bounds; // in world space | triangles
    float3 centeroid; uint padd0;
    uint bvhIndex;
	uint nodeIndex;
    uint primitiveIndex; // returns which primitive of the node
    uint padd2;
};

struct BVHInstanceGPU
{
    uint nodeIndex;
    uint bvhIndex;
};

// top-level BVH class
struct TLAS
{
    TLAS(struct Prefab* scene);
    ~TLAS();
    
    void Build();
    
    struct Prefab* prefab = 0;
    TLASNode*    tlasNodes = 0; // | nodes
    BVHInstance* instances = 0; // | tris
    BVHInstanceGPU* instancesGPU = 0; // | tris
    uint blasCount;
    
    uint numNodesUsed;

    void TraverseBVH(const Ray& ray, uint rootNode, Triout* out);

private:

    void UpdateNodeBounds(uint nodeIdx, Vector4x32f* centeroidMinOut, Vector4x32f* centeroidMaxOut);
    
    void RecurseBuild(TLASNode* parent, int depth);
    
    void SubdivideBVH(uint nodeIdx, uint depth, Vector4x32f centeroidMin, Vector4x32f centeroidMax);
    
    float FindBestSplitPlane(const TLASNode* node,
                             int* outAxis,
                             int* splitPos,
                             Vector4x32f centeroidMin,
                             Vector4x32f centeroidMax);
};


