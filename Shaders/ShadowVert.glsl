
// fragment shader is just: "void main() { }"
layout(location = 0) in vec3 aPosition;

uniform mat4 model;
uniform mat4 lightMatrix;
uniform int uHasAnimation;
uniform highp sampler2D uAnimTex;

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

void main() 
{
    mat4 vmodel = model;
    if (uHasAnimation > 0)
    {
        vmodel = vmodel * JointMatrixFromIndex(gl_VertexID);
    }
    gl_Position = lightMatrix * (vmodel * vec4(aPosition, 1.0));
}