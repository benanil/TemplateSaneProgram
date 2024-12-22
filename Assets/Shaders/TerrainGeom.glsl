
layout (points) in; // Geometry shader will receive points
layout (triangle_strip, max_vertices = 256) out; // Output 16384 vertices (triangle strip)

uniform ivec2 mChunkOffset;
uniform vec3 mCameraPos;
uniform vec3 mCameraDir;
uniform mat4 mViewProj;

uniform lowp sampler2D mPerlinNoise;
uniform lowp sampler2D mNormalTex;

out mediump vec3 vWorldPos;
out mediump vec3 vNormal;

const float PI = 3.14159265;

float ACosPositive(float x) {
    float p = -0.1565827f * x + 1.570796f;
    return p * sqrt(1.0f - x);
}

float AngleBetween(vec2 from, vec2 to) {
    float denominator = inversesqrt(dot(from, from) * dot(to, to));
    float dot = min(dot(from, to) * denominator, 1.0);
    return ACosPositive(dot);
}

// total      2048px x 2048px -> 4x4 chunks -> 32x32 groups -> 256x256 quads -> 2048x2048 vertices
// chunk size 512px  x 512px  -> 8x8 groups -> 
// group size 64px   x 64px   -> 8x8 quads
// quad  size 8px    x 8px    -> 8x8 vertices

void main()
{
    const int   numQuads      = 64;   // 64x64 on x and z axis (512/8)
    const float quadSize      = 20.0; // in meters. 
    const float quadNumSeg    = 8.0;  // 8x8 segments
    const int   iquadNumSeg   = int(quadNumSeg);
    const float segmentSize   = quadSize / quadNumSeg;
    const float terrainHeight = 35.0;
    const float chunkSize  = quadSize * numQuads;
    const float offsetSize = chunkSize / 8.0;
    
    int index = gl_PrimitiveIDIn;
    ivec2 igroupStart = ivec2(index & 63, index >> 6); // numGroups % numGroups, numGroups / numGroups
    
    vec2 groupPos = vec2(igroupStart) * quadSize;
    groupPos -= chunkSize * 0.5f; // start from center
    groupPos += vec2(mChunkOffset) * offsetSize;

    vec2 chunkCenterPos = groupPos + 10.0;
    vec2 camPosBack = mCameraPos.xz - mCameraDir.xz * 6.0; // slightly back of camera
    vec2 toChunk = normalize(camPosBack - chunkCenterPos);
    vec2 chunkToCam = camPosBack - chunkCenterPos;
    ivec2 segmentIndex = ivec2(igroupStart * iquadNumSeg);
    float chunkDistSqr = dot(chunkToCam, chunkToCam);
    bool closeToQuad =  chunkDistSqr < 2512.0f;

    if (!(AngleBetween(mCameraDir.xz, toChunk) < 1.31 || closeToQuad) || chunkDistSqr > 340000.0) // || chunkDistSqr > 40000.0f
        return;
    
    for (int i = 0; i < iquadNumSeg; i++)
    {
        for (int j = 0; j < iquadNumSeg; j++)
        {
            segmentIndex = ivec2(igroupStart * iquadNumSeg + ivec2(j, i));
    
            float heightCenter   = texelFetchOffset(mPerlinNoise, segmentIndex, 0, ivec2( 0,  0)).r;
            float heightRight    = texelFetchOffset(mPerlinNoise, segmentIndex, 0, ivec2(+1,  0)).r;
            float heightUp       = texelFetchOffset(mPerlinNoise, segmentIndex, 0, ivec2( 0, +1)).r;
            float heightDiagonal = texelFetchOffset(mPerlinNoise, segmentIndex, 0, ivec2(+1, +1)).r;
    
            vec2 vertPoint = vec2(float(j), float(i)) * segmentSize + groupPos;
    
            vec4 worldPos = vec4(vertPoint.x, heightCenter * terrainHeight, vertPoint.y, 1.0);
            gl_Position = mViewProj * worldPos;
            vWorldPos = worldPos.xyz; // vec2(igroupStart) / 32.0;
            vNormal = texelFetch(mNormalTex, segmentIndex, 0).rgb;
            EmitVertex();
    
            worldPos = vec4(vertPoint.x, heightUp * terrainHeight, vertPoint.y + segmentSize, 1.0);
            gl_Position = mViewProj * worldPos;
            vWorldPos = worldPos.xyz; 
            vNormal = texelFetchOffset(mNormalTex, segmentIndex, 0, ivec2(0, 1)).rgb;
            EmitVertex();
    
            worldPos = vec4(vertPoint.x + segmentSize, heightRight * terrainHeight, vertPoint.y, 1.0);
            gl_Position = mViewProj * worldPos;
            vWorldPos = worldPos.xyz; 
            vNormal = texelFetchOffset(mNormalTex, segmentIndex, 0, ivec2(1, 0)).rgb;
            EmitVertex();
    
            worldPos = vec4(vertPoint.x + segmentSize, heightDiagonal * terrainHeight, vertPoint.y + segmentSize, 1.0);
            gl_Position = mViewProj * worldPos;
            vNormal = texelFetchOffset(mNormalTex, segmentIndex, 0, ivec2(1, 1)).rgb;
            vWorldPos = worldPos.xyz; 
            EmitVertex();
        }
        EndPrimitive();
    }   
}

