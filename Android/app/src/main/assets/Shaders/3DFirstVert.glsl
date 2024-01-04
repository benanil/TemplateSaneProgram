
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTangent;
layout(location = 3) in vec2 aTexCoords;

out vec3 vFragPos;
out vec2 vTexCoords;
out mat3 vTBN;

uniform mat4 mvp;
uniform mat4 model;

uniform vec3 viewPos;
uniform vec3 lightPos;

uniform int hasNormalMap;

// https://developer.android.com/games/optimize/vertex-data-management
void main()
{
    vFragPos   = vec3(model * vec4(aPos, 1.0));
    vTexCoords = aTexCoords;

    vTBN[2] = aNormal; // if has no tangents use vertex normal

    if (hasNormalMap == 1)
    {
        // vec3 bitangent = cross(aNormal, aTangent);
        // vec3 T = aTangent; //normalize(vec3(model * vec4(aTangent , 0.0)));
        // vec3 N = aNormal; // normalize(vec3(model * vec4(aNormal  , 0.0)));
        // vec3 B = aNormal;//  normalize(vec3(model * vec4(bitangent, 0.0)));

        // vTBN = mat3(T, B, N);
    }

    gl_Position = mvp * vec4(aPos, 1.0);
}