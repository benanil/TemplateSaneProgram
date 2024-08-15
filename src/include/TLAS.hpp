#pragma once

#include "BVH.hpp"

struct TLASNode // | tlasNodes
{
    union { vec_t minv; struct { float3 aabbMin; uint leftFirst;     }; };
    union { vec_t maxv; struct { float3 aabbMax; uint instanceCount; }; };
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

// top-level BVH class
struct TLAS
{
	TLAS(struct Prefab* scene);
	~TLAS();

	void Build();
    
	struct Prefab* prefab = 0;
	TLASNode*    tlasNodes = 0; // | nodes
	BVHInstance* instances = 0; // | tris
	uint blasCount;
    
    uint numNodesUsed;

    void TraverseBVH(const Ray& ray, uint rootNode, Triout* out);

private:

    void UpdateNodeBounds(uint nodeIdx, vec_t* centeroidMinOut, vec_t* centeroidMaxOut);
    
    void RecurseBuild(TLASNode* parent, int depth);
    
    void SubdivideBVH(uint nodeIdx, uint depth, vec_t centeroidMin, vec_t centeroidMax);
    
    float FindBestSplitPlane(const TLASNode* node,
                             int* outAxis,
                             int* splitPos,
                             vec_t centeroidMin,
                             vec_t centeroidMax);
};


