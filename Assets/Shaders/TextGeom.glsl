
layout (points) in; // Geometry shader will receive points
layout (triangle_strip, max_vertices = 4) out; // Output 4 vertices (triangle strip)

// per character texture
// x = half2:size, y = character: uint8, depth: uint8, scale: half
uniform highp usampler2D dataTex; 
uniform ivec2 uScrSize;
uniform int uIndexStart; // vertex index of the first text character that will be rendered 

out mediump vec2 vTexCoord;
out lowp vec4 vColor;

void main() 
{
    int quadID = uIndexStart + gl_PrimitiveIDIn;

    // read per quad data
    highp uvec4 data = texelFetch(dataTex, ivec2(quadID & 63, quadID >> 6), 0);
    // unpack per quad data
    lowp uint depth     = (data.y >> 8) & 0xFFu;
    lowp uint character = data.y & 0xFFu; // corresponds to ascii character, used for atlas indexing
    vec4  sizePos = vec4((data.xxww >> uvec4(0u, 16u, 0u, 16u)) & 0xFFFFu) / 10.0f;
    vec2  size   = sizePos.xy;
    vec2  pos    = sizePos.zw;

    float scale = unpackHalf2x16(data.y).y;
    vColor = unpackUnorm4x8(data.z);

    // ----    Create Vertex    ----
    vec2 vertices[4];
    vertices[0] = vec2(pos.x         , pos.y         );
    vertices[1] = vec2(pos.x         , pos.y + size.y);
    vertices[2] = vec2(pos.x + size.x, pos.y         );
    vertices[3] = vec2(pos.x + size.x, pos.y + size.y);

    // ----    Create UV    ----
    const mediump vec2 uvs[4] = vec2[4](
        vec2(0.0, 0.0),            
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0)
    );

    const float CellSize   = 12.0;
    const float cellWidth  = 1.0 / CellSize;
    const float cellHeight = 1.0 / CellSize;

    float u = float(character % 12u) / CellSize;
    float v = float(character / 12u) / CellSize;
    
    float CharSize = 48.0 * scale;
    float us = size.x / CharSize;
    float vs = size.y / CharSize;

    vec2 proj = 2.0 / vec2(uScrSize);
	float depthf = float(depth) / 255.0;
    for (int i = 0; i < 4; i++)
    {
        mediump vec2 uv = uvs[i];
    
        vec2 translate = proj * vertices[i] - 1.0;
        translate.y = -translate.y;
        gl_Position = vec4(translate, depthf, 1.0);
    
        // Calculate the UV coordinates within the character cell
        vTexCoord = vec2(u + (uv.x * cellWidth * us), v + (uv.y * cellHeight * vs));
        EmitVertex();
    }
    EndPrimitive();
}
