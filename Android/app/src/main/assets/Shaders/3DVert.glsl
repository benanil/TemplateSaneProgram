
layout(location = 0) in highp   vec3 aPos;
layout(location = 1) in mediump vec3 aNormal;
layout(location = 2) in mediump vec4 aTangent;
layout(location = 3) in highp   vec2 aTexCoords;

out highp   vec3 vFragPos;
out highp   vec2 vTexCoords;
out highp   vec4 vLightSpaceFrag;
out mediump mat3 vTBN;

uniform highp mat4 mvp;
uniform highp mat4 model;
uniform highp mat4 lightMatrix;

uniform mediump vec3 viewPos;
uniform mediump vec3 sunDir;

uniform int hasNormalMap;

// https://developer.android.com/games/optimize/vertex-data-management
void main()
{
    vFragPos   = vec3(model * vec4(aPos, 1.0));
    vTexCoords = aTexCoords;

    vTBN[2] = normalize(aNormal); // if has no tangents use vertex normal
    #ifndef __ANDROID__
    if (hasNormalMap == 1)
    {
        vec3 T = normalize(vec3(model * vec4(aTangent.xyz, 0.0)));
        vec3 N = normalize(vec3(model * vec4(vTBN[2]  , 0.0)));
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T) * aTangent.w;

        vTBN = mat3(T, B, N);
    }
    #endif

    // for shadow, this matrix converts [-1, 1] space to [0, 1]
    const highp mat4 biasMatrix = mat4(
        0.5, 0.0, 0.0, 0.0,
        0.0, 0.5, 0.0, 0.0,
        0.0, 0.0, 0.5, 0.0,
        0.5, 0.5, 0.5, 1.0
    );

    vec3 normal = vTBN[2];
    float biasPosOffset = 0.5 + (1.0 - dot(normal, sunDir)); // clamp(tan(acos(ndl))*0.8, 0.0, 3.0)

    vLightSpaceFrag = biasMatrix * model * lightMatrix * vec4(aPos + (normal * biasPosOffset), 1.0);
    gl_Position = mvp * vec4(aPos, 1.0);
}