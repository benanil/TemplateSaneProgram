
#include "../ASTL/Memory.hpp"
#include "../ASTL/Algorithms.hpp"
#include "../ASTL/Additional/GLTFParser.hpp"
#include "../ASTL/Queue.hpp"

#include "../ASTL/Additional/Profiler.hpp"

#include "include/BVH.hpp"
#include "include/TLAS.hpp"
#include "include/Scene.hpp"

extern BVHNode* g_BVHNodes;

TLAS::TLAS(Prefab* scene)
{
    this->prefab = scene;
    
    int numPrimitives = 0;
    for (int m = 0; m < scene->numMeshes; m++)
    {
        numPrimitives += scene->meshes[m].numPrimitives;
    }
    
    instances = new BVHInstance[numPrimitives];
    int primitiveIndex = 0;
    
    // copy a pointer to the array of bottom level 
    Queue<int> nodeStack = {};
    nodeStack.Enqueue(scene->GetRootNodeIdx());
    
    // iterate over scene nodes
    while (!nodeStack.Empty())
    {
        int nodeIndex = nodeStack.Dequeue();
        // nodeStack.pop();
        ANode& node = scene->nodes[nodeIndex];
        AMesh amesh = scene->meshes[node.index];
        Matrix4 model = scene->globalNodeTransforms[nodeIndex];
        
        if (node.type == 0 && node.index != -1)
        for (int j = 0; j < amesh.numPrimitives; ++j)
        {
            APrimitive& primitive = amesh.primitives[j];
            if (primitive.numIndices == 0) continue;
            
            BVHInstance* instance = &instances[primitiveIndex++];
            BVHNode* bvh = &g_BVHNodes[primitive.bvhNodeIndex];
            instance->bvhIndex = primitive.bvhNodeIndex;
            instance->nodeIndex = nodeIndex;
            instance->primitiveIndex = j;
            // calculate world-space bounds using the new matrix
            instance->bounds = AABB();
        
            vec_t vmin = VecSet1(1e30f);  
            vec_t vmax = VecSet1(-1e30f); 

            // convert local Bounds to global bounds
            for (int i = 0; i < 8; i++)
            {
                vec_t point = VecSetR(i & 1 ? primitive.max[0] : primitive.min[0],
                                      i & 2 ? primitive.max[1] : primitive.min[1],
                                      i & 4 ? primitive.max[2] : primitive.min[2], 1.0f);
                point = Vector4Transform(point, model.r);
                vmin = VecMin(vmin, point);
                vmax = VecMax(vmax, point);
            }

            instance->bounds.grow(vmin);
            instance->bounds.grow(vmax);
            instance->centeroid = (instance->bounds.bmin3 + instance->bounds.bmax3) * 0.5f;
        }

        for (int j = 0; j < node.numChildren; j++)
        {
            nodeStack.Enqueue(node.children[j]);
        }
    }

    blasCount = numPrimitives;
    // allocate TLAS nodes, depth 2 binary tree
    tlasNodes = new TLASNode[blasCount * 2];
}

TLAS::~TLAS()
{
    delete[] instances;
    delete[] tlasNodes;
}

void TLAS::Build()
{
    TLASNode& root     = tlasNodes[0];
    root.leftFirst     = 0;
    root.instanceCount = blasCount;
    numNodesUsed = 0u;

    vec_t centeroidMin, centeroidMax;
    UpdateNodeBounds(numNodesUsed, &centeroidMin, &centeroidMax);
    
    SubdivideBVH(numNodesUsed++, 0, centeroidMin, centeroidMax);
}

