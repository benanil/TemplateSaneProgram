
layout(local_size_x = 16, local_size_y = 16) in;

// Images
layout(binding = 0)          uniform highp sampler2D uDepthMap;
layout(binding = 1, rgba32f) uniform highp readonly image2D  uBVHNodes;
layout(binding = 2, rgba32f) uniform highp readonly image2D  uTLASNodes;
layout(binding = 3, rgba32f) uniform highp readonly image2D  uGlobalMatrices;
layout(binding = 4, rgba8)   uniform highp readonly image2D  uNormal;
layout(binding = 5, rg32ui)  uniform highp readonly uimage2D uBVHInstances;
layout(binding = 6, rgba32i) uniform highp readonly iimage2D uTriangles;

layout(binding = 7, rgba8) uniform lowp writeonly image2D uResult;

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
    res[0] = imageLoad(uGlobalMatrices, ivec2(x + 0, y));
    res[1] = imageLoad(uGlobalMatrices, ivec2(x + 1, y));
    res[2] = imageLoad(uGlobalMatrices, ivec2(x + 2, y));
    res[3] = imageLoad(uGlobalMatrices, ivec2(x + 3, y));
    return res;
}

// struct BVHInstance {
//     int nodeIndex;
//     int bvhIndex;
// };
ivec2 GetBVHInstance(int index)
{
    return ivec2(imageLoad(uBVHInstances, ivec2(index, 0)).rg);
}

BVHNode GetBVHNode(int index)
{
    index <<= 1; // * 2
    highp int x = index & 1023;
    highp int y = index >> 10;

    vec4 a = imageLoad(uBVHNodes, ivec2(x    , y));
    vec4 b = imageLoad(uBVHNodes, ivec2(x + 1, y));
    
    BVHNode result;
    result.min = a.xyz;
    result.max = b.xyz;
    result.leftFirst = floatBitsToInt(a.w);
    result.triCount  = floatBitsToInt(b.w);
    return result;
}

BVHNode GetTLASNode(int index)
{
    index <<= 1; // * 2
    highp int x = index & 1023;
    highp int y = index >> 10;

    vec4 a = imageLoad(uTLASNodes, ivec2(x    , y));
    vec4 b = imageLoad(uTLASNodes, ivec2(x + 1, y));
    
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
        BVHNode node = GetBVHNode(nodesToVisit[--currentNodeIndex]);
    
        int triCount = node.triCount;
        int leftFirst = node.leftFirst;
        if (triCount > 0) // is leaf 
        {
            for (int i = leftFirst; i < leftFirst + triCount; ++i)
            {
                highp ivec3 tri = imageLoad(uTriangles, ivec2(i & 1023, i >> 10)).rgb;
    
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
    
        BVHNode leftNode  = GetBVHNode(leftIndex);
        BVHNode rightNode = GetBVHNode(rightIndex);
    
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
        BVHNode node = GetTLASNode(nodesToVisit[--currentNodeIndex]);
        
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
    
        BVHNode leftNode  = GetTLASNode(leftIndex);
        BVHNode rightNode = GetTLASNode(rightIndex);
    
        bool dist1 = IntersectAABB(ray.origin, invDir, leftNode.min, leftNode.max);
        bool dist2 = IntersectAABB(ray.origin, invDir, rightNode.min, rightNode.max);
    
        if (dist1 != false) nodesToVisit[currentNodeIndex++] = leftIndex;
        if (dist2 != false) nodesToVisit[currentNodeIndex++] = rightIndex;
    }

    return 500.0;
}

vec3 WorldSpacePosFromDepthBuffer(vec2 texCoord)
{
    float depth = texture(uDepthMap, texCoord).r;
    vec4 clipspace = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 WsPos = uInvViewProj * clipspace;
    return WsPos.xyz / WsPos.w;
}

Ray GetRay(vec2 pos)
{
    vec2 coord = pos; // (pos * vec2(depthMapSize)) / vec2(depthMapSize);
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
    ray.direction = normalize(rayDir);
    return ray;
}

void main()
{
    ivec2 pixelPos = ivec2(gl_GlobalInvocationID.xy);
    vec2 texCoord = vec2(pixelPos) / vec2(1920.0 / 4.0, 1088.0 / 4.0);

    vec2 normalSize = vec2(imageSize(uNormal));
    ivec2 gBufferPos = ivec2(texCoord * normalSize);
    vec3 normal = imageLoad(uNormal, gBufferPos).xyz * 2.0 - 1.0;

    vec3 pos = WorldSpacePosFromDepthBuffer(texCoord);

    Ray ray; // = GetRay(texCoord);
    ray.origin = pos + (normal * 0.05); // Offset slightly along the normal
    ray.direction = uSunDir;

    // // Traverse the top-level acceleration structure (TLAS)
    float res = TraverseTLAS(ray, 0);
    
    // facing opposite to the sun? then this is shadowed
    if (dot(normal, uSunDir) < 0.0) res = 0.0;
    
    // Write result to output image
    imageStore(uResult, pixelPos, vec4(res == 500.0 ? 1.0 : 0.0)); // 
}