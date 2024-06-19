
layout(location = 0) in highp   vec3  aPos;
layout(location = 1) in lowp    vec3  aNormal;
layout(location = 2) in lowp    vec4  aTangent;
layout(location = 3) in mediump vec2  aTexCoords;
layout(location = 4) in lowp    uvec4 aJoints; // lowp int ranges between 0-255 
layout(location = 5) in lowp    vec4  aWeights;

out mediump vec2 vTexCoords;
out highp   vec4 vLightSpaceFrag;
out lowp    mat3 vTBN;

uniform highp mat4 uModel;
uniform highp mat4 uLightMatrix;
uniform highp mat4 uViewProj;

uniform highp sampler2D uAnimTex;
uniform mediump vec3 uSunDir;

uniform int uHasNormalMap;
uniform int uHasAnimation;

// https://developer.android.com/games/optimize/vertex-data-management
void main()
{
    highp mat4 model = uModel;

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
        
        model = model * transpose(animMat);
    }

    lowp mat3 normalMatrix = mat3(model);
    vTBN[0] = normalize(normalMatrix * aTangent.xyz); 
    vTBN[2] = normalize(normalMatrix * aNormal);
    vTBN[1] = cross(vTBN[0], vTBN[2]) * aTangent.w;
    
    vec4 outPos = model * vec4(aPos, 1.0);
    
    // float biasPosOffset = 0.5 + (1.0 - dot(normal, uSunDir)); // clamp(tan(acos(dot(normal, uSunDir)))*0.8, 0.0, 3.0);
    vec3 normalBias = -uSunDir * 0.01; //normal * clamp(biasPosOffset, -0.5, 1.0);
 
    vLightSpaceFrag = uLightMatrix * (model * vec4(aPos + normalBias, 1.0));
    vLightSpaceFrag.xyz = vLightSpaceFrag.xyz * 0.5 + 0.5; // [-1,1] to [0, 1]

    vTexCoords  = aTexCoords; 
    gl_Position = uViewProj * outPos;
} 