void TLAS::UpdateNodeBounds(uint nodeIdx, vec_t* centeroidMinOut, vec_t* centeroidMaxOut)
{
    TLASNode* node = tlasNodes + nodeIdx;

    vec_t nodeMin = VecSet1(1e30f), nodeMax = VecSet1(-1e30f);
    vec_t centeroidMin = VecSet1(1e30f);
    vec_t centeroidMax = VecSet1(-1e30f);

    const BVHInstance* leafPtr = instances + node->leftFirst;

    for (uint i = 0; i < node->instanceCount; i++)
    {
        vec_t v0 = leafPtr->bounds.bmin;
        vec_t v1 = leafPtr->bounds.bmax;

        nodeMin = VecMin(nodeMin, v0);
        nodeMin = VecMin(nodeMin, v1);
        
        nodeMax = VecMax(nodeMax, v0);
        nodeMax = VecMax(nodeMax, v1);
        
        vec_t centeroid = VecLoadA(&leafPtr->centeroid.x);
        centeroidMin = VecMin(centeroidMin, centeroid);
        centeroidMax = VecMax(centeroidMax, centeroid);

        leafPtr++; // +3 for vertexPositions + 1 for (texcoords + material index) + 1 for normals
    }
    
    Vec3Store(&node->aabbMin.x, nodeMin);
    Vec3Store(&node->aabbMax.x, nodeMax);
    *centeroidMinOut = centeroidMin;
    *centeroidMaxOut = centeroidMax;
}

float TLAS::FindBestSplitPlane(const TLASNode* node, 
                               int* outAxis,
                               int* splitPos,
                               vec_t centeroidMin, 
                               vec_t centeroidMax)
{
    float bestCost = 1e30f;
    uint instanceCount = node->instanceCount, leftFirst = node->leftFirst;

    for (int axis = 0; axis < 3; ++axis)
    {
        float boundsMin = VecGetN(centeroidMin, axis);
        float boundsMax = VecGetN(centeroidMax, axis);
        
        if (boundsMax == boundsMin) continue;
        
        float scale = BINS / (boundsMax - boundsMin);
        float leftCountArea[BINS - 1], rightCountArea[BINS - 1];
        int leftSum = 0, rightSum = 0;

        struct Bin { AABB bounds; int instanceCount = 0; } bin[BINS] = {};
        for (uint i = 0; i < node->instanceCount; i++)
        {
            ASSERT(i + leftFirst < blasCount);
            BVHInstance* instance = instances + leftFirst + i;
            
            float centeroid = instance->centeroid[axis];
            int binIdx = MIN(BINS - 1, (int)((centeroid - boundsMin) * scale));
            
            ASSERT(binIdx < BINS && binIdx >= 0);
            bin[binIdx].instanceCount++;
            bin[binIdx].bounds.grow(instance->bounds.bmin);
            bin[binIdx].bounds.grow(instance->bounds.bmax);
        }
        // gather data for the 7 planes between the 8 bins
        AABB leftBox, rightBox;
        for (int i = 0; i < BINS - 1; i++)
        {
            leftSum += bin[i].instanceCount;
            leftBox.grow(bin[i].bounds);
            leftCountArea[i] = leftSum * leftBox.area();
            rightSum += bin[BINS - 1 - i].instanceCount;
            rightBox.grow(bin[BINS - 1 - i].bounds);
            rightCountArea[BINS - 2 - i] = rightSum * rightBox.area();
        }
        
        // calculate SAH cost for the 7 planes
        scale = (boundsMax - boundsMin) / BINS;
        for (int i = 0; i < BINS - 1; i++)
        {
            const float planeCost = leftCountArea[i] + rightCountArea[i];
            if (planeCost < bestCost)
            {
                *outAxis  = axis;
                *splitPos = i + 1;
                bestCost  = planeCost;
            }
        }
    
    }
    return bestCost;
}