// vec3 n0 = (texelFetch(mNormalTex, segmentIndex + 0, 0).rgb * 2.0) - 1.0;
// vec3 n1 = (texelFetch(mNormalTex, segmentIndex + 7, 0).rgb * 2.0) - 1.0;
// vec3 n2 = (texelFetch(mNormalTex, segmentIndex + ivec2(7, 0), 0).rgb * 2.0) - 1.0;
// vec3 n3 = (texelFetch(mNormalTex, segmentIndex + ivec2(0, 7), 0).rgb * 2.0) - 1.0;
//     
// if (!closeToQuad &&
//     abs(n0.x) + abs(n0.y) < 0.032 &&
//     abs(n1.x) + abs(n1.y) < 0.032 &&
//     abs(n2.x) + abs(n2.y) < 0.032 &&
//     abs(n3.x) + abs(n3.y) < 0.032
// )
// {
//     float heightCenter   = texelFetch(mPerlinNoise, segmentIndex + ivec2( 0,  0), 0).r;
//     float heightRight    = texelFetch(mPerlinNoise, segmentIndex + ivec2(+7,  0), 0).r;
//     float heightUp       = texelFetch(mPerlinNoise, segmentIndex + ivec2( 0, +7), 0).r;
//     float heightDiagonal = texelFetch(mPerlinNoise, segmentIndex + ivec2(+7, +7), 0).r;
//     
//     vec2 vertPoint = groupPos;
//         
//     vec4 worldPos = vec4(vertPoint.x, heightCenter * terrainHeight, vertPoint.y, 1.0);
//     worldPos.xyz -= n0;
//     vNormal = vec3(0.0);
//     // vNormal = (n0 + 1.0) * 0.5;
//     gl_Position = mViewProj * worldPos;
//     vWorldPos = worldPos.xyz; // vec2(igroupStart) / 32.0;
//     EmitVertex();
//     
//     worldPos = vec4(vertPoint.x, heightUp * terrainHeight, vertPoint.y + (segmentSize * 9.0), 1.0);
//     worldPos.xyz -= n3;
//     vNormal = vec3(0.0);
//     // vNormal = (n3 + 1.0) * 0.5;
//     gl_Position = mViewProj * worldPos;
//     vWorldPos = worldPos.xyz; 
//     EmitVertex();
//         
//     worldPos = vec4(vertPoint.x + (segmentSize * 9.0), heightRight * terrainHeight, vertPoint.y, 1.0);
//     worldPos.xyz -= n2;
//     vNormal = vec3(0.0);
//     // vNormal = (n2 + 1.0) * 0.5;
//     gl_Position = mViewProj * worldPos;
//     vWorldPos = worldPos.xyz; 
//     EmitVertex();
//         
//     worldPos = vec4(vertPoint.x + (segmentSize * 9.0), heightDiagonal * terrainHeight , vertPoint.y + (segmentSize * 9.0), 1.0);
//     worldPos.xyz -= n1;
//     vNormal = vec3(0.0);
//     // vNormal = (n1 + 1.0) * 0.5;
//     gl_Position = mViewProj * worldPos;
//     vWorldPos = worldPos.xyz; 
//     EmitVertex();
//     EndPrimitive();
// }
// else

