
layout(location = 0) in highp   vec3  aPos;
layout(location = 1) in lowp    vec3  aNormal;
layout(location = 2) in lowp    vec4  aTangent;
layout(location = 3) in mediump vec2  aTexCoords;
layout(location = 4) in lowp    uvec4 aJoints; // lowp int ranges between 0-255 
layout(location = 5) in lowp    vec4  aWeights;

out highp   vec3 vFragPos;
out mediump vec2 vTexCoords;
out highp   vec4 vLightSpaceFrag;
out lowp    mat3 vTBN;

uniform highp mat4 uModel;
uniform highp mat4 uLightMatrix;
uniform highp mat4 uViewProj;

uniform highp mat4 uInvBindMatrices[32];
uniform highp mat4 uJointMatrices[32];

uniform mediump vec3 uSunDir;

uniform int uHasNormalMap;
uniform int uHasSkin;

// https://developer.android.com/games/optimize/vertex-data-management
void main()
{
    highp mat4 model = uModel;
    vec4 weights = normalize(aWeights);

    if (uHasSkin == 1)
    {
        model = (uJointMatrices[aJoints.x] * weights.x)
              + (uJointMatrices[aJoints.y] * weights.y)
              + (uJointMatrices[aJoints.z] * weights.z)
              + (uJointMatrices[aJoints.w] * weights.w);
    }
    vFragPos   = vec3(model * vec4(aPos, 1.0));
    vTexCoords = aTexCoords; 
    
    lowp mat3 lowModel = mat3(normalize(model[0].xyz),
                              normalize(model[1].xyz),
                              normalize(model[2].xyz));
    lowp vec3 normal = (lowModel * normalize(aNormal));
    vTBN[2] = normal; // if has no tange.nts use vertex normal

#ifndef __ANDROID__
    if (uHasNormalMap == 1)
    {
        lowp vec3 T = (lowModel * normalize(aTangent.xyz));
        lowp vec3 N = normal;
        T = normalize(T - dot(T, N) * N);
        lowp vec3 B = cross(N, T) * aTangent.w;

        vTBN = mat3(T, B, N);
    }
#endif
    
    // for shadow, this matrix converts [-1, 1] space to [0, 1]
    const mat4 biasMatrix = mat4( 
        0.5, 0.0, 0.0, 0.0,
        0.0, 0.5, 0.0, 0.0,
        0.0, 0.0, 0.5, 0.0,
        0.5, 0.5, 0.5, 1.0
    );

    float biasPosOffset = 0.5 + (1.0 - dot(normal, uSunDir)); // clamp(tan(acos(ndl))*0.8, 0.0, 3.0)

    vLightSpaceFrag = biasMatrix * model * uLightMatrix * vec4(aPos + (normal * biasPosOffset), 1.0);

    gl_Position = uViewProj * (model * vec4(aPos, 1.0));//vec4(locPos.xyz / locPos.w, 1.0);
}