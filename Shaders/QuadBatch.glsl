
// per character textures
uniform highp usampler2D dataTex; // ivec2 uint32 = half2:size, rgba8:color

uniform ivec2 uScrSize;
uniform vec2 uScale;
uniform int uIndexStart; // vertex index of the first quad that will be rendered 

layout (location = 0) out      lowp vec2 vTexCoord;
layout (location = 1) out      lowp vec4 vColor;
layout (location = 2) out      lowp float oFade;
layout (location = 3) out flat lowp uint  oEffect;
layout (location = 4) out flat lowp float oCutStart;

void main() 
{
    int quadID   = (uIndexStart + gl_VertexID) / 6;
    int vertexID = (uIndexStart + gl_VertexID) % 6;

    // unpack per quad data
    highp uvec4 data    = texelFetch(dataTex, ivec2(quadID, 0), 0);
                vColor  = unpackUnorm4x8(data.y);
    
    lowp vec2 depthCutStart = unpackUnorm4x8(data.z).xy;
    vec4  sizePos = vec4((data.xxww >> uvec4(0u, 16u, 0u, 16u)) & 0xFFFFu) / 10.0f;
    vec2  size   = sizePos.xy * uScale;
    vec2  pos    = sizePos.zw;
    // ----    Create Vertex    ----
    vec2 vertices[6];
    vertices[0] = vec2(pos.x         , pos.y         );
    vertices[1] = vec2(pos.x         , pos.y + size.y);
    vertices[2] = vec2(pos.x + size.x, pos.y         );
    vertices[4] = vec2(pos.x + size.x, pos.y + size.y);
    vertices[5] = vertices[2]; // reuse vertex 2
    vertices[3] = vertices[1]; // reuse vertex 1 

    vec2 proj = 2.0 / vec2(uScrSize);
    vec2 translate = proj * vertices[vertexID] - 1.0;
    translate.y = -translate.y;
    lowp float depth = depthCutStart.x;
    gl_Position = vec4(translate, depth, 1.0);

    // ----    Create UV    ----
    const lowp vec2 uvs[6] = vec2[6](
        vec2(0.0, 0.0),            
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0),
        vec2(1.0, 0.0)
    );

    vTexCoord = uvs[vertexID];

    const lowp float fades[6] = float[](
        1.0, 1.0, 0.0, 1.0, 0.0, 0.0
    );
    oEffect = 0xFFu & (data.z >> 24u);
    oFade = fades[vertexID];
    oCutStart = depthCutStart.y;
}
