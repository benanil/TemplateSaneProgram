// not used in anywhere for now
layout(location = 0) out vec4 result;

in vec2 texCoord;

uniform vec4 projInfo; 
uniform vec2 InvFullResolution;
uniform sampler2D texLinearDepth;

//----------------------------------------------------------------------------------

vec3 UVToView(vec2 uv, float eye_z)
{
    return vec3((uv * projInfo.xy + projInfo.zw) * eye_z, eye_z);
}

vec3 FetchViewPos(vec2 UV)
{
    float ViewDepth = textureLod(texLinearDepth,UV,0).x;
    return UVToView(UV, ViewDepth);
}

vec3 MinDiff(vec3 P, vec3 Pr, vec3 Pl)
{
    vec3 V1 = Pr - P;
    vec3 V2 = P - Pl;
    return (dot(V1,V1) < dot(V2,V2)) ? V1 : V2;
}

vec3 ReconstructNormal(vec2 UV, vec3 P)
{
    vec3 Pr = FetchViewPos(UV + vec2(InvFullResolution.x, 0));
    vec3 Pl = FetchViewPos(UV + vec2(-InvFullResolution.x, 0));
    vec3 Pt = FetchViewPos(UV + vec2(0, InvFullResolution.y));
    vec3 Pb = FetchViewPos(UV + vec2(0, -InvFullResolution.y));
    return normalize(cross(MinDiff(P, Pr, Pl), MinDiff(P, Pt, Pb)));
}

//----------------------------------------------------------------------------------

void main() {
    vec3 P = FetchViewPos(texCoord);
    vec3 N = ReconstructNormal(texCoord, P);
    result = vec4(N * 0.5 + 0.5, 0.0);
}