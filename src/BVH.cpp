
// Bounding Volume Hierarchy data structure for acceleration
// you can see example code for raycasting at the end of this file

#include "include/BVH.hpp"
#include "include/Renderer.hpp"
#include "include/Scene.hpp"
#include "include/Platform.hpp"

#include "../ASTL/Math/Matrix.hpp"
#include "../ASTL/Algorithms.hpp"
#include "../ASTL/Queue.hpp"

#include "include/BVH.hpp"
#include "include/Renderer.hpp"
#include "include/Camera.hpp"
#include "include/Scene.hpp"
#include "include/Animation.hpp"
#include "include/TLAS.hpp"

#include "../ASTL/Additional/Profiler.hpp"

const size_t MAX_TRIANGLES = 2048 * 2048; // 4'194'304; // more than bistro num triangles * 2

uint g_TotalBVHNodesUsed = 0;
uint g_CurrTriangleFace = 0;

BVHNode*  g_BVHNodes;
Tri*      g_Triangles;
Vector3f* centeroids; // 4th is padding


void InitBVH()
{
    g_TotalBVHNodesUsed = 0;
    g_CurrTriangleFace   = 0;
    centeroids  = new Vector3f[MAX_TRIANGLES];
    g_Triangles = new Tri[MAX_TRIANGLES]{};
    g_BVHNodes  = new BVHNode[MAX_TRIANGLES]{};
}

void DestroyBVH()
{
    delete[] g_Triangles;
    delete[] g_BVHNodes;
    delete[] centeroids;
}

static void UpdateNodeBounds(SceneBundle* mesh, uint nodeIdx, Vector4x32f* centeroidMinOut, Vector4x32f* centeroidMaxOut)
{
    BVHNode* node = g_BVHNodes + nodeIdx;
    Vector4x32f nodeMin = VecSet1(1e30f), nodeMax = VecSet1(-1e30f);
    
    Vector4x32f centeroidMin = VecSet1(1e30f);
    Vector4x32f centeroidMax = VecSet1(-1e30f);

    const Tri* leafPtr = g_Triangles + node->leftFirst;
    int stride = mesh->numSkins > 0 ? sizeof(ASkinedVertex) : sizeof(AVertex);

    for (uint i = 0; i < node->triCount; i++)
    {
        Vector4x32f v0 = VecLoad((const float*)((char*)mesh->allVertices + (leafPtr->v0 * stride)));
        Vector4x32f v1 = VecLoad((const float*)((char*)mesh->allVertices + (leafPtr->v1 * stride)));
        Vector4x32f v2 = VecLoad((const float*)((char*)mesh->allVertices + (leafPtr->v2 * stride)));

        nodeMin = VecMin(nodeMin, v0);
        nodeMin = VecMin(nodeMin, v1);
        nodeMin = VecMin(nodeMin, v2);
        
        nodeMax = VecMax(nodeMax, v0);
        nodeMax = VecMax(nodeMax, v1);
        nodeMax = VecMax(nodeMax, v2);
        
        Vector4x32f centeroid = VecLoad(centeroids[node->leftFirst + i].arr);
        centeroidMin = VecMin(centeroidMin, centeroid);
        centeroidMax = VecMax(centeroidMax, centeroid);

        leafPtr++; // +3 for vertexPositions + 1 for (texcoords + material index) + 1 for normals
    }
    
    Vec3Store(&node->aabbMin.x, nodeMin);
    Vec3Store(&node->aabbMax.x, nodeMax);
    *centeroidMinOut = centeroidMin;
    *centeroidMaxOut = centeroidMax;
}

