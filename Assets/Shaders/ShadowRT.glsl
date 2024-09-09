
out lowp vec4 Result;

in vec2 texCoord;

uniform highp sampler2D uDepthMap;
uniform highp sampler2D uBVHNodes;  // float4x2
uniform highp sampler2D uTLASNodes; // float4x2
uniform highp sampler2D uGlobalMatrices; // transform of the objects in scene
uniform lowp sampler2D uNormal;

uniform highp usampler2D uBVHInstances; // for each entity in scene, uint2 x:nodeIndex, y: bvhIndex

uniform highp isampler2D uTriangles; // faces, indices of triangles uint4, but we use first 3 component for 3 vertex in face

uniform highp mat4 uInvView;
uniform highp mat4 uInvProj;

uniform highp mat4 uInvViewProj;
uniform highp vec3 uSunDir;

layout(std430, binding = 0) buffer VertexBuffer
{
    float vertices[];
};

struct BVHNode
{
    vec3 min, max;
    int leftFirst, triCount;
};

struct Ray
{
    vec3 origin;
    vec3 direction;
};

vec3 UnpackVec3(int idx)
{
    return vec3(vertices[idx], vertices[idx + 1], vertices[idx + 2]);
}

mat4 GetGlobalMatrix(int index)
{
    index = index << 2; // * 4
    
    highp int x = index & 1023;
    highp int y = index >> 10;

    mat4 res; 
    res[0] = texelFetch(uGlobalMatrices, ivec2(x + 0, y), 0);
    res[1] = texelFetch(uGlobalMatrices, ivec2(x + 1, y), 0);
    res[2] = texelFetch(uGlobalMatrices, ivec2(x + 2, y), 0);
    res[3] = texelFetch(uGlobalMatrices, ivec2(x + 3, y), 0);
    return res;
}

// struct BVHInstance {
//     int nodeIndex;
//     int bvhIndex;
// };
ivec2 GetBVHInstance(int index)
{
    return ivec2(texelFetch(uBVHInstances, ivec2(index, 0), 0).rg);
}

BVHNode GetBVHNode(highp sampler2D tex, int index)
{
    index <<= 1; // * 2
    highp int x = index & 1023;
    highp int y = index >> 10;

    vec4 a = texelFetch(tex, ivec2(x    , y), 0);
    vec4 b = texelFetch(tex, ivec2(x + 1, y), 0);
    
    BVHNode result;
    result.min = a.xyz;
    result.max = b.xyz;
    result.leftFirst = floatBitsToInt(a.w);
    result.triCount  = floatBitsToInt(b.w);
    return result;
}

bool IsPointInsideAABB(vec3 point, vec3 aabbMin, vec3 aabbMax)
{
    return all(equal(lessThan(point, aabbMax), greaterThan(point, aabbMin)));
           // point.x < aabbMax.x && point.x > aabbMin.x &&
           // point.y < aabbMax.y && point.y > aabbMin.y &&
           // point.z < aabbMax.z && point.z > aabbMin.z;
}

float Max3(vec3 v) {
    return max(v.x, max(v.y, v.z));
}

float Min3(vec3 v) {
    return min(v.x, min(v.y, v.z));
}

bool IntersectAABB(vec3 origin, vec3 invDir, vec3 aabbMin, vec3 aabbMax)
{
    if (IsPointInsideAABB(origin, aabbMin, aabbMax)) return true;
    
    vec3 tmin = (aabbMin - origin) * invDir;
    vec3 tmax = (aabbMax - origin) * invDir;
    float tnear = Max3(min(tmin, tmax));
    float tfar  = Min3(max(tmin, tmax));

    return tnear < tfar && tnear > 0.0;
}

float IntersectTriangle(Ray ray, vec3 v0, vec3 v1, vec3 v2)
{
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;

    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);

    float f = 1.0 / a;
    vec3 s = ray.origin - v0;
    float u = f * dot(s, h);
    bool fail = (u < 0.0) || (u > 1.0);

    vec3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);
    float t = f * dot(edge2, q);
    fail = fail || (v < 0.0f) || (u + v > 1.0);

    return (!fail && t > 0.0001f) ? t : 0.0;
}

