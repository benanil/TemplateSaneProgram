
layout(location = 0) in highp vec3 aPos;
uniform mat4 proj;
out vec3 pos;

void main()
{
    pos = aPos;
    gl_Position = proj * vec4(aPos * 500.0, 1.0);
}