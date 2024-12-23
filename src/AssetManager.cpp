/*********************************************************************************
*    Purpose:                                                                    *
*         Saves or Loads given FBX or GLTF scene, as binary                      *
*         Compresses Vertices for GPU using half precison and xyz10w2 format.    *
*         Uses zstd to reduce size on disk                                       *
*    Author:                                                                     *
*        Anilcan Gulkaya 2024 anilcangulkaya7@gmail.com github @benanil          *
*********************************************************************************/

#include "include/AssetManager.hpp"
#include "include/Platform.hpp"
#include "include/Renderer.hpp"
#include "include/Scene.hpp"

#if !AX_GAME_BUILD
	#include "../External/ufbx.h"
#endif

#include "../ASTL/Array.hpp"
#include "../ASTL/String.hpp"
#include "../ASTL/Math/Matrix.hpp"
#include "../ASTL/Math/Color.hpp"
#include "../ASTL/Additional/GLTFParser.hpp"
#include "../ASTL/IO.hpp"

#include "../External/zstd.h"

// https://copyprogramming.com/howto/how-to-pack-normals-into-gl-int-2-10-10-10-rev
inline uint32_t Pack_INT_2_10_10_10_REV(Vector3f v) {
    const uint32_t xs = v.x < 0.0f, ys = v.y < 0.0f, zs = v.z < 0.0f;   
    return zs << 29 | ((uint32_t)(v.z * 511 + (zs << 9)) & 511) << 20 |
           ys << 19 | ((uint32_t)(v.y * 511 + (ys << 9)) & 511) << 10 |
           xs << 9  | ((uint32_t)(v.x * 511 + (xs << 9)) & 511);
}

