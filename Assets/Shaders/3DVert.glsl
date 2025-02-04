
layout(location = 0) in highp   vec3  aPos;
layout(location = 1) in lowp    vec3  aNormal;
layout(location = 2) in lowp    vec4  aTangent;
layout(location = 3) in mediump vec2  aTexCoords;
layout(location = 4) in lowp    uvec4 aJoints; // lowp int ranges between 0-255 
layout(location = 5) in lowp    vec4  aWeights;

out mediump vec2 vTexCoords;
out highp   vec4 vLightSpaceFrag;
out lowp    mat3 vTBN;
// out flat    lowp int  vBoneIdx;

uniform highp mat4 uModel;
uniform highp mat4 uLightMatrix;
uniform highp mat4 uViewProj;

uniform highp sampler2D uAnimTex;
uniform mediump vec3 uSunDir;

uniform int uHasNormalMap;
uniform int uHasAnimation;

// https://www.shadertoy.com/view/3s33zj
mat3 adjoint(in mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz),
                cross(m[2].xyz, m[0].xyz),
                cross(m[0].xyz, m[1].xyz));
}

// https://developer.android.com/games/optimize/vertex-data-management
void main()
{
    highp mat4 model = uModel;

    // vBoneIdx = -1;
    if (uHasAnimation > 0) 
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

    mediump mat3 normalMatrix = adjoint(model);
    vTBN[0] = normalize(normalMatrix * aTangent.xyz); 
    vTBN[2] = normalize(normalMatrix * aNormal);
    vTBN[1] = cross(vTBN[0], vTBN[2]) * aTangent.w;
    
    vec4 outPos = model * vec4(aPos, 1.0);
    
    float scale = 1.0 / length(model[0].xyz);
    vec3 normalBias = aNormal * scale * 0.08;

    vLightSpaceFrag = uLightMatrix * (model * vec4(aPos + normalBias, 1.0));
    vLightSpaceFrag.xyz = vLightSpaceFrag.xyz * 0.5 + 0.5; // [-1,1] to [0, 1]

    vTexCoords  = aTexCoords; 
    gl_Position = uViewProj * outPos;
} 