static float FindBestSplitPlane(const BVHNode* node, 
                                SceneBundle* mesh,
                                APrimitive* primitive,
                                int* outAxis,
                                int* splitPos,
                                Vector4x32f centeroidMin, 
                                Vector4x32f centeroidMax)
{
    float bestCost = 1e30f;
    uint triCount = node->triCount, leftFirst = node->leftFirst;
    int stride = mesh->numSkins > 0 ? sizeof(ASkinedVertex) : sizeof(AVertex);

    for (int axis = 0; axis < 3; ++axis)
    {
        float boundsMin = VecGetN(centeroidMin, axis);
        float boundsMax = VecGetN(centeroidMax, axis);
        
        if (boundsMax == boundsMin) continue;
        
        float scale = BINS / (boundsMax - boundsMin);
        float leftCountArea[BINS - 1], rightCountArea[BINS - 1];
        int leftSum = 0, rightSum = 0;

        struct Bin { AABB bounds; int triCount = 0; } bin[BINS] = {};
        for (uint i = 0; i < node->triCount; i++)
        {
            int triIdx = leftFirst + i;
            ASSERT(triIdx < MAX_TRIANGLES);
            Tri* triangle = g_Triangles + triIdx;
            
            float centeroid = centeroids[triIdx][axis];
            int binIdx = MIN(BINS - 1, (int)((centeroid - boundsMin) * scale));
            
            ASSERT(binIdx < BINS && binIdx >= 0);
            bin[binIdx].triCount++;
            AABB& bounds = bin[binIdx].bounds; // .grow(mesh, triangle, stride);

            Vector4x32f v0 = VecLoad((const float*)((char*)mesh->allVertices + (triangle->v0 * stride)));
            Vector4x32f v1 = VecLoad((const float*)((char*)mesh->allVertices + (triangle->v1 * stride)));
            Vector4x32f v2 = VecLoad((const float*)((char*)mesh->allVertices + (triangle->v2 * stride)));

            bounds.bmin = VecMin(bounds.bmin, v0);
            bounds.bmin = VecMin(bounds.bmin, v1); 
            bounds.bmin = VecMin(bounds.bmin, v2);

            bounds.bmax = VecMax(bounds.bmax, v0);
            bounds.bmax = VecMax(bounds.bmax, v1);
            bounds.bmax = VecMax(bounds.bmax, v2);
        }
        // gather data for the 7 planes between the 8 bins
        AABB leftBox, rightBox;
        for (int i = 0; i < BINS - 1; i++)
        {
            leftSum += bin[i].triCount;
            leftBox.grow(bin[i].bounds);
            leftCountArea[i] = leftSum * leftBox.area();
            rightSum += bin[BINS - 1 - i].triCount;
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

static void SubdivideBVH(uint depth, SceneBundle* mesh, APrimitive* primitive, uint nodeIdx, Vector4x32f centeroidMin, Vector4x32f centeroidMax)
{
    // terminate recursion
    BVHNode* node = g_BVHNodes + nodeIdx;
    uint leftFirst = node->leftFirst;
    uint triCount = node->triCount;
    // determine split axis and position
    int axis;
    int splitPos;
    float splitCost = FindBestSplitPlane(node, mesh, primitive, &axis, &splitPos, centeroidMin, centeroidMax);
    float nosplitCost = CalculateNodeCost(node->minv, node->maxv, node->triCount);
    
    if (splitCost >= nosplitCost) return; // || depth >= 20 || node->triCount <= 24

    // in-place partition
    int i = leftFirst;
    int j = i + triCount - 1;
    float centeroidMinAxis = VecGetN(centeroidMin, axis);
    float centeroidMaxAxis = VecGetN(centeroidMax, axis);
    float scale = BINS / (centeroidMaxAxis - centeroidMinAxis);
    
    int stride = mesh->numSkins > 0 ? sizeof(ASkinedVertex) : sizeof(AVertex);

    ASSERT(j < MAX_TRIANGLES && i < MAX_TRIANGLES);

    while (i <= j)
    {
        ASSERT(i < MAX_TRIANGLES);
        Tri* tri = g_Triangles + i;
        float centeroid = centeroids[i][axis];
        
        int binIdx = MIN(BINS - 1, (int)((centeroid - centeroidMinAxis) * scale));
        
        if (binIdx < splitPos)
            i++;
        else {
            Swap(g_Triangles[i], g_Triangles[j]);
            j--;
        }
    }
    // abort split if one of the sides is empty
    int leftCount = i - leftFirst;
    if (leftCount == 0 || leftCount == triCount) return;
    // create child nodes
    uint leftChildIdx = g_TotalBVHNodesUsed++;
    uint rightChildIdx = g_TotalBVHNodesUsed++;
    ASSERT(rightChildIdx < MAX_TRIANGLES);
    g_BVHNodes[leftChildIdx].leftFirst = leftFirst;
    g_BVHNodes[leftChildIdx].triCount = leftCount;
    g_BVHNodes[rightChildIdx].leftFirst = i;
    g_BVHNodes[rightChildIdx].triCount = triCount - leftCount;
    node->leftFirst = leftChildIdx;
    node->triCount = 0;
    // recurse
    UpdateNodeBounds(mesh, leftChildIdx, &centeroidMin, &centeroidMax);
    SubdivideBVH(depth + 1, mesh, primitive, leftChildIdx, centeroidMin, centeroidMax);
    
    UpdateNodeBounds(mesh, rightChildIdx, &centeroidMin, &centeroidMax);
    SubdivideBVH(depth + 1, mesh, primitive, rightChildIdx, centeroidMin, centeroidMax);
}

uint BuildBVH(SceneBundle* prefab)
{
    Tri* tris = g_Triangles + g_CurrTriangleFace;
    const Tri* triEnd = g_Triangles + MAX_TRIANGLES;

    int totalTriangles = 0;
    int stride = prefab->numSkins > 0 ? sizeof(ASkinedVertex) : sizeof(AVertex);

    uint* indices = (uint*)prefab->allIndices;
    char* vertices = (char*)prefab->allVertices;

    Tri* tri = tris;
    Vector3f* centeroid = centeroids + g_CurrTriangleFace;
    // for each primitive: calculate centroids and create tris for bvh
    for (int m = 0; m < prefab->numMeshes; m++)
    {
        AMesh* mesh = prefab->meshes + m;
    
        for (int pr = 0; pr < mesh->numPrimitives; pr++)
        {
            APrimitive* primitive = mesh->primitives + pr;
            int numTriangles = primitive->numIndices / 3;
    
            uint indexStart = primitive->indexOffset;

            // calculate triangle centroids for partitioning
            for (int tr = 0; tr < numTriangles; tr++) 
            {
                tri->v0 = indices[indexStart + (tr * 3) + 0];
                tri->v1 = indices[indexStart + (tr * 3) + 1];
                tri->v2 = indices[indexStart + (tr * 3) + 2];
    
                // set centeroids
                Vector4x32f v0 = VecLoad((const float*)(vertices + (tri->v0 * stride)));
                Vector4x32f v1 = VecLoad((const float*)(vertices + (tri->v1 * stride)));
                Vector4x32f v2 = VecLoad((const float*)(vertices + (tri->v2 * stride)));
    
                centeroid->x = (VecGetX(v0) + VecGetX(v1) + VecGetX(v2)) * 0.333333f;
                centeroid->y = (VecGetY(v0) + VecGetY(v1) + VecGetY(v2)) * 0.333333f;
                centeroid->z = (VecGetZ(v0) + VecGetZ(v1) + VecGetZ(v2)) * 0.333333f;
    
                tri++;
                centeroid++;
                ASSERT(tri < triEnd);
            }
    
            totalTriangles += numTriangles;
        }   
    }

    uint nodesUsedStart = g_TotalBVHNodesUsed;

    // create bvh for each primitive
    for (int m = 0; m < prefab->numMeshes; m++)
    {
        AMesh* mesh = prefab->meshes + m;
    
        for (int pr = 0; pr < mesh->numPrimitives; pr++)
        {
            APrimitive* primitive = mesh->primitives + pr;
            int numTriangles = primitive->numIndices / 3;
            
            // assign all triangles to root node
            uint rootNodeIndex = g_TotalBVHNodesUsed++;
    
            BVHNode& root  = g_BVHNodes[rootNodeIndex];
            root.leftFirst = g_CurrTriangleFace;
            root.triCount  = numTriangles;
    
            primitive->bvhNodeIndex = rootNodeIndex;

            Vector4x32f centeroidMin, centeroidMax;
            UpdateNodeBounds(prefab, rootNodeIndex, &centeroidMin, &centeroidMax);
    
            // subdivide recursively
            SubdivideBVH(0, prefab, primitive, rootNodeIndex, centeroidMin, centeroidMax);
            g_CurrTriangleFace += numTriangles;
        } 
    }
    
    return g_TotalBVHNodesUsed - nodesUsedStart;
}

purefn bool VECTORCALL IntersectTriangle(const Ray& ray, Vector4x32f v0, Vector4x32f v1, Vector4x32f v2, Triout* o, int i)
{
    Vector4x32f edge1 = VecSub(v1, v0);
    Vector4x32f edge2 = VecSub(v2, v0);

    Vector4x32f h = Vec3Cross(ray.direction, edge2);
    float a = Vec3Dotf(edge1, h);
    bool fail = false; // Abs(a) < 0.0001f; // ray parallel to triangle

    float f = 1.0f / a;
    Vector4x32f s = VecSub(ray.origin, v0);
    float u = f * Vec3Dotf(s, h);
    fail |= (u < 0.0f) | (u > 1.0f);

    Vector4x32f q = Vec3Cross(s, edge1);
    float v = f * Vec3Dotf(ray.direction, q);
    float t = f * Vec3Dotf(edge2, q);
    fail |= (v < 0.0f) | (u + v > 1.0f);

    // if passed we are going to draw triangle
    if (!fail & (t > 0.0001f) & (t < o->t))
    {
        o->u = u;
        o->v = v;
        o->t = t;
        o->triIndex = i;
        return true;
    }
    return false;
}

bool IntersectBVH(const Ray& ray, GPUMesh* mesh, uint rootNode, Triout* out)
{
    TimeBlock("IntersectBVH");

    int nodesToVisit[32] = { (int)rootNode };
    int currentNodeIndex = 1;
    Vector4x32f invDir = VecRcp(ray.direction);
    bool intersection = 0;
    int protection = 0;
    
    while (currentNodeIndex > 0 && protection++ < 250)
    {
        const BVHNode* node = g_BVHNodes + nodesToVisit[--currentNodeIndex];
        ASSERT(node < g_BVHNodes + g_TotalBVHNodesUsed);

    traverse:
        uint triCount = node->triCount, leftFirst = node->leftFirst;
        if (triCount > 0) // is leaf 
        {
            for (uint i = leftFirst; i < leftFirst + triCount; ++i)
            {
                const Tri* tri = g_Triangles + i;
                ASSERT(tri < g_Triangles + g_CurrTriangleFace);

                Vector4x32f v0 = mesh->GetPosition(tri->v0);
                Vector4x32f v1 = mesh->GetPosition(tri->v1);
                Vector4x32f v2 = mesh->GetPosition(tri->v2);
                intersection |= IntersectTriangle(ray, v0, v1, v2, out, i);
            }

            continue;
        }

        uint leftIndex = leftFirst;
        uint rightIndex = leftIndex + 1;
        ASSERT(rightIndex < g_CurrTriangleFace);

        BVHNode leftNode  = g_BVHNodes[leftIndex];
        BVHNode rightNode = g_BVHNodes[rightIndex];

        float dist1 = IntersectAABB(ray.origin, invDir, leftNode.minv, leftNode.maxv, out->t);
        float dist2 = IntersectAABB(ray.origin, invDir, rightNode.minv, rightNode.maxv, out->t);

        if (dist1 > dist2) { Swap(dist1, dist2); Swap(leftIndex, rightIndex); }

        if (dist1 > out->t) dist1 = RayacastMissDistance;
        if (dist2 > out->t) dist2 = RayacastMissDistance;

        if (dist1 == RayacastMissDistance) continue;
        else {
            node = g_BVHNodes + leftIndex;
            if (dist2 != RayacastMissDistance)
                nodesToVisit[currentNodeIndex++] = rightIndex;
            goto traverse;
        }
    }
    return intersection;
}

purefn int SampleTexture(Texture texture, float2 uv)
{
    uv -= Vec2(Floor(uv.x), Floor(uv.y));
    int uScaled = (int)(texture.width * uv.x);  // (0, 1) to (0, TextureWidth )
    int vScaled = (int)(texture.height * uv.y); // (0, 1) to (0, TextureHeight)
    return vScaled * texture.width + uScaled;
}

purefn int SampleSkyboxPixel(float3 rayDirection, Texture texture)
{
    int theta = (int)((ATan2(rayDirection.x, -rayDirection.z) / PI) * 0.5f * (float)(texture.width));
    int phi = (int)((ACos(rayDirection.y) / PI) * (float)(texture.height));
    return (phi * texture.width) + theta + 2;
}

Triout RayCastScene(Ray ray,
                    Scene* scene,
                    ushort prefabID,
                    AnimationController* animSystem)
{
    Prefab* prefab = scene->GetPrefab(prefabID);
    VecSetW(ray.origin, 1.0);
    VecSetW(ray.direction, 0.0);

    Triout hitOut = {};
	hitOut.t = RayacastMissDistance;
 
    prefab->tlas->TraverseBVH(ray, 0, &hitOut);

    if (hitOut.t == RayacastMissDistance) {
        return hitOut; // maybe return sky color
    }

    int hitNodeIdx = hitOut.nodeIndex;
    struct Triangle 
    {
        Vector4x32f pos[3];
        Vector4x32f normal[3];
        Vector2f uv[3];
    };
    
    Triangle triangle;
    Tri tri = g_Triangles[hitOut.triIndex];

    triangle.pos[0] = prefab->bigMesh.GetPosition(tri.v0);
    triangle.pos[1] = prefab->bigMesh.GetPosition(tri.v1);
    triangle.pos[2] = prefab->bigMesh.GetPosition(tri.v2);

    triangle.normal[0] = prefab->bigMesh.GetNormal(tri.v0);
    triangle.normal[1] = prefab->bigMesh.GetNormal(tri.v1);
    triangle.normal[2] = prefab->bigMesh.GetNormal(tri.v2);
    
    triangle.uv[0] = prefab->bigMesh.GetUV(tri.v0);
    triangle.uv[1] = prefab->bigMesh.GetUV(tri.v1);
    triangle.uv[2] = prefab->bigMesh.GetUV(tri.v2);

    float3 baryCentrics = { 1.0f - hitOut.u - hitOut.v, hitOut.u, hitOut.v };
    Matrix4 inverseMat = Matrix4::InverseTransform(prefab->globalNodeTransforms[hitNodeIdx]);
    
    Vector4x32f n0 = Vector4Transform(triangle.normal[0], inverseMat.r);
    Vector4x32f n1 = Vector4Transform(triangle.normal[1], inverseMat.r);
    Vector4x32f n2 = Vector4Transform(triangle.normal[2], inverseMat.r);
            
    hitOut.normal = VecMulf(n0, baryCentrics.x);
    hitOut.normal = VecAdd(hitOut.normal, VecMulf(n1, baryCentrics.y));
    hitOut.normal = VecAdd(hitOut.normal, VecMulf(n2, baryCentrics.z));
    hitOut.normal = Vec3Norm(hitOut.normal);
    
    hitOut.uv = triangle.uv[0] * baryCentrics.x
              + triangle.uv[1] * baryCentrics.y
              + triangle.uv[2] * baryCentrics.z;
    // Material material = g_Materials[hitInstance.materialStart + triangle.materialIndex];
    // RGBA8 pixel = g_TexturePixels[SampleTexture(g_Textures[material.albedoTextureIndex], record.uv)];
    // record.color = MultiplyU32Colors(material.color, pixel);
    hitOut.position = VecAdd(ray.origin, VecMulf(ray.direction, hitOut.t));
    VecSetW(hitOut.position, 1.0);
    return hitOut;
}

// todo ignore mask
Triout RayCastFromCamera(CameraBase* camera,
    Vector2f screenPos,
    Scene* scene,
    ushort prefabID,
    AnimationController* animSystem)
{
    Ray ray = camera->ScreenPointToRay(screenPos);
    return RayCastScene(ray, scene, prefabID, animSystem);
}

// Prefab* mainScene = g_CurrentScene.GetPrefab(MainScenePrefab);
// int rootNodeIdx = mainScene->GetRootNodeIdx();
// ANode* rootNode = &mainScene->nodes[rootNodeIdx];
// 
// // VecStore(rootNode->rotation, QFromYAngle(HalfPI/2.0f));
// // mainScene->UpdateGlobalNodeTransforms(rootNodeIdx, Matrix4::Identity());
// 
// mainScene->tlas = new TLAS(mainScene);
// mainScene->tlas->Build();
// SceneRenderer::InitRayTracing(mainScene);

// void EditorCastRay()
// {
//     Vector2f rayPos;
//     GetMouseWindowPos(&rayPos.x, &rayPos.y); // { 1920.0f / 2.0f, 1080.0f / 2.0f };
// 
//     if (!GetMousePressed(MouseButton_Left) || uAnyWindowHovered(rayPos)) return;
//     
//     Prefab* sphere = g_CurrentScene.GetPrefab(SpherePrefab);
//     CameraBase* camera = SceneRenderer::GetCamera();
//     Scene* currentScene = &g_CurrentScene;
// 
//     Prefab* mainScene = g_CurrentScene.GetPrefab(MainScenePrefab);
//     Triout rayResult = RayCastFromCamera(camera, rayPos, currentScene, MainScenePrefab, nullptr);
//     
//     if (rayResult.t != RayacastMissDistance)
//     {
//         int nodeIndex = mainScene->nodes[SelectedNodeIndex].index;
//         AMesh* mesh = nullptr;
// 
//         if (nodeIndex != -1) {
//             mesh = mainScene->meshes + nodeIndex;
//             // remove outline of last selected object
//             mesh->primitives[SelectedNodePrimitiveIndex].hasOutline = false;
//         }
//   
//         SelectedNodeIndex = rayResult.nodeIndex;
//         SelectedNodePrimitiveIndex = rayResult.primitiveIndex;
// 
//         mesh = mainScene->meshes + mainScene->nodes[SelectedNodeIndex].index;
//         mesh->primitives[SelectedNodePrimitiveIndex].hasOutline = true;
//         
//         sphere->globalNodeTransforms[0].r[3] = rayResult.position;
//         
//         FocusPrefabViewToSelectedNode();
//     }
//     else {
//         SelectedNodeIndex = 0;
//         SelectedNodePrimitiveIndex = 0;
//     }
//     // static char rayDistTxt[16] = {};
//     // float rayDist = 999.0f;
//     // FloatToString(rayDistTxt, rayDist);
//     // uDrawText(rayDistTxt, rayPos);
// }
