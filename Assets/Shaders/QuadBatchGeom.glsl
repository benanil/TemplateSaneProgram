
// per character textures
uniform highp usampler2D dataTex; // ivec2 uint32 = half2:size, rgba8:color

uniform ivec2 uScrSize;
uniform vec2 uScale;
uniform int uIndexStart; // vertex index of the first quad that will be rendered 

layout (points) in; // Geometry shader will receive points
layout (triangle_strip, max_vertices = 4) out; // Output 4 vertices (triangle strip)

layout (location = 0) out      lowp vec2 vTexCoord;
layout (location = 1) out      lowp vec4 vColor;
layout (location = 2) out      lowp float oFade;
layout (location = 3) out flat lowp uint  oEffect;
layout (location = 4) out flat lowp float oCutStart;

void main() 
{
    int quadID = uIndexStart + gl_PrimitiveIDIn;

    // unpack per quad data
    highp uvec4 data    = texelFetch(dataTex, ivec2(quadID, 0), 0);
                vColor  = unpackUnorm4x8(data.y);
    
    lowp vec2 depthCutStart = unpackUnorm4x8(data.z).xy;
    vec4  sizePos = vec4((data.xxww >> uvec4(0u, 16u, 0u, 16u)) & 0xFFFFu) / 10.0f;
    vec2  size   = sizePos.xy * uScale;
    vec2  pos    = sizePos.zw;

    // ----    Create Vertex    ----
    vec2 vertices[4];
    vertices[0] = vec2(pos.x         , pos.y         );
    vertices[1] = vec2(pos.x         , pos.y + size.y);
    vertices[2] = vec2(pos.x + size.x, pos.y         );
    vertices[3] = vec2(pos.x + size.x, pos.y + size.y);

    const lowp vec2 uvs[4] = vec2[4](
        vec2(0.0, 0.0),            
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0)
    );

    const lowp float fades[4] = float[](
        1.0, 1.0, 0.0, 0.0
    );

    vec2 proj = 2.0 / vec2(uScrSize);
    lowp float depth = depthCutStart.x;
    
    oEffect = 0xFFu & (data.z >> 24u);
    oCutStart = depthCutStart.y;

    for (int i = 0; i < 4; i++)
    {
        vec2 translate = proj * vertices[i] - 1.0;
        translate.y = -translate.y;
        
        gl_Position = vec4(translate, depth, 1.0);
        vTexCoord = uvs[i];
        
        oFade = fades[i];
        EmitVertex();
    }
    EndPrimitive();
}