float IntersectBVH(Ray ray, int rootNode)
{
    int nodesToVisit[32];
    nodesToVisit[0] = rootNode;
    
    int currentNodeIndex = 1;
    
    vec3 invDir = vec3(1.0) / ray.direction;
    int protection = 0;
    
    while (currentNodeIndex > 0 && protection++ < 128)
    {
        BVHNode node = GetBVHNode(uBVHNodes, nodesToVisit[--currentNodeIndex]);
    
        int triCount = node.triCount;
        int leftFirst = node.leftFirst;
        if (triCount > 0) // is leaf 
        {
            for (int i = leftFirst; i < leftFirst + triCount; ++i)
            {
                highp ivec3 tri = texelFetch(uTriangles, ivec2(i & 1023, i >> 10), 0).rgb;
    
                vec3 v0 = UnpackVec3(tri[0] * 6);
                vec3 v1 = UnpackVec3(tri[1] * 6);
                vec3 v2 = UnpackVec3(tri[2] * 6);
                float res = IntersectTriangle(ray, v0, v1, v2);
                if (res != 0.0f)
                    return res;
            }
    
            continue;
        }
    
        int leftIndex  = leftFirst;
        int rightIndex = leftIndex + 1;
    
        BVHNode leftNode  = GetBVHNode(uBVHNodes, leftIndex);
        BVHNode rightNode = GetBVHNode(uBVHNodes, rightIndex);
    
        bool dist1 = IntersectAABB(ray.origin, invDir, leftNode.min, leftNode.max);
        bool dist2 = IntersectAABB(ray.origin, invDir, rightNode.min, rightNode.max);
    
        if (dist1 != false) nodesToVisit[currentNodeIndex++] = leftIndex;
        if (dist2 != false) nodesToVisit[currentNodeIndex++] = rightIndex;
    }
    return 500.0;
}

float TraverseTLAS(Ray ray, int rootNode)
{
    int nodesToVisit[32];
    int currentNodeIndex = 1;
    vec3 invDir = vec3(1.0f) / ray.direction;
    int protection = 0;

    nodesToVisit[0] = rootNode;
    
    while (currentNodeIndex > 0 && protection++ < 64)
    {
        BVHNode node = GetBVHNode(uTLASNodes, nodesToVisit[--currentNodeIndex]);
        
        int instanceCount = node.triCount;
        int leftFirst = node.leftFirst;

        if (instanceCount > 0) // is leaf 
        {
            for (int i = leftFirst; i < leftFirst + instanceCount; ++i)
            {
                ivec2 instance = GetBVHInstance(i); // .x = nodeIdx, .y = bvhIndex
                
                Ray meshRay;
                mat4 model = GetGlobalMatrix(instance.x);
                // change ray position & oriantation instead of mesh position for capturing in different positions
                mat4 inverseTransform = inverse(model);
                meshRay.origin    = (vec4(ray.origin, 1.0f) * inverseTransform).xyz;
                meshRay.direction = (vec4(ray.direction, 1.0f) * inverseTransform).xyz;
                float res = IntersectBVH(meshRay, instance.y);
                if (res != 500.0) return res;
            }
            continue;
        }
        
        int leftIndex  = leftFirst;
        int rightIndex = leftIndex + 1;
    
        BVHNode leftNode  = GetBVHNode(uTLASNodes, leftIndex);
        BVHNode rightNode = GetBVHNode(uTLASNodes, rightIndex);
    
        bool dist1 = IntersectAABB(ray.origin, invDir, leftNode.min, leftNode.max);
        bool dist2 = IntersectAABB(ray.origin, invDir, rightNode.min, rightNode.max);
    
        if (dist1 != false) nodesToVisit[currentNodeIndex++] = leftIndex;
        if (dist2 != false) nodesToVisit[currentNodeIndex++] = rightIndex;
    }

    return 500.0;
}

vec3 WorldSpacePosFromDepthBuffer()
{
    float depth = texture(uDepthMap, texCoord).r;
    vec4 clipspace = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 WsPos = uInvViewProj * clipspace;
    return WsPos.xyz / WsPos.w;
}

Ray GetRay(vec2 pos)
{
    ivec2 viewportSize = textureSize(uDepthMap, 0);
    vec2 coord = pos; // (pos * vec2(viewportSize)) / vec2(viewportSize);
    coord.y = 1.0f - coord.y;    // Flip Y to match the NDC coordinate system
    coord = coord * 2.0f - 1.0f; // Map to range [-1, 1]

    vec4 clipSpacePos = vec4(coord.x, coord.y, 1.0f, 1.0f);
    vec4 viewSpacePos = clipSpacePos * uInvProj;
    viewSpacePos = viewSpacePos / viewSpacePos.w;
    
    vec3 position = uInvView[3].xyz;
    vec3 worldSpacePos = vec3(viewSpacePos * uInvView);
    vec3 rayDir = normalize(worldSpacePos - position);
        
    Ray ray;
    ray.origin = position; 
    ray.direction = rayDir;
    return ray;
}

void main()
{
    vec3 pos = WorldSpacePosFromDepthBuffer();
    vec3 normal = texture(uNormal, texCoord).xyz * 2.0 - 1.0;
    
    Ray ray; //  = GetRay(texCoord);
    ray.origin = pos + (normal * 0.2f);
    ray.direction = uSunDir;

    float res = TraverseTLAS(ray, 0);
    if (dot(normal, uSunDir) < 0.0) res = 0.0;

    Result.xyz = vec3(res == 500.0f ? 1.0 : 0.0); // res / 50.0f
}