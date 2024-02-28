
const vec2 Noise[32] = vec2[32](
    vec2(0.195175, 0.736741),
    vec2(0.244343, -0.188865),
    vec2(0.625741, 0.937121),
    vec2(-0.050130, -0.450137),
    vec2(-0.931756, 0.560139),
    vec2(0.918269, 0.652424),
    vec2(0.194104, -0.404106),
    vec2(-0.936278, 0.118957),
    vec2(0.529759, -0.914171),
    vec2(-0.553064, -0.497885),
    vec2(-0.454489, 0.633071),
    vec2(0.363320, -0.519524),
    vec2(-0.961545, 0.338638),
    vec2(0.444608, -0.311692),
    vec2(0.441159, -0.325567),
    vec2(-0.068537, -0.747794),
    vec2(0.516745, -0.226640),
    vec2(0.479423, 0.223924),
    vec2(-0.299612, 0.642286),
    vec2(-0.071347, -0.369064),
    vec2(0.489274, -0.426653),
    vec2(-0.956226, 0.308837),
    vec2(-0.549539, -0.276691),
    vec2(0.528425, -0.270296),
    vec2(-0.126725, -0.645969),
    vec2(0.494536, 0.382938),
    vec2(-0.505193, 0.410266),
    vec2(-0.403827, -0.904105),
    vec2(0.719992, -0.711682),
    vec2(-0.550251, -0.610243),
    vec2(0.774245, 0.505680),
    vec2(-0.241162, -0.266085));

out float Result;
in vec2 texCoord;

uniform sampler2D uDepthMap;
uniform sampler2D uNormalTex;

uniform mat4 uView;

const float PI = 3.141592653589793238;
const float FarPlane = 1000.0f;

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

float EaseOut(float x) {
    float t = 1.0 - x;
    return (x * x);
}

vec3 ConvertToViewNormal(vec3 normal) {
    return vec3(uView * vec4(normal, 0.0));
}

void main()
{
    vec3 baseNormal = texture(uNormalTex, texCoord).rgb * vec3(2.0) - vec3(1.0); // < convert to [-1,1] range
    float baseDepth = texture(uDepthMap, texCoord).r;

    // optional scaling for normals that parallel to view,
    // we might want to sample less range for parallel normals (walls that we look sideways)
    vec2 sampleScale = vec2(1.0) - abs(ConvertToViewNormal(baseNormal).rg);
    sampleScale.x = mix(0.2, 1.0, sampleScale.x);
    sampleScale.y = mix(0.2, 1.0, sampleScale.y);

    vec2 screenSize = vec2(textureSize(uNormalTex, 0));
    vec2 texelSize = vec2(1.0) / screenSize;
    
#ifndef __ANDROID__
    const int numSamples  = 12;
#else
    const int numSamples  = 8;
#endif
    const float radius    = 1.0;

    float howFar = 1.0 - baseDepth;
    vec2 pixelRange = screenSize * texelSize * howFar;

    float occlusion  = 0.0;
    for (int i = 0; i < numSamples; i++)
    {
        vec2 offset = Noise[i].xy * pixelRange;
        vec2 sampleCoord = texCoord + offset;

        float depth = texture(uDepthMap, sampleCoord).r;
        vec3 normal = texture(uNormalTex, sampleCoord).rgb * vec3(2.0) - vec3(1.0); // < convert to [-1,1] range

        // calculate square distance, because distance function uses sqrt
        vec3 diff = baseNormal - normal;
        float normalDiffSqr = dot(diff, diff);
        float depthDiff  = abs(depth - baseDepth);
        float rangeCheck = depthDiff < 0.00005 ? 1.0 : 0.0;
        occlusion += normalDiffSqr * rangeCheck;
    }
    
    // max distance between two unit vectors are two, we need 0-1 range divide by 2(*.5)
    occlusion = 1.0 - saturate((sqrt(occlusion) * 0.5) / float(numSamples));
    occlusion = EaseOut(occlusion);
    
    Result = occlusion;
}