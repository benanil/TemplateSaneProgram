
layout(location = 0) in highp   vec3 aPos;
layout(location = 1) in mediump vec2 aTex;
layout(location = 2) in mediump vec3 aNormal;

out mediump vec2 vTexCoords;
out lowp    mat3 vTBN;
out highp   vec4 vLightSpaceFrag;
out highp   vec3 vWorldPos;

uniform highp mat4 uModel;
uniform highp mat4 uLightMatrix;
uniform highp mat4 uViewProj;

// todo add shadows and maybe normal maps
void main()
{
    mediump mat3 normalMatrix = mat3(uModel);
    normalMatrix[0] = normalize(normalMatrix[0]);
    normalMatrix[1] = normalize(normalMatrix[1]);
    normalMatrix[2] = normalize(normalMatrix[2]);

    vec3 tangent = vec3(0.0, 1.0, 0.0); // aTangent.xyz
    vTBN[0] = tangent; // normalize(normalMatrix * aTangent.xyz);
    vTBN[2] = normalize(normalMatrix * aNormal);
    vTBN[1] = cross(vTBN[0], vTBN[2]); // * aTangent.w;
    
    vec4 outPos = uModel * vec4(aPos, 1.0);
    #if 0
    float scale = 1.0 / length(uModel[0].xyz);
    vec3 normalBias = aNormal * scale * 0.08;
    vLightSpaceFrag = uLightMatrix * (uModel * vec4(aPos + normalBias, 1.0));
    vLightSpaceFrag.xyz = vLightSpaceFrag.xyz * 0.5 + 0.5; // [-1,1] to [0, 1]
    #endif

    vTexCoords  = aTex;
    vWorldPos   = aPos;
    gl_Position = uViewProj * outPos;
}