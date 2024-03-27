
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

uniform mediump sampler2D uAnimTex;
uniform mediump vec3 uSunDir;

uniform int uHasNormalMap;
uniform int uHasAnimation;

mat4 JointMatrixFromIndex(int x)
{
    x *= 3;
    int y = x / 600;
    x = x % 600;
    return transpose(mat4(
       texelFetch(uAnimTex, ivec2(x + 0, y), 0),
       texelFetch(uAnimTex, ivec2(x + 1, y), 0),
       texelFetch(uAnimTex, ivec2(x + 2, y), 0),
       vec4(0.0f, 0.0f, 0.0f, 1.0f)
    ));
}

// https://developer.android.com/games/optimize/vertex-data-management
void main()
{
    highp mat4 model = uModel;

    if (uHasAnimation > 0) {
        model = model * JointMatrixFromIndex(gl_VertexID);
    }

    vTBN[0] = normalize(vec3(model * vec4(aTangent.xyz, 0.0))); 
    vTBN[2] = normalize(vec3(model * vec4(aNormal, 0.0)));
    vTBN[1] = cross(vTBN[0], vTBN[2]) * aTangent.w;
    
    vec4 outPos = model * vec4(aPos, 1.0);
    mediump vec3 normal = vTBN[2];
    
    float biasPosOffset = 0.5 + (1.0 - dot(normal, uSunDir)); // clamp(tan(acos(dot(normal, uSunDir)))*0.8, 0.0, 3.0);
    vec3 normalBias = normal * clamp(biasPosOffset, -0.5, 1.0) * 0.7;
 
    vLightSpaceFrag = uLightMatrix * (model * vec4(aPos + normalBias, 1.0));
    vLightSpaceFrag.xyz = vLightSpaceFrag.xyz * 0.5 + 0.5;

    vTexCoords  = aTexCoords; 
    vFragPos    = outPos.xyz / outPos.w;
    gl_Position = uViewProj * outPos;
} 