
layout (points) in; // Geometry shader will receive points
layout (triangle_strip, max_vertices = 256) out; // Output 16384 vertices (triangle strip)

uniform int mNumChunks; // on x and z axis
uniform int mChunkNumSegments;
uniform float mChunkSize; // < in meters. each segment has (chunkSize / numSegment) width and height

uniform vec3 mCameraPos;
uniform vec3 mCameraDir;
uniform mat4 mViewProj;

uniform sampler2D mPerlinNoise;
uniform sampler2D mNormalTex;

out mediump vec3 vWorldPos;
out mediump vec3 vNormal;

const float PI = 3.14159265;

float ACos(float x) {
    // Lagarde 2014, "Inverse trigonometric functions GPU optimization for AMD GCN architecture"
    // This is the approximation of degree 1, with a max absolute error of 9.0x10^-3
    float y = abs(x);
    float p = -0.1565827 * y + 1.570796;
    p *= sqrt(1.0 - y);
    return x >= 0.0 ? p : PI - p;
}

float AngleBetween(vec2 from, vec2 to)
{
    float denominator = inversesqrt(dot(from, from) * dot(to, to));
    float dot = clamp(dot(from, to) * denominator, -1.0, 1.0);
    return ACos(dot);
}

float EaseOut(float x) { 
    float r = 1.0 - x;
    return 1.0 - (r * r); 
}

void main()
{
    float chunkWidth = sqrt(float(mChunkNumSegments));
    int iChunkWidth = int(chunkWidth);
    float segmentSize = mChunkSize / chunkWidth;
    int index = gl_PrimitiveIDIn;

    ivec2 iChunkStart = ivec2(index % mNumChunks, index / mNumChunks);
    vec2 chunkPos = vec2(iChunkStart) * mChunkSize;
    const float terrainHeight = 35.0;

    for (int i = 0; i < iChunkWidth; i++)
    {
        for (int j = 0; j < iChunkWidth; j++)
        {
            ivec2 segmentIndex = iChunkStart * iChunkWidth + ivec2(j, i);

            float heightCenter   = texelFetchOffset(mPerlinNoise, segmentIndex, 0, ivec2( 0,  0)).r;
            float heightRight    = texelFetchOffset(mPerlinNoise, segmentIndex, 0, ivec2(+1,  0)).r;
            float heightUp       = texelFetchOffset(mPerlinNoise, segmentIndex, 0, ivec2( 0, +1)).r;
            float heightDiagonal = texelFetchOffset(mPerlinNoise, segmentIndex, 0, ivec2(+1, +1)).r;

            vec2 vertPoint = vec2(float(j), float(i)) * segmentSize + chunkPos;
        
            vec4 worldPos = vec4(vertPoint.x, heightCenter * terrainHeight, vertPoint.y, 1.0);
            gl_Position = mViewProj * worldPos;
            vWorldPos = worldPos.xyz; 
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

// vec2 chunkCenterPos = mChunkSize * 0.5 + chunkPos;
// vec2 toChunk = normalize(mCameraPos.xz - chunkCenterPos);
// vec2 chunkToCam = mCameraPos.xz - chunkCenterPos;
// 
// if (AngleBetween(mCameraDir.xz, toChunk) < 1.72 || dot(chunkToCam, chunkToCam) < 512.0f)