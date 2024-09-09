
layout(location = 0) in highp   vec3  aPos;
layout(location = 1) in lowp    vec3  aNormal;
layout(location = 2) in lowp    vec4  aTangent;
layout(location = 3) in mediump vec2  aTexCoords;
layout(location = 4) in lowp    uvec4 aJoints; // lowp int ranges between 0-255 
layout(location = 5) in lowp    vec4  aWeights;

uniform highp mat4 uModel;
uniform highp mat4 uViewProj;

uniform highp sampler2D uAnimTex;

uniform int uHasAnimation;

// https://developer.android.com/games/optimize/vertex-data-management
void main()
{
    highp mat4 model = uModel;

    // vBoneIdx = -1;
    if (false) // (uHasAnimation > 0) 
    {
        mediump mat4 animMat = mat4(0.0);
        animMat[3].w = 1.0; // last row is [0.0, 0.0, 0.0, 1.0]

        for (int i = 0; i < 4; i++)
        {
            int matIdx = int(aJoints[i]) * 3; // 3 because our matrix is: RGBA16f x 3
            animMat[0] += texelFetch(uAnimTex, ivec2(matIdx + 0, 0), 0) * aWeights[i];
            animMat[1] += texelFetch(uAnimTex, ivec2(matIdx + 1, 0), 0) * aWeights[i];
            animMat[2] += texelFetch(uAnimTex, ivec2(matIdx + 2, 0), 0) * aWeights[i]; 
        }
        // vBoneIdx = int(aJoints[0]);
        model = model * transpose(animMat);
    }

    mediump mat3 normalMatrix = mat3(model);
    normalMatrix[0] = normalize(normalMatrix[0]);
    normalMatrix[1] = normalize(normalMatrix[1]);
    normalMatrix[2] = normalize(normalMatrix[2]);

    vec3 normal = normalize(normalMatrix * aNormal);
    
    float scale = 1.0 / length(model[0].xyz);
    vec3 normalBias = aNormal * scale * 0.04;

    gl_Position = uViewProj * (model * vec4(aPos + normalBias, 1.0));
} 