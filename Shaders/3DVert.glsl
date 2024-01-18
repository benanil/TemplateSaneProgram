
layout(location = 0) in highp vec3 aPos;
layout(location = 1) in mediump vec3 aNormal;
layout(location = 2) in mediump vec4 aTangent;
layout(location = 3) in highp vec2 aTexCoords;

out vec3 vFragPos;
out vec2 vTexCoords;
out mediump mat3 vTBN;

uniform mat4 mvp;
uniform mat4 model;

uniform vec3 viewPos;
uniform vec3 lightPos;

uniform int hasNormalMap;

// use gouard shading on android, calculate lighting in vertex shader
#ifdef __ANDROID__
vec3 PhongLighting(vec3 N, vec3 pos)
{
    // diffuse
    N = normalize(N); // this is using xyz10w1 format normalizing makes it better
    vec3  L       = normalize(lightPos - pos);
    float ndl     = max(dot(N, L), 0.0);
    // specular
    vec3  V     = normalize(viewPos - pos);
    float dotRV = pow(max(dot(reflect(L, N), -V), 0.0), 10.0);
    return vec3(ndl, dotRV, 0.0);
}
#endif

// https://developer.android.com/games/optimize/vertex-data-management
void main()
{
    vFragPos   = vec3(model * vec4(aPos, 1.0));   
    vTexCoords = aTexCoords; 
    
    vTBN[2] = aNormal; // if has no tangents use vertex normal
#ifndef __ANDROID__
    if (hasNormalMap == 1)
    {
        vec3 T = normalize(vec3(model * vec4(aTangent.xyz, 0.0)));
        vec3 N = normalize(vec3(model * vec4(aNormal  , 0.0)));
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T) * aTangent.w;

        vTBN = mat3(T, B, N);
    }
#else
    vTBN[0] = PhongLighting(aNormal, vFragPos);
#endif

    gl_Position = mvp * vec4(aPos, 1.0);
}