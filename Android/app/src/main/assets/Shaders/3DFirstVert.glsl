
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;  
layout(location = 2) in vec3 inNormal;  

out vec3 vnorm;
out vec2 vtexCoord;

uniform mat4 mvp;
uniform mat4 model;

void main()
{
    gl_Position = mvp * vec4(inPosition, 1.0);
    vnorm       = inNormal;
    vtexCoord   = inTexCoord; 
}