inline uint32_t Pack_INT_2_10_10_10_REV(Vector4x32f v)
{
	float x = VecGetX(v), y = VecGetY(v), z = VecGetZ(v), w = VecGetW(v); 

    const uint32_t xs = x < 0.0f, ys = y < 0.0f, zs = z < 0.0f, ws = w < 0.0f;
    return ws << 31 | ((uint32_t)(w       + (ws << 1)) & 1) << 30 |
           zs << 29 | ((uint32_t)(z * 511 + (zs << 9)) & 511) << 20 |
           ys << 19 | ((uint32_t)(y * 511 + (ys << 9)) & 511) << 10 |
           xs << 9  | ((uint32_t)(x * 511 + (xs << 9)) & 511);
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                              FBX LOAD                                    */
/*//////////////////////////////////////////////////////////////////////////*/

#if !AX_GAME_BUILD

static short GetFBXTexture(ufbx_material* umaterial,
                    ufbx_scene* uscene, 
                    ufbx_material_feature feature,
                    ufbx_material_pbr_map pbr,
                    ufbx_material_fbx_map fbx)
{
    if (umaterial->features.features[feature].enabled)
    {
        ufbx_texture* texture = nullptr;
        // search for normal texture
        if (umaterial->features.pbr.enabled)
            texture = umaterial->pbr.maps[pbr].texture;
        
        if (!texture)
            texture = umaterial->fbx.maps[fbx].texture;
        
        if (texture)
        {
            short textureIndex = IndexOf(uscene->textures.data, texture, (int)uscene->textures.count);
            ASSERT(textureIndex != -1); // we should find in textures
            return textureIndex;
        }
    }
    return -1;
}

static char* GetNameFromFBX(ufbx_string ustr, FixedSizeGrowableAllocator<char>& stringAllocator)
{
    if (ustr.length == 0) return nullptr;
    char* name = stringAllocator.AllocateUninitialized((int)ustr.length + 1);
    SmallMemCpy(name, ustr.data, ustr.length);
    name[ustr.length] = 0;
    return name;
}
#endif

int LoadFBX(const char* path, SceneBundle* fbxScene, float scale)
{
#if !AX_GAME_BUILD
    ufbx_load_opts opts = { 0 };
    opts.evaluate_skinning = false;
    opts.evaluate_caches = false;
    opts.load_external_files = false;
    opts.generate_missing_normals = true;
    opts.ignore_missing_external_files = true;
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f * (1.0f / scale);
    opts.obj_search_mtl_by_filename = true;

    opts.unicode_error_handling = UFBX_UNICODE_ERROR_HANDLING_ABORT_LOADING;
    
    ufbx_error error;
    ufbx_scene* uscene;
    
    uscene = ufbx_load_file(path, &opts, &error);
    
    if (!uscene) {
        AX_ERROR("fbx mesh load failed! %s", error.info);
        return 0;
    }    
    
    fbxScene->numMeshes     = (short)uscene->meshes.count;
    fbxScene->numNodes      = (short)uscene->nodes.count;
    fbxScene->numMaterials  = (short)uscene->materials.count;
    fbxScene->numImages     = (short)uscene->texture_files.count; 
    fbxScene->numTextures   = (short)uscene->textures.count;
    fbxScene->numCameras    = (short)uscene->cameras.count;
    fbxScene->totalVertices = 0;
    fbxScene->totalIndices  = 0;
    fbxScene->numScenes     = 0; // todo
    
    FixedSizeGrowableAllocator<char> stringAllocator(512);
    FixedSizeGrowableAllocator<int> intAllocator(32);
    
    uint64_t totalIndices  = 0, totalVertices = 0;
    for (int i = 0; i < fbxScene->numMeshes; i++)
    {
        ufbx_mesh* umesh = uscene->meshes[i];
        totalIndices  += umesh->num_triangles * 3;
        totalVertices += umesh->num_vertices;
    }
    
    fbxScene->allVertices = AllocAligned(sizeof(ASkinedVertex) * totalVertices, alignof(ASkinedVertex));
    fbxScene->allIndices  = AllocAligned(sizeof(uint32_t) * totalIndices, alignof(uint32_t));
    
    if (fbxScene->numMeshes) fbxScene->meshes = new AMesh[fbxScene->numMeshes]{};
    
    uint32_t* currentIndex = (uint32_t*)fbxScene->allIndices;
    ASkinedVertex* currentVertex = (ASkinedVertex*)fbxScene->allVertices;

    uint32_t vertexCursor = 0u, indexCursor = 0u;
    
    for (int i = 0; i < fbxScene->numMeshes; i++)
    {
        AMesh& amesh = fbxScene->meshes[i];
        ufbx_mesh* umesh = uscene->meshes[i];
        amesh.name = GetNameFromFBX(umesh->name, stringAllocator);
        amesh.primitives = nullptr;
        amesh.numPrimitives = 1;
        SBPush(amesh.primitives, {});
        
        APrimitive& primitive = amesh.primitives[0];
        primitive.numIndices  = (int)umesh->num_triangles * 3;
        primitive.numVertices = (int)umesh->num_vertices;
        primitive.indexType   = 5; //GraphicType_UnsignedInt;
        primitive.material    = 0; // todo
        primitive.indices     = currentIndex; 
        primitive.vertices    = currentVertex;
       
        primitive.attributes |= AAttribType_POSITION;
        primitive.attributes |= ((int)umesh->vertex_uv.exists << 1) & AAttribType_TEXCOORD_0;
        primitive.attributes |= ((int)umesh->vertex_normal.exists << 2) & AAttribType_NORMAL;
        
        for (int j = 0; j < primitive.numVertices; j++)
        {
            SmallMemCpy(&currentVertex[j].position.x, &umesh->vertex_position.values.data[j], sizeof(float) * 3);
            if (umesh->vertex_uv.exists) {
                currentVertex[j].texCoord = ConvertFloat2ToHalf2((float*)(umesh->vertex_uv.values.data + j));
            }
            if (umesh->vertex_normal.exists) {
                currentVertex[j].normal = Pack_INT_2_10_10_10_REV(Vec3((float*)(umesh->vertex_normal.values.data + j)));
            }
            if (umesh->vertex_tangent.exists)
            {
                Vector4x32f tangent = Vec3Load((float*)(umesh->vertex_normal.values.data + j));
                VecSetW(tangent, 1.0f);
                currentVertex[j].tangent = Pack_INT_2_10_10_10_REV(tangent);
            }
        }

        uint32_t* currIndices = (uint32_t*)primitive.indices;
        uint32_t indices[64] = {};
        
        for (int j = 0; j < umesh->faces.count; j++)
        {
            ufbx_face face = umesh->faces.data[j];
            uint32_t num_triangles = ufbx_triangulate_face(indices, ArraySize(indices), umesh, face);
            
            for (uint32_t tri_ix = 0; tri_ix < num_triangles; tri_ix++)
            {
                *currIndices++ = umesh->vertex_indices[indices[tri_ix * 3 + 0]] + vertexCursor;
                *currIndices++ = umesh->vertex_indices[indices[tri_ix * 3 + 1]] + vertexCursor;
                *currIndices++ = umesh->vertex_indices[indices[tri_ix * 3 + 2]] + vertexCursor;
            }
        }
            
        bool hasSkin = umesh->skin_deformers.count > 0;
        if (hasSkin)
        {
            primitive.attributes |= AAttribType_JOINTS | AAttribType_WEIGHTS;
            ufbx_skin_deformer* deformer = umesh->skin_deformers[0];
                
            for (int j = 0; j < deformer->vertices.count; j++)
            {
                uint32_t weightBegin = deformer->vertices[j].weight_begin;
                uint32_t weightResult = 0, shift = 0;
                uint32_t indexResult  = 0;
            
                for (uint32_t w = 0; w < deformer->vertices[j].num_weights && w < 4; w++, shift += 8)
                {
                    ufbx_skin_weight skinWeight = deformer->weights[weightBegin + w];
                    float    weight = skinWeight.weight;
                    uint32_t index  = skinWeight.cluster_index;
                    ASSERT(index < 255 && weight <= 1.0f);
                    weightResult |= (uint32_t)(weight * 255.0f) << shift;
                    indexResult  |= index << shift;
                }
            
                currentVertex[j].weights = weightResult;
                currentVertex[j].joints  = indexResult;
            }
        }
        
        fbxScene->totalIndices  += primitive.numIndices;
        fbxScene->totalVertices += primitive.numVertices;
        
        currentIndex  += primitive.numIndices;
        currentVertex += primitive.numVertices;
        
        primitive.indexOffset = indexCursor;
        vertexCursor += primitive.numVertices;
        indexCursor  += primitive.numIndices;
    }

    uint32_t numSkins = (uint32_t)uscene->skin_deformers.count;
    fbxScene->numSkins = numSkins;
    
    if (numSkins > 0) {
        fbxScene->skins = new ASkin[numSkins];
    }

    for (uint32_t d = 0; d < numSkins; d++)
    {
        ufbx_skin_deformer* deformer = uscene->skin_deformers[d];
        ASkin& skin = fbxScene->skins[d];
        uint32_t numJoints = (uint32_t)deformer->clusters.count;
        skin.numJoints = numJoints;
        skin.skeleton = 0;
        skin.inverseBindMatrices = (float*)new Matrix4[numJoints];
        skin.joints = intAllocator.AllocateUninitialized(numJoints+1);
    
        Matrix4* matrices = (Matrix4*)skin.inverseBindMatrices;
        for (uint32_t j = 0u; j < numJoints; j++)
        {
            ufbx_matrix uMatrix = deformer->clusters[j]->geometry_to_bone;
            matrices[j].r[0] = Vec3Load(&uMatrix.cols[0].x); 
            matrices[j].r[1] = Vec3Load(&uMatrix.cols[1].x); 
            matrices[j].r[2] = Vec3Load(&uMatrix.cols[2].x); 
            matrices[j].r[3] = Vec3Load(&uMatrix.cols[3].x); 
            VecSetW(matrices[j].r[3], 1.0f); 
        }
        
        for (uint32_t j = 0u; j < numJoints; j++)
        {
            skin.joints[j] = IndexOf(uscene->nodes.data, deformer->clusters[j]->bone_node, (int)uscene->nodes.count);
        }
    }
    
    short numImages = (short)uscene->texture_files.count;
    Array<AImage> images((int)uscene->texture_files.count, (int)uscene->texture_files.count);
    
    for (int i = 0; i < numImages; i++)
    {
        images[i].path = GetNameFromFBX(uscene->texture_files[i].filename, stringAllocator);
    }
    
    short numTextures = (short)uscene->textures.count;
    fbxScene->numTextures = numTextures;
    fbxScene->numSamplers = numTextures;
    
    if (numTextures) 
        fbxScene->textures = new ATexture[numTextures]{}, 
        fbxScene->samplers = new ASampler[numTextures]{};
    
    for (short i = 0; i < numTextures; i++)
    {
        ufbx_texture* utexture = uscene->textures[i];
        ATexture& atexture = fbxScene->textures[i];
        
        atexture.source  = utexture->has_file ? utexture->file_index : 0; // index to images array
        atexture.name    = GetNameFromFBX(utexture->name, stringAllocator);
        
        // is this embedded
        if (utexture->content.size)
        {
            char* buffer = stringAllocator.AllocateUninitialized(256);
            MemsetZero(buffer, 256);
            int pathLen = StringLength(path);
            SmallMemCpy(buffer, path, pathLen);
            
            char* fbxPath = PathGoBackwards(buffer, pathLen, false);
            // concat: FbxPath/TextureName
            SmallMemCpy(fbxPath, utexture->name.data, utexture->name.length); 
            SmallMemCpy(fbxPath + utexture->name.length, ".png", 4); // FbxPath/TextureName.png
            AFile file = AFileOpen(buffer, AOpenFlag_WriteBinary);
            AFileWrite(utexture->content.data, utexture->content.size, file);
            atexture.source = images.Size();
            images.Add( { buffer });
        }
        
        atexture.sampler = i;
        fbxScene->samplers[i].wrapS = utexture->wrap_u;
        fbxScene->samplers[i].wrapT = utexture->wrap_v;
    }
    
    short numMaterials = (short)uscene->materials.count;
    fbxScene->numMaterials = numMaterials;
    
    if (numMaterials) {
        fbxScene->materials = new AMaterial[numMaterials]{};
    }

    for (short i = 0; i < numMaterials; i++)
    {
        ufbx_material* umaterial = uscene->materials[i];
        AMaterial& amaterial = fbxScene->materials[i];
        
        ufbx_texture* normalTexture = nullptr;
        // search for normal texture
        if (umaterial->features.pbr.enabled) 
            normalTexture = umaterial->pbr.normal_map.texture;
        
        if (!normalTexture && umaterial->fbx.normal_map.has_value)
            normalTexture = umaterial->fbx.normal_map.texture;
        
        if (normalTexture)
        {
            short normalIndex = IndexOf(uscene->textures.data, normalTexture, (int)uscene->textures.count);
            ASSERT(normalIndex != -1); // we should find in textures
            amaterial.GetNormalTexture().index = normalIndex;
        }
        
        amaterial.GetOcclusionTexture().index = GetFBXTexture(umaterial, uscene, UFBX_MATERIAL_FEATURE_AMBIENT_OCCLUSION,
                                                                                 UFBX_MATERIAL_PBR_AMBIENT_OCCLUSION,
                                                                                 UFBX_MATERIAL_FBX_AMBIENT_COLOR);
        
        amaterial.GetEmissiveTexture().index = GetFBXTexture(umaterial, uscene, UFBX_MATERIAL_FEATURE_EMISSION,
                                                                                UFBX_MATERIAL_PBR_EMISSION_COLOR,
                                                                                UFBX_MATERIAL_FBX_EMISSION_COLOR);
        
        amaterial.baseColorTexture.index = GetFBXTexture(umaterial, uscene, UFBX_MATERIAL_FEATURE_PBR,
                                                                            UFBX_MATERIAL_PBR_BASE_COLOR,
                                                                            UFBX_MATERIAL_FBX_DIFFUSE_COLOR);
        if (amaterial.baseColorTexture.index == UINT16_MAX)
            amaterial.baseColorTexture.index = GetFBXTexture(umaterial, uscene, UFBX_MATERIAL_FEATURE_DIFFUSE,
                                                                                UFBX_MATERIAL_PBR_BASE_COLOR,
                                                                                UFBX_MATERIAL_FBX_DIFFUSE_COLOR);
        
        amaterial.specularTexture.index = GetFBXTexture(umaterial, uscene, UFBX_MATERIAL_FEATURE_SPECULAR,
                                                                           UFBX_MATERIAL_PBR_SPECULAR_COLOR,
                                                                           UFBX_MATERIAL_FBX_SPECULAR_COLOR);
        
        
        amaterial.metallicRoughnessTexture.index = GetFBXTexture(umaterial, uscene, UFBX_MATERIAL_FEATURE_DIFFUSE_ROUGHNESS,
                                                                                    UFBX_MATERIAL_PBR_ROUGHNESS,
                                                                                    UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT_FACTOR);
        
        amaterial.metallicFactor   = AMaterial::MakeFloat16(umaterial->pbr.metalness.value_real);
        amaterial.roughnessFactor  = AMaterial::MakeFloat16(umaterial->pbr.roughness.value_real);
        amaterial.baseColorFactor  = AMaterial::MakeFloat16(umaterial->pbr.base_factor.value_real);
        
        amaterial.specularFactor   = umaterial->features.pbr.enabled ? AMaterial::MakeFloat16(umaterial->pbr.specular_factor.value_real)
                                                                     : AMaterial::MakeFloat16(umaterial->fbx.specular_factor.value_real);
        amaterial.diffuseColor     = PackColor3PtrToUint(&umaterial->fbx.diffuse_color.value_real);
        amaterial.specularColor    = PackColor3PtrToUint(&umaterial->fbx.specular_color.value_real);
        
        amaterial.doubleSided = umaterial->features.double_sided.enabled;
        
        if (umaterial->pbr.emission_factor.value_components == 1)
        {
            amaterial.emissiveFactor[0] = amaterial.emissiveFactor[1] = amaterial.emissiveFactor[2] 
                 = AMaterial::MakeFloat16(umaterial->pbr.emission_factor.value_real);
        }
        else if (umaterial->pbr.emission_factor.value_components > 2)
        {
            amaterial.emissiveFactor[0] = AMaterial::MakeFloat16(umaterial->pbr.emission_factor.value_vec3.x);
            amaterial.emissiveFactor[1] = AMaterial::MakeFloat16(umaterial->pbr.emission_factor.value_vec3.y);
            amaterial.emissiveFactor[2] = AMaterial::MakeFloat16(umaterial->pbr.emission_factor.value_vec3.z);
        }
    }
    
    // copy nodes
    short numNodes = (short)uscene->nodes.count;
    fbxScene->numNodes = numNodes;
    
    if (numNodes) {
        fbxScene->nodes = new ANode[numNodes * 4]{};
    }
    for (int i = 0; i < numNodes; i++)
    {
        ANode& anode = fbxScene->nodes[i];
        ufbx_node* unode = uscene->nodes[i];
        anode.type = unode->camera != nullptr;
        anode.name = GetNameFromFBX(unode->name, stringAllocator);
        anode.numChildren = (int)unode->children.count;
        anode.children = intAllocator.AllocateUninitialized(anode.numChildren + 1);
        
        for (int j = 0; j < anode.numChildren; j++)
        {
            anode.children[j] = IndexOf(uscene->nodes.data, unode->children[j], (int)uscene->nodes.count);
            ASSERT(anode.children[j] != -1);
        }
        
        SmallMemCpy(anode.translation, &unode->world_transform.translation.x, sizeof(Vector3f));
        SmallMemCpy(anode.rotation, &unode->world_transform.rotation.x, sizeof(Vector4x32f));
        SmallMemCpy(anode.scale, &unode->world_transform.scale.x, sizeof(Vector3f));
        
        if (anode.type == 0)
        {
            anode.index = IndexOf(uscene->meshes.data, unode->mesh, (int)uscene->meshes.count);
            if (unode->materials.count > 0)
                fbxScene->meshes[anode.index].primitives[0].material = IndexOf(uscene->materials.data, unode->materials[0], (int)uscene->materials.count);
        }
        else
            anode.index = IndexOf(uscene->cameras.data, unode->camera, (int)uscene->cameras.count);
    }
    
    fbxScene->numImages       = images.Size();
    fbxScene->images          = images.TakeOwnership();
    fbxScene->stringAllocator = stringAllocator.TakeOwnership();
    fbxScene->intAllocator    = intAllocator.TakeOwnership();
    ufbx_free_scene(uscene);
#endif // android
    return 1;
}


/*//////////////////////////////////////////////////////////////////////////*/
/*                            Vertex Load                                   */
/*//////////////////////////////////////////////////////////////////////////*/


void CreateVerticesIndices(SceneBundle* gltf)
{
    AMesh* meshes = gltf->meshes;
    
    // pre allocate all vertices and indices 
    gltf->allVertices = AllocAligned(sizeof(AVertex) * gltf->totalVertices, alignof(AVertex));
    gltf->allIndices  = AllocAligned(gltf->totalIndices * sizeof(uint32_t) + 16, alignof(uint32)); // 16->give little bit of space for memcpy
    
    AVertex* currVertex = (AVertex*)gltf->allVertices;
    uint32_t* currIndex = (uint32_t*)gltf->allIndices;
    
    uint32_t vertexCursor = 0, indexCursor = 0;
    int count = 0;

    for (int m = 0; m < gltf->numMeshes; ++m)
    {
        // get number of vertex, getting first attribute count because all of the others are same
        AMesh mesh = meshes[m];
        for (uint64_t p = 0; p < mesh.numPrimitives; p++)
        {
            APrimitive& primitive = mesh.primitives[p];
            char* beforeCopy = (char*)primitive.indices;
            primitive.indices = currIndex;
            primitive.indexOffset = indexCursor;
            primitive.vertices = currVertex;
            
            // https://www.yosoygames.com.ar/wp/2018/03/vertex-formats-part-1-compression/
            Vector3f* positions = (Vector3f*)primitive.vertexAttribs[0];
            Vector2f* texCoords = (Vector2f*)primitive.vertexAttribs[1];
            Vector3f* normals   = (Vector3f*)primitive.vertexAttribs[2];
            Vector4x32f* tangents     = (Vector4x32f*)primitive.vertexAttribs[3];
            
            for (int v = 0; v < primitive.numVertices; v++)
            {
                Vector4x32f    tangent  = tangents  ? tangents[v]  : VecZero();
                Vector2f texCoord = texCoords ? texCoords[v] : Vector2f{0.0f, 0.0f};
                Vector3f normal   = normals   ? normals[v]   : Vector3f{0.5f, 0.5f, 0.0};

                currVertex[v].position  = positions[v];
                currVertex[v].texCoord  = ConvertFloat2ToHalf2(&texCoord.x);
                currVertex[v].normal    = Pack_INT_2_10_10_10_REV(normal);
                currVertex[v].tangent   = Pack_INT_2_10_10_10_REV(tangent);
            }

            int indexSize = GraphicsTypeToSize(primitive.indexType);
            
            for (int i = 0; i < primitive.numIndices; i++)
            {
                uint32_t index = 0;
                // index type might be ushort we are converting it to uint32 here.
                SmallMemCpy(&index, beforeCopy, indexSize);
                // we are combining all vertices and indices into one buffer, that's why we have to add vertex cursor
                currIndex[i] = index + vertexCursor;
                beforeCopy += indexSize;
            }

            currVertex += primitive.numVertices;

            primitive.indexOffset = indexCursor;
            indexCursor += primitive.numIndices;

            vertexCursor += primitive.numVertices;
            currIndex += primitive.numIndices;
        }
    }
    
    FreeGLTFBuffers(gltf);
}

void CreateVerticesIndicesSkined(SceneBundle* gltf)
{
    AMesh* meshes = gltf->meshes;
    
    // pre allocate all vertices and indices 
    gltf->allVertices = AllocAligned(sizeof(ASkinedVertex) * gltf->totalVertices, alignof(ASkinedVertex));
    gltf->allIndices  = AllocAligned(gltf->totalIndices * sizeof(uint32_t) + 16, alignof(uint32)); // 16->give little bit of space for memcpy
    
    ASkinedVertex* currVertex = (ASkinedVertex*)gltf->allVertices;
    uint32_t* currIndices = (uint32_t*)gltf->allIndices;
    
    int vertexCursor = 0;
    int indexCursor = 0;
    
    for (int m = 0; m < gltf->numMeshes; ++m)
    {
        // get number of vertex, getting first attribute count because all of the others are same
        AMesh mesh = meshes[m];
        for (uint64_t p = 0; p < mesh.numPrimitives; p++)
        {
            APrimitive& primitive = mesh.primitives[p];
            char* beforeCopy = (char*)primitive.indices;
            primitive.indices = currIndices;
            int indexSize = GraphicsTypeToSize(primitive.indexType);

            for (int i = 0; i < primitive.numIndices; i++)
            {
                uint32_t index = 0;
                SmallMemCpy(&index, beforeCopy, indexSize);
                // we are combining all vertices and indices into one buffer, that's why we have to add vertex cursor
                currIndices[i] = index + vertexCursor; 
                beforeCopy += indexSize;
            }
            
            // https://www.yosoygames.com.ar/wp/2018/03/vertex-formats-part-1-compression/
            primitive.vertices = currVertex;
            Vector3f* positions = (Vector3f*)primitive.vertexAttribs[0];
            Vector2f* texCoords = (Vector2f*)primitive.vertexAttribs[1];
            Vector3f* normals   = (Vector3f*)primitive.vertexAttribs[2];
            Vector4x32f*    tangents  = (Vector4x32f*)primitive.vertexAttribs[3];

            for (int v = 0; v < primitive.numVertices; v++)
            {
                Vector4x32f tangent     = tangents  ? tangents[v]  : VecZero();
                Vector2f texCoord = texCoords ? texCoords[v] : Vector2f{0.0f, 0.0f};
                Vector3f normal   = normals   ? normals[v]   : Vector3f{0.5f, 0.5f, 0.0};

                currVertex[v].position = positions[v];
                currVertex[v].texCoord = ConvertFloat2ToHalf2(&texCoord.x);
                currVertex[v].normal   = Pack_INT_2_10_10_10_REV(normal);
                currVertex[v].tangent  = Pack_INT_2_10_10_10_REV(tangent);
            }

            // convert whatever joint format to rgb8u
            char* joints  = (char*)primitive.vertexAttribs[5];
            char* weights = (char*)primitive.vertexAttribs[6];

            // size and offset in bytes
            int jointSize = GraphicsTypeToSize(primitive.jointType);
            int jointOffset = MAX((int)(primitive.jointStride - (jointSize * primitive.jointCount)), 0); // stride - sizeof(rgbau16)
            // size and offset in bytes
            int weightSize   = GraphicsTypeToSize(primitive.weightType);
            int weightOffset = MAX((int)(primitive.weightStride - (weightSize * primitive.jointCount)), 0);
            
            for (int j = 0; j < primitive.numVertices; j++)
            {
                // Combine 4 indices into one integer to save space
                uint32_t packedJoints = 0u;
                // iterate over joint indices, most of the time 4 indices
                for (int k = 0, shift = 0; k < primitive.jointCount; k++) 
                {
                    uint32_t jointIndex = 0;
                    SmallMemCpy(&jointIndex, joints, jointSize); 
                    ASSERT(jointIndex < 255u && "index has to be smaller than 255");
                    packedJoints |= jointIndex << shift;
                    shift += 8;
                    joints += jointSize;
                }

                uint32_t packedWeights;
                if (weightSize == 4) // if float, pack it directly
                {
                    packedWeights = PackColor4PtrToUint((float*)weights);
                    weights += weightSize * 4;
                }
                else
                {
                    for (int k = 0, shift = 0; k < primitive.jointCount && k < 4; k++, shift += 8)
                    {
                        uint32 jointWeight = 0u;
                        SmallMemCpy(&jointWeight, weights, weightSize); 
                        float weightMax = (float)((1u << (weightSize * 8)) - 1);
                        float norm = (float)jointWeight / weightMax; // divide by 255 or 65535
                        packedWeights |= uint32_t(norm * 255.0f) << shift;
                        weights += weightSize;
                    }
                }
                currVertex[j].joints  = packedJoints;
                currVertex[j].weights = packedWeights;

                if (currVertex[j].weights == 0)
                    currVertex[j].weights = 0XFF000000u;

                joints  += jointOffset; // stride offset at the end of the struct
                weights += weightOffset;
            }

            currVertex += primitive.numVertices;
            primitive.indexOffset = indexCursor;
            indexCursor += primitive.numIndices;
            
            vertexCursor += primitive.numVertices;
            currIndices  += primitive.numIndices;
        }
    }
    
    for (int s = 0; s < gltf->numSkins; s++)
    {
        ASkin& skin = gltf->skins[s];
        Matrix4* inverseBindMatrices = new Matrix4[skin.numJoints];
        SmallMemCpy(inverseBindMatrices, skin.inverseBindMatrices, sizeof(Matrix4) * skin.numJoints);
        skin.inverseBindMatrices = (float*)inverseBindMatrices;
    }

    if (gltf->numAnimations)
    {
        int totalSamplerInput = 0;
        for (int a = 0; a < gltf->numAnimations; a++)
            for (int s = 0; s < gltf->animations[a].numSamplers; s++)
                totalSamplerInput += gltf->animations[a].samplers[s].count;
        
        float* currSampler = new float[totalSamplerInput]{};
        Vector4x32f* currOutput  = new Vector4x32f[totalSamplerInput]{};

        for (int a = 0; a < gltf->numAnimations; a++)
        {
            for (int s = 0; s < gltf->animations[a].numSamplers; s++)
            {
                AAnimSampler& sampler = gltf->animations[a].samplers[s];
                SmallMemCpy(currSampler, sampler.input, sampler.count * sizeof(float));
                sampler.input = currSampler;
                currSampler += sampler.count;

                for (int i = 0; i < sampler.count; i++)
                {
                    SmallMemCpy(currOutput + i, sampler.output + (i * sampler.numComponent), sizeof(float) * sampler.numComponent);
                    // currOutput[i] = VecLoad(sampler.output + (i * sampler.numComponent));
                    // if (sampler.numComponent == 3) currOutput[i] = VecSetW(currOutput[i], 0.0f);
                }
                sampler.output = (float*)currOutput;
                currOutput += sampler.count;
            }
        }
    }

    FreeGLTFBuffers(gltf);
}


/*//////////////////////////////////////////////////////////////////////////*/
/*                            Binary Save                                   */
/*//////////////////////////////////////////////////////////////////////////*/

ZSTD_CCtx* zstdCompressorCTX = nullptr;
const int ABMMeshVersion = 42;

bool IsABMLastVersion(const char* path)
{
    if (!FileExist(path))
        return false;
    AFile file = AFileOpen(path, AOpenFlag_ReadBinary);
    if (AFileSize(file) < sizeof(short) * 16) 
        return false;
    int version = 0;
    AFileRead(&version, sizeof(int), file);
    uint64_t hex;
    AFileRead(&hex, sizeof(uint64_t), file);
    AFileClose(file);
    return version == ABMMeshVersion && hex == 0xABFABF;
}

static void WriteAMaterialTexture(AMaterial::Texture texture, AFile file)
{
    uint64_t data = texture.scale; data <<= sizeof(ushort) * 8;
    data |= texture.strength;      data <<= sizeof(ushort) * 8;
    data |= texture.index;         data <<= sizeof(ushort) * 8;
    data |= texture.texCoord;
    
    AFileWrite(&data, sizeof(uint64_t), file);
}

static void WriteGLTFString(const char* str, AFile file)
{
    int nameLen = str ? StringLength(str) : 0;
    AFileWrite(&nameLen, sizeof(int), file);
    if (str) AFileWrite(str, nameLen + 1, file);
}

int SaveGLTFBinary(SceneBundle* gltf, const char* path)
{
#if !AX_GAME_BUILD
    AFile file = AFileOpen(path, AOpenFlag_WriteBinary);
    
    int version = ABMMeshVersion;
    AFileWrite(&version, sizeof(int), file);
    
    uint64_t reserved[4] = { 0xABFABF };
    AFileWrite(&reserved, sizeof(uint64_t) * 4, file);
    
    AFileWrite(&gltf->scale, sizeof(float), file);
    AFileWrite(&gltf->numMeshes, sizeof(short), file);
    AFileWrite(&gltf->numNodes, sizeof(short), file);
    AFileWrite(&gltf->numMaterials,  sizeof(ushort), file);
    AFileWrite(&gltf->numTextures, sizeof(short), file);
    AFileWrite(&gltf->numImages, sizeof(short), file);
    AFileWrite(&gltf->numSamplers, sizeof(short), file);
    AFileWrite(&gltf->numCameras, sizeof(short), file);
    AFileWrite(&gltf->numScenes, sizeof(short), file);
    AFileWrite(&gltf->numSkins, sizeof(short), file);
    AFileWrite(&gltf->numAnimations, sizeof(short), file);
    AFileWrite(&gltf->defaultSceneIndex, sizeof(short), file);
    short isSkined = (short)(gltf->skins != nullptr);
    AFileWrite(&isSkined, sizeof(short), file);
    
    AFileWrite(&gltf->totalIndices, sizeof(int), file);
    AFileWrite(&gltf->totalVertices, sizeof(int), file);
    
    uint64_t vertexSize = isSkined ? sizeof(ASkinedVertex) : sizeof(AVertex);
    uint64_t allVertexSize = vertexSize * (uint64_t)gltf->totalVertices;
    uint64_t allIndexSize = (uint64_t)gltf->totalIndices * sizeof(uint32_t);
    
    // Compress and write, vertices and indices
    uint64_t compressedSize = (uint64_t)(allVertexSize * 0.9);
    char* compressedBuffer = new char[compressedSize];
    
    size_t afterCompSize = ZSTD_compress(compressedBuffer, compressedSize, gltf->allVertices, allVertexSize, 5);
    AFileWrite(&afterCompSize, sizeof(uint64_t), file);
    AFileWrite(compressedBuffer, afterCompSize, file);
    
    afterCompSize = ZSTD_compress(compressedBuffer, compressedSize, gltf->allIndices, allIndexSize, 5);
    AFileWrite(&afterCompSize, sizeof(uint64_t), file);
    AFileWrite(compressedBuffer, afterCompSize, file);

    delete[] compressedBuffer;
    // Note: anim morph targets aren't saved

    for (int i = 0; i < gltf->numMeshes; i++)
    {
        AMesh mesh = gltf->meshes[i];
        WriteGLTFString(mesh.name, file);
        
        AFileWrite(&mesh.numPrimitives  , sizeof(int), file);
        
        for (int j = 0; j < mesh.numPrimitives; j++)
        {
            APrimitive& primitive = mesh.primitives[j];
            AFileWrite(&primitive.attributes , sizeof(int), file);
            AFileWrite(&primitive.indexType  , sizeof(int), file);
            AFileWrite(&primitive.numIndices , sizeof(int), file);
            AFileWrite(&primitive.numVertices, sizeof(int), file);
            AFileWrite(&primitive.indexOffset, sizeof(int), file);
            AFileWrite(&primitive.jointType  , sizeof(short), file);
            AFileWrite(&primitive.jointCount , sizeof(short), file);
            AFileWrite(&primitive.jointStride, sizeof(short), file);
            AFileWrite(&primitive.material   , sizeof(ushort), file);
        }
    }
    
    for (int i = 0; i < gltf->numNodes; i++)
    {
        ANode& node = gltf->nodes[i];
        AFileWrite(&node.type       , sizeof(int), file);
        AFileWrite(&node.index      , sizeof(int), file);
        AFileWrite(&node.translation, sizeof(float) * 3, file);
        AFileWrite(&node.rotation   , sizeof(float) * 4, file);
        AFileWrite(&node.scale      , sizeof(float) * 3, file);
        AFileWrite(&node.numChildren, sizeof(int), file);
        
        if (node.numChildren)
            AFileWrite(node.children, sizeof(int) * node.numChildren, file);
        
        WriteGLTFString(node.name, file);
    }
    
    for (int i = 0; i < gltf->numMaterials; i++)
    {
        AMaterial& material = gltf->materials[i];
        for (int j = 0; j < 3; j++)
        {
            WriteAMaterialTexture(material.textures[j], file);
        }
        
        WriteAMaterialTexture(material.baseColorTexture, file);
        WriteAMaterialTexture(material.specularTexture, file);
        WriteAMaterialTexture(material.metallicRoughnessTexture, file);
        
        uint64_t data = material.emissiveFactor[0]; data <<= sizeof(short) * 8;
        data |= material.emissiveFactor[1];         data <<= sizeof(short) * 8;
        data |= material.emissiveFactor[2];         data <<= sizeof(short) * 8;
        data |= material.specularFactor;
        AFileWrite(&data, sizeof(uint64_t), file);
        
        data = (uint64_t(material.diffuseColor) << 32) | material.specularColor;
        AFileWrite(&data, sizeof(uint64_t), file);
        
        data = (uint64_t(material.baseColorFactor) << 32) | (uint64_t)material.doubleSided;
        AFileWrite(&data, sizeof(uint64_t), file);
        
        AFileWrite(&material.alphaCutoff, sizeof(float), file);
        AFileWrite(&material.alphaMode, sizeof(int), file);
        
        WriteGLTFString(material.name, file);
    }
    
    for (int i = 0; i < gltf->numTextures; i++)
    {
        ATexture texture = gltf->textures[i];
        AFileWrite(&texture.sampler, sizeof(int), file);
        AFileWrite(&texture.source, sizeof(int), file);
        WriteGLTFString(texture.name , file);
    }
    
    for (int i = 0; i < gltf->numImages; i++)
    {
        WriteGLTFString(gltf->images[i].path, file);
    }
    
    for (int i = 0; i < gltf->numSamplers; i++)
    {
    	AFileWrite(&gltf->samplers[i], sizeof(ASampler), file);
    }
    
    for (int i = 0; i < gltf->numCameras; i++)
    {
        ACamera camera = gltf->cameras[i];
        AFileWrite(&camera.aspectRatio, sizeof(float), file);
        AFileWrite(&camera.yFov, sizeof(float), file);
        AFileWrite(&camera.zFar, sizeof(float), file);
        AFileWrite(&camera.zNear, sizeof(float), file);
        AFileWrite(&camera.type, sizeof(int), file);
        WriteGLTFString(camera.name , file);
    }
    for (int i = 0; i < gltf->numScenes; i++)
    {
        AScene scene = gltf->scenes[i];
        WriteGLTFString(scene.name, file);
        AFileWrite(&scene.numNodes, sizeof(int), file);
        AFileWrite(scene.nodes, sizeof(int) * scene.numNodes, file);
    }
    
    for (int i = 0; i < gltf->numSkins; i++)
    {
        ASkin skin = gltf->skins[i];
        AFileWrite(&skin.skeleton, sizeof(int), file);
        AFileWrite(&skin.numJoints, sizeof(int), file);
        AFileWrite(skin.inverseBindMatrices, sizeof(Matrix4) * skin.numJoints, file);
        AFileWrite(skin.joints, sizeof(int) * skin.numJoints, file);
    }
    
    int totalAnimSamplerInput = 0;
    if (gltf->numAnimations > 0)
    {
        for (int a = 0; a < gltf->numAnimations; a++)
            for (int s = 0; s < gltf->animations[a].numSamplers; s++)
                totalAnimSamplerInput += gltf->animations[a].samplers[s].count;
    }

    AFileWrite(&totalAnimSamplerInput, sizeof(int), file);
    if (totalAnimSamplerInput > 0) {
        // all sampler input and outputs are allocated in one buffer each. at the end of the CreateVerticesIndicesSkined function
        AFileWrite(gltf->animations[0].samplers[0].input, sizeof(float) * totalAnimSamplerInput, file);
        AFileWrite(gltf->animations[0].samplers[0].output, sizeof(Vector4x32f) * totalAnimSamplerInput, file);
    }

    for (int i = 0; i < gltf->numAnimations; i++)
    {
        AAnimation animation = gltf->animations[i];
        AFileWrite(&animation.numSamplers, sizeof(int), file);
        AFileWrite(&animation.numChannels, sizeof(int), file);
        AFileWrite(&animation.duration, sizeof(float), file);
        AFileWrite(&animation.speed, sizeof(float), file);
        WriteGLTFString(animation.name, file);

        AFileWrite(animation.channels, sizeof(AAnimChannel) * animation.numChannels, file);
        
        for (int j = 0; j < animation.numSamplers; j++)
        {
            AFileWrite(&animation.samplers[j].count, sizeof(int), file);
            AFileWrite(&animation.samplers[j].numComponent, sizeof(int), file);
            AFileWrite(&animation.samplers[j].interpolation, sizeof(float), file);
        }
    }
    
    AFileClose(file);
#endif
    return 1;
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                            Binary Read                                   */
/*//////////////////////////////////////////////////////////////////////////*/

void ReadAMaterialTexture(AMaterial::Texture& texture, AFile file)
{
    uint64_t data;
    AFileRead(&data, sizeof(uint64_t), file);
    
    texture.texCoord = data & 0xFFFFu; data >>= sizeof(ushort) * 8;
    texture.index    = data & 0xFFFFu; data >>= sizeof(ushort) * 8;
    texture.strength = data & 0xFFFFu; data >>= sizeof(ushort) * 8;
    texture.scale    = data & 0xFFFFu;
}

void ReadGLTFString(char*& str, AFile file, FixedSizeGrowableAllocator<char>& stringAllocator)
{
    int nameLen = 0;
    AFileRead(&nameLen, sizeof(int), file);
    if (nameLen)    
    {
        str = stringAllocator.AllocateUninitialized(nameLen + 1);
        AFileRead(str, nameLen + 1, file);
        str[nameLen + 1] = 0;
    }
}

int LoadSceneBundleBinary(const char* path, SceneBundle* gltf)
{
    AFile file = AFileOpen(path, AOpenFlag_ReadBinary);
    if (!AFileExist(file))
    {
        perror("Failed to open file for writing");
        return 0;
    }
    
    FixedSizeGrowableAllocator<char> stringAllocator(1024);
    FixedSizeGrowableAllocator<int> intAllocator;
    
    int version = ABMMeshVersion;
    AFileRead(&version, sizeof(int), file);
    ASSERT(version == ABMMeshVersion);
    
    uint64_t reserved[4];
    AFileRead(&reserved, sizeof(uint64_t) * 4, file);
    
    AFileRead(&gltf->scale, sizeof(float), file);
    AFileRead(&gltf->numMeshes, sizeof(short), file);
    AFileRead(&gltf->numNodes, sizeof(short), file);
    AFileRead(&gltf->numMaterials, sizeof(ushort), file);
    AFileRead(&gltf->numTextures, sizeof(short), file);
    AFileRead(&gltf->numImages, sizeof(short), file);
    AFileRead(&gltf->numSamplers, sizeof(short), file);
    AFileRead(&gltf->numCameras, sizeof(short), file);
    AFileRead(&gltf->numScenes, sizeof(short), file);
    AFileRead(&gltf->numSkins, sizeof(short), file);
    AFileRead(&gltf->numAnimations, sizeof(short), file);
    AFileRead(&gltf->defaultSceneIndex, sizeof(short), file);
    short isSkined;
    AFileRead(&isSkined, sizeof(short), file);
    
    AFileRead(&gltf->totalIndices, sizeof(int), file);
    AFileRead(&gltf->totalVertices, sizeof(int), file);
    
    size_t vertexSize = isSkined ? sizeof(ASkinedVertex) : sizeof(AVertex);
    size_t vertexAlignment = isSkined ? alignof(ASkinedVertex) : alignof(AVertex);

    {
        uint64_t allVertexSize = gltf->totalVertices * vertexSize;
        uint64_t allIndexSize  = gltf->totalIndices * sizeof(uint32_t);
        
        gltf->allVertices = AllocAligned(vertexSize * gltf->totalVertices, vertexAlignment); // divide / 4 to get number of floats
        gltf->allIndices = AllocAligned(allIndexSize, alignof(uint32_t));
        
        char* compressedBuffer = new char[allVertexSize];
        uint64_t compressedSize;
        AFileRead(&compressedSize, sizeof(uint64_t), file);
        AFileRead(compressedBuffer, compressedSize, file);
        ZSTD_decompress(gltf->allVertices, allVertexSize, compressedBuffer, compressedSize);
        
        AFileRead(&compressedSize, sizeof(uint64_t), file);
        AFileRead(compressedBuffer, compressedSize, file);
        ZSTD_decompress(gltf->allIndices, allIndexSize, compressedBuffer, compressedSize);
        
        delete[] compressedBuffer;
    }
    
    char* currVertices = (char*)gltf->allVertices;
    char* currIndices = (char*)gltf->allIndices;
    
    if (gltf->numMeshes > 0) gltf->meshes = new AMesh[gltf->numMeshes]{};
    for (int i = 0; i < gltf->numMeshes; i++)
    {
        AMesh& mesh = gltf->meshes[i];
        ReadGLTFString(mesh.name, file, stringAllocator);
        
        AFileRead(&mesh.numPrimitives, sizeof(int), file);
        
        mesh.primitives = nullptr;
        
        for (int j = 0; j < mesh.numPrimitives; j++)
        {
            SBPush(mesh.primitives, {});
            APrimitive& primitive = mesh.primitives[j];
            AFileRead(&primitive.attributes , sizeof(int), file);
            AFileRead(&primitive.indexType  , sizeof(int), file);
            AFileRead(&primitive.numIndices , sizeof(int), file);
            AFileRead(&primitive.numVertices, sizeof(int), file);
            AFileRead(&primitive.indexOffset, sizeof(int), file);
            AFileRead(&primitive.jointType, sizeof(short), file);
            AFileRead(&primitive.jointCount, sizeof(short), file);
            AFileRead(&primitive.jointStride, sizeof(short), file);
            
            uint64_t indexSize = uint64_t(GraphicsTypeToSize(primitive.indexType)) * primitive.numIndices;
            primitive.indices = (void*)currIndices;
            currIndices += indexSize;
            
            uint64_t primitiveVertexSize = uint64_t(primitive.numVertices) * vertexSize;
            primitive.vertices = currVertices;
            currVertices += primitiveVertexSize;
            AFileRead(&primitive.material, sizeof(short), file);
            primitive.hasOutline = false; // always false 
        }
    }
    
    if (gltf->numNodes > 0) gltf->nodes = new ANode[gltf->numNodes]{};
    
    for (int i = 0; i < gltf->numNodes; i++)
    {
        ANode& node = gltf->nodes[i];
        AFileRead(&node.type       , sizeof(int), file);
        AFileRead(&node.index      , sizeof(int), file);
        AFileRead(&node.translation, sizeof(float) * 3, file);
        AFileRead(&node.rotation   , sizeof(float) * 4, file);
        AFileRead(&node.scale      , sizeof(float) * 3, file);
        AFileRead(&node.numChildren, sizeof(int), file);
        
        if (node.numChildren)
        {
            node.children = intAllocator.Allocate(node.numChildren+1);
            AFileRead(node.children, sizeof(int) * node.numChildren, file);
        }
        
        ReadGLTFString(node.name, file, stringAllocator);
    }
    
    if (gltf->numMaterials > 0) gltf->materials = new AMaterial[gltf->numMaterials]{};
    for (int i = 0; i < gltf->numMaterials; i++)
    {
        AMaterial& material = gltf->materials[i];
        for (int j = 0; j < 3; j++)
        {
            ReadAMaterialTexture(material.textures[j], file);
        }
        
        ReadAMaterialTexture(material.baseColorTexture, file);
        ReadAMaterialTexture(material.specularTexture, file);
        ReadAMaterialTexture(material.metallicRoughnessTexture, file);
        
        uint64_t data;
        AFileRead(&data, sizeof(uint64_t), file);
        
        material.specularFactor    = data & 0xFFFF; data >>= sizeof(short) * 8;
        material.emissiveFactor[2] = data & 0xFFFF; data >>= sizeof(short) * 8;
        material.emissiveFactor[1] = data & 0xFFFF; data >>= sizeof(short) * 8;
        material.emissiveFactor[0] = data & 0xFFFF; 
        
        AFileRead(&data, sizeof(uint64_t), file);
        material.diffuseColor  = (data >> 32);
        material.specularColor = data & 0xFFFFFFFF;
        
        AFileRead(&data, sizeof(uint64_t), file);
        material.baseColorFactor = (data >> 32);
        material.doubleSided     = data & 0x1;
        
        AFileRead(&material.alphaCutoff, sizeof(float), file);
        AFileRead(&material.alphaMode, sizeof(int), file);
        
        ReadGLTFString(material.name, file, stringAllocator);
    }
    
    if (gltf->numTextures > 0) gltf->textures = new ATexture[gltf->numTextures]{};
    for (int i = 0; i < gltf->numTextures; i++)
    {
        ATexture& texture = gltf->textures[i];
        AFileRead(&texture.sampler, sizeof(int), file);
        AFileRead(&texture.source, sizeof(int), file);
        ReadGLTFString(texture.name, file, stringAllocator);
    }
    if (gltf->numImages > 0) gltf->images = new AImage[gltf->numImages]{};
    for (int i = 0; i < gltf->numImages; i++)
    {
        ReadGLTFString(gltf->images[i].path, file, stringAllocator);
    }
    
    if (gltf->numSamplers > 0) gltf->samplers = new ASampler[gltf->numSamplers]{};
    for (int i = 0; i < gltf->numSamplers; i++)
    {
        AFileRead(&gltf->samplers[i], sizeof(ASampler), file);
    }
    
    if (gltf->numCameras > 0) gltf->cameras = new ACamera[gltf->numCameras]{};
    for (int i = 0; i < gltf->numCameras; i++)
    {
        ACamera& camera = gltf->cameras[i];
        AFileRead(&camera.aspectRatio, sizeof(float), file);
        AFileRead(&camera.yFov, sizeof(float), file);
        AFileRead(&camera.zFar, sizeof(float), file);
        AFileRead(&camera.zNear, sizeof(float), file);
        AFileRead(&camera.type, sizeof(int), file);
        ReadGLTFString(camera.name, file, stringAllocator);
    }
    
    if (gltf->numScenes > 0) gltf->scenes = new AScene[gltf->numScenes]{};
    for (int i = 0; i < gltf->numScenes; i++)
    {
        AScene& scene = gltf->scenes[i];
        ReadGLTFString(scene.name, file, stringAllocator);
        AFileRead(&scene.numNodes, sizeof(int), file);
        scene.nodes = intAllocator.Allocate(scene.numNodes);
        AFileRead(scene.nodes, sizeof(int) * scene.numNodes, file);
    }

    if (gltf->numSkins > 0) gltf->skins = new ASkin[gltf->numSkins]{};
    for (int i = 0; i < gltf->numSkins; i++)
    {
        ASkin& skin = gltf->skins[i];
        AFileRead(&skin.skeleton, sizeof(int), file);
        AFileRead(&skin.numJoints, sizeof(int), file);
        skin.inverseBindMatrices = (float*)new Matrix4[skin.numJoints];
        skin.joints = new int[skin.numJoints];
        AFileRead(skin.inverseBindMatrices, sizeof(Matrix4) * skin.numJoints, file);
        AFileRead(skin.joints, sizeof(int) * skin.numJoints, file);
    }

    int totalAnimSamplerInput = 0;
    AFileRead(&totalAnimSamplerInput, sizeof(int), file);
    float* currSamplerInput;
    Vector4x32f* currSamplerOutput;

    if (totalAnimSamplerInput) {
        currSamplerInput = new float[totalAnimSamplerInput]{};
        currSamplerOutput = new Vector4x32f[totalAnimSamplerInput]{};
        AFileRead(currSamplerInput, sizeof(float) * totalAnimSamplerInput, file);
        AFileRead(currSamplerOutput, sizeof(Vector4x32f) * totalAnimSamplerInput, file);
    }

    if (gltf->numAnimations) gltf->animations = new AAnimation[gltf->numAnimations]{};
    for (int i = 0; i < gltf->numAnimations; i++)
    {
        AAnimation& animation = gltf->animations[i];

        AFileRead(&animation.numSamplers, sizeof(int), file);
        AFileRead(&animation.numChannels, sizeof(int), file);
        AFileRead(&animation.duration, sizeof(float), file);
        AFileRead(&animation.speed, sizeof(float), file);
        ReadGLTFString(animation.name, file, stringAllocator);
        animation.channels = new AAnimChannel[animation.numChannels];
        AFileRead(animation.channels, sizeof(AAnimChannel) * animation.numChannels, file);
        animation.samplers = new AAnimSampler[animation.numSamplers];

        for (int j = 0; j < animation.numSamplers; j++)
        {
            AFileRead(&animation.samplers[j].count, sizeof(int), file);
            AFileRead(&animation.samplers[j].numComponent, sizeof(int), file);
            AFileRead(&animation.samplers[j].interpolation, sizeof(float), file);
            int count = animation.samplers[j].count;
            animation.samplers[j].input = currSamplerInput;
            animation.samplers[j].output = (float*)currSamplerOutput;
            currSamplerInput += count;
            currSamplerOutput += count;
        }
    }

    AFileClose(file);
    
    gltf->stringAllocator = stringAllocator.TakeOwnership();
    gltf->intAllocator    = intAllocator.TakeOwnership();
    return 1;
}