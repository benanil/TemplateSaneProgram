
layout(location = 0) out vec4 result;
in vec3 pos;

float Ease(float x) {
    return x * x * x * x;
}

void main()
{
    const vec4 skyColor = vec4(0.85, 0.44, 0.25, 1.0);
    result = skyColor * Ease(inversesqrt(dot(pos, pos)) * 0.42);
}