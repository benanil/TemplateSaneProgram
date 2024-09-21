
layout(location = 0) in highp vec3 aPos;
uniform mat4 uViewProj;
uniform vec3 viewPos;

out vec3 vPos;

void main()
{
    vPos = aPos;
    gl_Position = uViewProj * vec4(aPos * 2300.0 + viewPos, 1.0);
}