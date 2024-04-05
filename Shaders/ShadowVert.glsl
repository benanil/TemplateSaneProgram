
// fragment shader is just: "void main() { }"
layout(location = 0) in highp   vec3  aPos;
layout(location = 1) in lowp    vec3  aNormal;
layout(location = 2) in lowp    vec4  aTangent;
layout(location = 3) in mediump vec2  aTexCoords;
layout(location = 4) in lowp    uvec4 aJoints; // lowp int ranges between 0-255 
layout(location = 5) in lowp    vec4  aWeights;

uniform mat4 model;
uniform mat4 lightMatrix;
uniform int uHasAnimation;
uniform highp sampler2D uAnimTex;

void main() 
{
    mat4 vmodel = model;
    if (uHasAnimation > 0)
    {
        mediump mat4 animMat = mat4(0.0);
        animMat[3].w = 1.0;

        for (int i = 0; i < 4; i++)
        {
            int matIdx = int(aJoints[i]) * 3; // 3 because our matrix is: RGBA16f x 3
            animMat[0] += texelFetch(uAnimTex, ivec2(matIdx + 0, 0), 0) * aWeights[i];
            animMat[1] += texelFetch(uAnimTex, ivec2(matIdx + 1, 0), 0) * aWeights[i];
            animMat[2] += texelFetch(uAnimTex, ivec2(matIdx + 2, 0), 0) * aWeights[i]; 
        }
        
        vmodel = vmodel * transpose(animMat);
    }
    gl_Position = lightMatrix * (vmodel * vec4(aPos, 1.0));
}