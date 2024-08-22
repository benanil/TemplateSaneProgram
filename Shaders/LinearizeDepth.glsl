
layout(location = 0) out float result;
in vec2 texCoord;

uniform vec3 clipInfo; // z_n * z_f,  z_n - z_f,  z_f
uniform highp sampler2D depthTexture;

// perspective
// ze = (zNear * zFar) / (zFar - zb * (zFar - zNear)); 
void main() 
{
    float depth = texture(depthTexture, texCoord).r;
    result = clipInfo.x / (clipInfo.z - depth * clipInfo.y);
}