void TLAS::SubdivideBVH(uint nodeIdx, uint depth, vec_t centeroidMin, vec_t centeroidMax)
{
    // terminate recursion
    TLASNode* node = tlasNodes + nodeIdx;
    uint leftFirst = node->leftFirst;
    uint instanceCount = node->instanceCount;
    // determine split axis and position
    int axis;
    int splitPos;
    float splitCost = FindBestSplitPlane(node, &axis, &splitPos, centeroidMin, centeroidMax);
    float nosplitCost = CalculateNodeCost(node->minv, node->maxv, node->instanceCount);
    
    if (splitCost >= nosplitCost || depth >= 12u || node->instanceCount <= 6u) return;

    // in-place partition
    uint i = leftFirst;
    uint j = i + instanceCount - 1u;
    float centeroidMinAxis = VecGetN(centeroidMin, axis);
    float centeroidMaxAxis = VecGetN(centeroidMax, axis);
    float scale = BINS / (centeroidMaxAxis - centeroidMinAxis);
    
    while (i <= j)
    {
        ASSERT(i < blasCount);
        BVHInstance* instance = instances + i;
        float centeroid = instance->centeroid[axis];
        
        int binIdx = MIN(BINS - 1, (int)((centeroid - centeroidMinAxis) * scale));
        
        if (binIdx < splitPos)
            i++;
        else {
            Swap(instances[i], instances[j]);
            j--;
        }
    }
    // abort split if one of the sides is empty
    int leftCount = i - leftFirst;
    if (leftCount == 0 || leftCount == instanceCount) return;

    // create child nodes
    uint leftChildIdx = numNodesUsed++;
    uint rightChildIdx = numNodesUsed++;
    ASSERT(rightChildIdx < numNodesUsed);

    tlasNodes[leftChildIdx].leftFirst = leftFirst;
    tlasNodes[leftChildIdx].instanceCount = leftCount;
    tlasNodes[rightChildIdx].leftFirst = i;
    tlasNodes[rightChildIdx].instanceCount = instanceCount - leftCount;
    node->leftFirst = leftChildIdx;
    node->instanceCount = 0;
    // recurse
    UpdateNodeBounds(leftChildIdx, &centeroidMin, &centeroidMax);
    SubdivideBVH(leftChildIdx, depth + 1, centeroidMin, centeroidMax);
    
    UpdateNodeBounds(rightChildIdx, &centeroidMin, &centeroidMax);
    SubdivideBVH(rightChildIdx, depth + 1, centeroidMin, centeroidMax);
}

void TLAS::TraverseBVH(const Ray& ray, uint rootNode, Triout* out)
{
    TimeBlock("TLASIntersectBVH");
    
    int nodesToVisit[32] = { (int)rootNode };
    int currentNodeIndex = 1;
    vec_t invDir = VecRcp(ray.direction);
    int protection = 0;
    
    while (currentNodeIndex > 0 && protection++ < 250)
    {
        const TLASNode* node = tlasNodes + nodesToVisit[--currentNodeIndex];
        ASSERT(node < tlasNodes + numNodesUsed);

        traverse:{}
        uint instanceCount = node->instanceCount, leftFirst = node->leftFirst;
        if (instanceCount > 0) // is leaf 
        {
            // Vector3f corners[8];
            // GetAABBCorners(corners, node->minv, node->maxv);
            // DrawLineCube(corners, ~0);

            // #pragma omp parallel for schedule(dynamic)
            for (uint i = leftFirst; i < leftFirst + instanceCount; ++i)
            {
                const BVHInstance* instance = instances + i;
                
                Ray meshRay;
                Matrix4 model = prefab->globalNodeTransforms[instance->nodeIndex];
                // change ray position & oriantation instead of mesh position for capturing in different positions
                Matrix4 inverseTransform = Matrix4::InverseTransform(model);
                meshRay.origin    = Vector4Transform(ray.origin, inverseTransform.r);
                meshRay.direction = Vector4Transform(ray.direction, inverseTransform.r);
                
                if (::IntersectBVH(meshRay, &prefab->bigMesh, instance->bvhIndex, out))
                {
                    out->nodeIndex = instance->nodeIndex;
                }
            }

            continue;
        }

        uint leftIndex = leftFirst;
        uint rightIndex = leftIndex + 1;

        TLASNode* leftNode = tlasNodes + leftIndex;
        TLASNode* rightNode = tlasNodes + rightIndex;

        float dist1 = IntersectAABB(ray.origin, invDir, leftNode->minv, leftNode->maxv, out->t);
        float dist2 = IntersectAABB(ray.origin, invDir, rightNode->minv, rightNode->maxv, out->t);

        if (dist1 > dist2) { Swap(dist1, dist2); Swap(leftIndex, rightIndex); }

        if (dist1 > out->t) dist1 = RayacastMissDistance;
        if (dist2 > out->t) dist2 = RayacastMissDistance;

        if (dist1 == RayacastMissDistance) continue;
        else {
            node = tlasNodes + leftIndex;
            if (dist2 != RayacastMissDistance)
                nodesToVisit[currentNodeIndex++] = rightIndex;
            goto traverse;
        }
    }
}

