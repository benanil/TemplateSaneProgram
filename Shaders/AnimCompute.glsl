
layout(local_size_x = 32) in;

layout(rgba16f, binding = 0) readonly  uniform mediump image2D uAnimMatrix0;  // mat3x4[numJoints] 
layout(rgba16f, binding = 1) writeonly uniform mediump image2D uVertexMatricesOut; // mat3x4[numJoints] 

layout(location = 0) uniform vec2 FrameFrameBlend;

struct Vertex
{
    vec3 pos;
    uint normal;
    uvec4 packedData; // zw is joints, weights
};

layout(std140, binding = 0) buffer vbo
{
    Vertex vertices[];
} VertexBuffer;

void main() 
{
    int vertexIdx   = int(gl_GlobalInvocationID.x);

    int frame       = int(FrameFrameBlend.x);
    float frameBlend = FrameFrameBlend.y;

    uint jointsPacked = VertexBuffer.vertices[vertexIdx].packedData.z;

    ivec4 joints = ivec4(uvec4((jointsPacked & 0xffu),
                               (jointsPacked >> 8u)  & 0xffu,
                               (jointsPacked >> 16u) & 0xffu, 
                               (jointsPacked >> 24u) & 0xffu));
    
    mediump vec4 weights = unpackUnorm4x8(VertexBuffer.vertices[vertexIdx].packedData.w);
    
    // Working on Todo: 
    // 1: interpolate between two frames of animation like in vertex shader //< testing
    // 2: blend between two anamation
    mediump mat4 animA = mat4(0.0);
    {
        mediump mat4 mat0 = mat4(0.0);
        for (int i = 0; i < 4; i++)
        {
            int matIdx = joints[i] * 3; // 3 because our matrix is: RGBA16f x 3

            mat0 += transpose(mat4(
                        imageLoad(uAnimMatrix0, ivec2(matIdx + 0, frame)),
                        imageLoad(uAnimMatrix0, ivec2(matIdx + 1, frame)),
                        imageLoad(uAnimMatrix0, ivec2(matIdx + 2, frame)),
                        vec4(0.0, 0.0, 0.0, 1.0)
                    )) * weights[i]; 
        }

        mediump mat4 mat1 = mat4(0.0);
        for (int i = 0; i < 4; i++)
        {
            int matIdx = joints[i] * 3; // 3 because our matrix is: RGBA16f x 3

            mat1 += transpose(mat4(
                        imageLoad(uAnimMatrix0, ivec2(matIdx + 0, frame + 1)),
                        imageLoad(uAnimMatrix0, ivec2(matIdx + 1, frame + 1)),
                        imageLoad(uAnimMatrix0, ivec2(matIdx + 2, frame + 1)),
                        vec4(0.0, 0.0, 0.0, 1.0)
                    )) * weights[i]; 
        }

        animA[0] = mix(mat0[0], mat1[0], frameBlend);
        animA[1] = mix(mat0[1], mat1[1], frameBlend);
        animA[2] = mix(mat0[2], mat1[2], frameBlend);
        animA[3] = mix(mat0[3], mat1[3], frameBlend);
    }

    animA = transpose(animA);
    
    // reason why I'm using 600 is its divisible by 3 and big enough for x axis, 
    // also this number is less than 1024 that it might have caused problems with some devices otherwise.
    int x = vertexIdx * 3;
    int y = x / 600; 
    x = x % 600;
    imageStore(uVertexMatricesOut, ivec2(x + 0, y), animA[0]);
    imageStore(uVertexMatricesOut, ivec2(x + 1, y), animA[1]);
    imageStore(uVertexMatricesOut, ivec2(x + 2, y), animA[2]);
}
