
layout (points) in; // Geometry shader will receive points
layout (triangle_strip, max_vertices = 128) out; // Output vertices (triangle strip)

uniform ivec2 mChunkOffset;
uniform vec3 mCameraPos;
uniform vec3 mCameraDir;
uniform mat4 mViewProj;
uniform float mTime;

uniform lowp sampler2D mPerlinNoise;
uniform lowp sampler2D mNormalTex;

out mediump vec3 vWorldPos;
out mediump vec3 vNormal;
out lowp float vIsTop; // top vertex of triangle ?

const float PI = 3.14159265;

float Sin0pi(float x) {
    x *= 0.63655f; // constant founded using desmos
    x *= 2.0f - x;
    return x * (0.225f * x + 0.775f); 
}

// calculates cos(x) between [0,pi]
float Cos0pi(float a) {
    a *= 0.159f;
    return 1.0f - 32.0f * a * a * (0.75f - a);
}

float hash(ivec2 p ) {
    // 2D -> 1D
    int n = p.x*3 + p.y*113;
    // 1D hash by Hugo Elias
    n = (n << 13) ^ n;
    n = n * (n * n * 15731 + 789221) + 1376312589;
    return -1.0 + 2.0 * float(n &0x0fffffff) / float(0x0fffffff);
}

float noise(vec2 p ) {
    ivec2 i = ivec2(floor(p));
    vec2 f = fract(p);
    // cubic interpolant
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i + ivec2(0, 0)), 
                   hash(i + ivec2(1, 0)), u.x),
               mix(hash(i + ivec2(0, 1)), 
                   hash(i + ivec2(1, 1)), u.x), u.y);
}

vec2 hash22(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

float sqr(float x) { return x * x; }

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
    
    int grassIndex    = gl_PrimitiveIDIn;
    ivec2 grassStart = ivec2(grassIndex & 511, grassIndex >> 9);
    vec2 grassPos = grassStart * segmentSize;
    grassPos -= chunkSize * 0.5f; // start from center
    grassPos += vec2(mChunkOffset) * offsetSize;

    vec2 grassCenterPos = grassPos + segmentSize;
    vec2 camPosBack = mCameraPos.xz + mCameraDir.xz * 6.0;
    vec2 toGrass = normalize(camPosBack - grassCenterPos);
    vec2 grassToCam = camPosBack - grassCenterPos;
    const float MaxGrassDist = 40000.0f;
    float grassDistSqr = dot(grassToCam, grassToCam);
    bool closeToGrass = grassDistSqr < MaxGrassDist;

    vec3 testNormal = normalize(texelFetch(mNormalTex, grassStart, 0).rgb * 2.0 - 1.0);
    float steepness = 1.0 - sqr(testNormal.y); // abs(normal.x) + abs(normal.z); 
    float testHeight = texelFetchOffset(mPerlinNoise, grassStart, 0, ivec2(0, 0)).r / 2.24;

    if (dot(-mCameraDir.xz, toGrass) > -0.5 || !closeToGrass || steepness > 0.7 || testHeight > 0.67)
        return;

    float heightCenter   = texelFetchOffset(mPerlinNoise, grassStart, 0, ivec2( 0, 0)).r;
    float heightRight    = texelFetchOffset(mPerlinNoise, grassStart, 0, ivec2(+1, 0)).r;
    float heightUp       = texelFetchOffset(mPerlinNoise, grassStart, 0, ivec2( 0,+1)).r;
    float heightDiagonal = texelFetchOffset(mPerlinNoise, grassStart, 0, ivec2(+1,+1)).r;
    
    ivec2 globalStart = ivec2((grassStart.x + mChunkOffset.x * 64) & 511, (grassStart.y + mChunkOffset.y * 64) >> 9);
    vec2 off = vec2(globalStart) * vec2(12.9898, 78.233);

    // normalize and rotate 90 degree for billboard effect
    grassToCam = normalize(grassToCam);
    grassToCam = grassToCam.yx * vec2(1.0, -1.0); // 90 degree rotate

    float grassDist = grassDistSqr / MaxGrassDist;
    float numBladePerSegment = 16.0 - (grassDist * 16.0);
    float grassHeight = (1.0-testHeight) * 1.63f - sin(grassPos.y * 0.14) * 0.25; 
    float grassWidth = 0.08f + (testHeight * 0.08);
    vec3 faceDir = vec3(grassToCam.x, 0.0, grassToCam.y) * grassWidth; // vec3(Sin0pi(bc.x * PI), 0.0, Cos0pi(bc.x * PI)) * grassWidth;
    
    const float grassWidthRCP = (1.0 / grassWidth);
    vec3 norm = cross(faceDir * grassWidthRCP, vec3(0.0, 1.0, 0.0));
    vec2 wind = faceDir.xz * grassWidthRCP * sin(grassPos.y * .12 + mTime * 2.0 + grassPos.x * 0.2) * 0.4;

    if (noise(grassPos * 0.22) < 0.4) // < test with some variance noise to not have grass always
    for (float k = 0.0; k < numBladePerSegment; k += 1.0)
    {
        // random value between 0 - 1;
        vec2 bc = hash22(off + k); // fract(sin((off + k) * vec2(12.9898, 78.233)) * vec2(7453.5453, 453.2435));
    
        float bottom = mix(heightCenter, heightRight, bc.x);
        float top    = mix(heightUp, heightDiagonal, bc.x);
        float height = mix(bottom, top, bc.y); // ground level
    
        vec3 center = vec3(
            grassPos.x + bc.x * segmentSize, 
            height * terrainHeight - 0.002, 
            grassPos.y + bc.y * segmentSize
        );
    
        vec4 left  = vec4(center + faceDir, 1.0);
        vec4 right = vec4(center - faceDir, 1.0);
        vec4 up    = vec4(center, 1.0);
        up.y += grassHeight;
        
        gl_Position = mViewProj * right;
        vWorldPos = right.xyz;
        vNormal = norm;
        vIsTop = 0.0;
        EmitVertex();

        gl_Position = mViewProj * left;
        vWorldPos = left.xyz;
        vNormal = norm;
        vIsTop = 0.0;
        EmitVertex();

        // if (grassDist < 0.3) // < reduce triangle count when we are close, but I haven't seen any performance improvements
        {
            vec4 mid = mix(right, up, 0.25);
            mid.xz += wind * 0.16;
            gl_Position = mViewProj * mid;
            vWorldPos = mid.xyz;
            vNormal = norm;
            vIsTop = 0.35;
            EmitVertex();

            mid = mix(left, up, 0.5);
            mid.xz += wind * 0.44;
            gl_Position = mViewProj * mid;
            vWorldPos = mid.xyz;
            vNormal = norm;
            vIsTop = 0.6;
            EmitVertex();
        }

        up.xz += wind * 1.0;
        gl_Position = mViewProj * up;
        vWorldPos = up.xyz;
        vNormal = norm;
        vIsTop = 1.0;
        EmitVertex();

        EndPrimitive();
    }
}

// // backface
// gl_Position = mViewProj * left;
// vWorldPos = left.xyz;
// vNormal = norm;
// vIsTop = 0.0;
// EmitVertex();
//                 
// gl_Position = mViewProj * right;
// vWorldPos = right.xyz;
// vNormal = norm;
// vIsTop = 0.0;
// EmitVertex();
//                 
// gl_Position = mViewProj * up;
// vWorldPos = up.xyz;
// vNormal = norm;
// vIsTop = 1.0;
// EmitVertex();