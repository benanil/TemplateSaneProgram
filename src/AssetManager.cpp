/*********************************************************************************
*    Purpose:                                                                    *
*         Saves or Loads given FBX or GLTF scene, as binary                      *
*         Compresses Vertices using half precison and xyz10w2 format.            *
*         Uses zstd to reduce size on disk                                       *
*    Author:                                                                     *
*        Anilcan Gulkaya 2024 anilcangulkaya7@gmail.com github @benanil          *
*********************************************************************************/

#include "AssetManager.hpp"
#include "Platform.hpp"
#include "Renderer.hpp"

#if !AX_GAME_BUILD
	#include "../External/ufbx.h"
#endif

#include "../ASTL/Array.hpp"
#include "../ASTL/String.hpp"
#include "../ASTL/Math/Matrix.hpp"
#include "../ASTL/Additional/GLTFParser.hpp"
#include "../ASTL/IO.hpp"

#include "../External/zstd.h"

/*//////////////////////////////////////////////////////////////////////////*/
/*                              FBX LOAD                                    */
/*//////////////////////////////////////////////////////////////////////////*/

#if !AX_GAME_BUILD

struct FBXVertex { float pos[3]; float texCoord[2]; float normal[3]; };

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
	char* name = stringAllocator.AllocateUninitialized(ustr.length + 1);
	SmallMemCpy(name, ustr.data, ustr.length);
	name[ustr.length] = 0;
	return name;
}
#endif

int LoadFBX(const char* path, ParsedGLTF* fbxScene, float scale)
{
#if !AX_GAME_BUILD
	ufbx_load_opts opts = { 0 };
	opts.evaluate_skinning = false;
	opts.evaluate_caches = false;
	opts.load_external_files = false;
	opts.generate_missing_normals = true;
	opts.ignore_missing_external_files = true;
	opts.target_axes = ufbx_axes_right_handed_y_up;
	opts.target_unit_meters = (ufbx_real)0.01;
	opts.obj_search_mtl_by_filename = true;
    
	opts.unicode_error_handling = UFBX_UNICODE_ERROR_HANDLING_ABORT_LOADING;

	ufbx_error error;
	ufbx_scene* uscene;

	uscene = ufbx_load_file(path, &opts, &error);

	if (!uscene)
	{
		AX_ERROR("fbx mesh load failed! %s", error.info);
		return 0;
	}    

	fbxScene->numMeshes    = (short)uscene->meshes.count;
	fbxScene->numNodes     = (short)uscene->nodes.count;
	fbxScene->numMaterials = (short)uscene->materials.count;
	fbxScene->numImages    = (short)uscene->texture_files.count; 
	fbxScene->numTextures  = (short)uscene->textures.count;
	fbxScene->numCameras   = (short)uscene->cameras.count;
	fbxScene->numScenes    = 1;// todo

	FixedSizeGrowableAllocator<char> stringAllocator(512);
	FixedSizeGrowableAllocator<int> intAllocator(32);

	uint64_t totalIndices  = 0, totalVertices = 0;
	for (int i = 0; i < fbxScene->numMeshes; i++)
	{
		ufbx_mesh* umesh = uscene->meshes[i];
		totalIndices  += umesh->num_triangles * 3;
		totalVertices += umesh->num_vertices;
	}

	fbxScene->allVertices = new FBXVertex[totalVertices];
	fbxScene->allIndices  = AllocAligned(totalIndices * sizeof(uint32_t), alignof(uint32_t));

	if (fbxScene->numMeshes) fbxScene->meshes = new AMesh[fbxScene->numMeshes]{};
	
	uint32_t*  currentIndex  = (uint32_t*)fbxScene->allIndices;
	FBXVertex* currentVertex = (FBXVertex*)fbxScene->allVertices;

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
		
		currentIndex  += primitive.numIndices;
		currentVertex += primitive.numVertices;

		primitive.attributes |= AAttribType_POSITION;
		primitive.attributes |= ((int)umesh->vertex_uv.exists << 1) & AAttribType_TEXCOORD_0;
		primitive.attributes |= ((int)umesh->vertex_normal.exists << 2) & AAttribType_NORMAL;

		FBXVertex* currVertex = (FBXVertex*)primitive.vertices;

		for (int j = 0; j < primitive.numVertices; j++)
		{
			SmallMemCpy(currVertex->pos, &umesh->vertex_position.values.data[j], sizeof(float) * 3);
			if (umesh->vertex_uv.exists) SmallMemCpy(currVertex->texCoord, umesh->vertex_uv.values.data + j, sizeof(float) * 2);
			if (umesh->vertex_normal.exists) SmallMemCpy(currVertex->normal, umesh->vertex_normal.values.data + j, sizeof(float) * 3);
			currVertex++;
		}

		uint32_t* currIndice = (uint32_t*)primitive.indices;
		uint32_t indices[64]{};

		for (int j = 0; j < umesh->faces.count; j++)
		{
			ufbx_face face = umesh->faces.data[j];
			uint32_t num_triangles = ufbx_triangulate_face(indices, 64, umesh, face);

			for (uint32_t tri_ix = 0; tri_ix < num_triangles; tri_ix++)
			{
				*currIndice++ = umesh->vertex_indices[indices[tri_ix * 3 + 0]];
				*currIndice++ = umesh->vertex_indices[indices[tri_ix * 3 + 1]];
				*currIndice++ = umesh->vertex_indices[indices[tri_ix * 3 + 2]];
			}
		}
	}

	short numImages = (short)uscene->texture_files.count;
	Array<AImage> images((int)uscene->texture_files.count, (int)uscene->texture_files.count);
	
	for (int i = 0; i < numImages; i++)
	{
		images[i].path = GetNameFromFBX(uscene->texture_files[i].filename, stringAllocator);
	}
	
	// todo: textures
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
			AFile file = AFileOpen(buffer, AOpenFlag_Write);
			AFileWrite(utexture->content.data, utexture->content.size, file);
			atexture.source = images.Size();
			images.Add( { buffer });
		}
	
		atexture.sampler = i;
		fbxScene->samplers[i].wrapS = utexture->wrap_u;
		fbxScene->samplers[i].wrapT = utexture->wrap_v;
	}
	
	// todo: materials
	short numMaterials = uscene->materials.count;
	fbxScene->numMaterials = numMaterials;
    
	if (numMaterials) fbxScene->materials = new AMaterial[numMaterials]{};
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
		if (amaterial.baseColorTexture.index == -1)
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
		amaterial.diffuseColor     = PackColorRGBU32(&umaterial->fbx.diffuse_color.value_real);
		amaterial.specularColor    = PackColorRGBU32(&umaterial->fbx.specular_color.value_real);
        
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
	
	if (numNodes) fbxScene->nodes = new ANode[numNodes]{};
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

		SmallMemCpy(anode.translation, &unode->world_transform, sizeof(ufbx_transform));

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

// https://copyprogramming.com/howto/how-to-pack-normals-into-gl-int-2-10-10-10-rev
inline uint32_t Pack_INT_2_10_10_10_REV(Vector3f v)
{
	const uint32_t xs = v.x < 0;
	const uint32_t ys = v.y < 0;
	const uint32_t zs = v.z < 0;
	uint32_t vi =
	       zs << 29 | ((uint32_t)(v.z * 511 + (zs << 9)) & 511) << 20 |
	       ys << 19 | ((uint32_t)(v.y * 511 + (ys << 9)) & 511) << 10 |
	       xs << 9  | ((uint32_t)(v.x * 511 + (xs << 9)) & 511);
	return vi;
}

void CreateVerticesIndices(ParsedGLTF* gltf)
{
	AMesh* meshes = gltf->meshes;
	
	// BYTE, UNSIGNED_BYTE, SHORT, UNSIGNED_SHORT, INT, UNSIGNED_INT, FLOAT
	const int TypeToSize[8] = { 1, 1, 2, 2, 4, 4, 4 };
	
	// pre allocate all vertices and indices 
	gltf->allVertices = new AVertex[gltf->totalVertices];
	gltf->allIndices  = AllocAligned(gltf->totalIndices * sizeof(uint32_t) + 16, alignof(uint32)); // 16->give little bit of space for memcpy
	
	AVertex* currVertex = (AVertex*)gltf->allVertices;
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
			uint32_t* beforeCopy = (uint32_t*)primitive.indices;
			primitive.indices = currIndices;
			
			for (int i = 0; i < primitive.numIndices; i++)
			{
				currIndices[i] = beforeCopy[i] + vertexCursor;
			}

			primitive.indexOffset = indexCursor;
			indexCursor += primitive.numIndices;

			vertexCursor += primitive.numVertices;
			currIndices  += primitive.numIndices;

			// https://www.yosoygames.com.ar/wp/2018/03/vertex-formats-part-1-compression/
			primitive.vertices = currVertex;
			Vector3f* positions = (Vector3f*)primitive.vertexAttribs[0];
			Vector2f* texCoords = (Vector2f*)primitive.vertexAttribs[1];
			Vector3f* normals   = (Vector3f*)primitive.vertexAttribs[2];
			Vector4f* tangents  = (Vector4f*)primitive.vertexAttribs[3];
			
			for (int v = 0; v < primitive.numVertices; v++)
			{
				Vector4f tangent = tangents ? tangents[v] : Vector4f::Zero();
				currVertex[v].position  = positions[v];
				currVertex[v].texCoord  = texCoords[v];
				currVertex[v].normal    = Pack_INT_2_10_10_10_REV(normals[v]);
				ConvertFloatToHalf(currVertex[v].tangent, &tangent.x, 4);
			}
			currVertex += primitive.numVertices;
		}
	}

	for (int i = 0; i < gltf->numBuffers; i++)
	{
		FreeAllText((char*)gltf->buffers[i].uri);
		gltf->buffers[i].uri = nullptr;
	}
	delete[] gltf->buffers;
	gltf->numBuffers = 0;
	gltf->buffers = nullptr;
}

/*//////////////////////////////////////////////////////////////////////////*/
/*                            Binary Save                                   */
/*//////////////////////////////////////////////////////////////////////////*/

ZSTD_CCtx* zstdCompressorCTX = nullptr;
const int ABMMeshVersion = 19;

bool IsABMLastVersion(const char* path)
{
	AFile file = AFileOpen(path, AOpenFlag_Read);
	if (AFileSize(file) < sizeof(short) * 12) 
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
	uint64_t data = texture.scale; data <<= sizeof(short) * 8;
	data |= texture.strength;      data <<= sizeof(short) * 8;
	data |= texture.index;         data <<= sizeof(short) * 8;
	data |= texture.texCoord;

	AFileWrite(&data, sizeof(uint64_t), file);
}

static void WriteGLTFString(const char* str, AFile file)
{
	int nameLen = str ? StringLength(str) : 0;
	AFileWrite(&nameLen, sizeof(int), file);
	if (str) AFileWrite(str, nameLen + 1, file);
}

int SaveGLTFBinary(ParsedGLTF* gltf, const char* path)
{
#if !AX_GAME_BUILD
	AFile file = AFileOpen(path, AOpenFlag_Write);

	int version = ABMMeshVersion;
	AFileWrite(&version, sizeof(int), file);
    
	uint64_t reserved[4] = { 0xABFABF };
	AFileWrite(&reserved, sizeof(uint64_t) * 4, file);
   
	AFileWrite(&gltf->scale, sizeof(float), file);
	AFileWrite(&gltf->numMeshes, sizeof(short), file);
	AFileWrite(&gltf->numNodes, sizeof(short), file);
	AFileWrite(&gltf->numMaterials,  sizeof(short), file);
	AFileWrite(&gltf->numTextures, sizeof(short), file);
	AFileWrite(&gltf->numImages, sizeof(short), file);
	AFileWrite(&gltf->numSamplers, sizeof(short), file);
	AFileWrite(&gltf->numCameras, sizeof(short), file);
	AFileWrite(&gltf->numScenes, sizeof(short), file);
	AFileWrite(&gltf->defaultSceneIndex, sizeof(short), file);
	
	AFileWrite(&gltf->totalIndices, sizeof(int), file);
	AFileWrite(&gltf->totalVertices, sizeof(int), file);
	
	uint64_t allVertexSize = (uint64_t)gltf->totalVertices * sizeof(AVertex);
	uint64_t allIndexSize = (uint64_t)gltf->totalIndices * sizeof(uint32_t);

	// Compress and write, vertices and indices
	uint64_t compressedSize = allVertexSize * 0.9;
	char* compressedBuffer = new char[compressedSize];
	
	size_t afterCompSize = ZSTD_compress(compressedBuffer, compressedSize, gltf->allVertices, allVertexSize, 5);
	AFileWrite(&afterCompSize, sizeof(uint64_t), file);
	AFileWrite(compressedBuffer, afterCompSize, file);
	
	afterCompSize = ZSTD_compress(compressedBuffer, compressedSize, gltf->allIndices, allIndexSize, 5);
	AFileWrite(&afterCompSize, sizeof(uint64_t), file);
	AFileWrite(compressedBuffer, afterCompSize, file);
	
	delete[] compressedBuffer;
	
	for (int i = 0; i < gltf->numMeshes; i++)
	{
		AMesh mesh = gltf->meshes[i];
		WriteGLTFString(mesh.name, file);
        
		AFileWrite(&mesh.numPrimitives, sizeof(int), file);
        
		for (int j = 0; j < mesh.numPrimitives; j++)
		{
			APrimitive& primitive = mesh.primitives[j];
			AFileWrite(&primitive.attributes , sizeof(int), file);
			AFileWrite(&primitive.indexType  , sizeof(int), file);
			AFileWrite(&primitive.numIndices , sizeof(int), file);
			AFileWrite(&primitive.numVertices, sizeof(int), file);
			AFileWrite(&primitive.indexOffset, sizeof(int), file);
            
			int material = primitive.material;
			AFileWrite(&material, sizeof(int), file);
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
		AFileWrite(&gltf->samplers[i], sizeof(ASampler ), file);
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

	texture.texCoord = data & 0xFFFF; data >>= sizeof(short) * 8;
	texture.index    = data & 0xFFFF; data >>= sizeof(short) * 8;
	texture.strength = data & 0xFFFF; data >>= sizeof(short) * 8;
	texture.scale    = data & 0xFFFF;
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

int LoadGLTFBinary(const char* path, ParsedGLTF* gltf)
{
	AFile file = AFileOpen(path, AOpenFlag_Read);
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
	AFileRead(&gltf->numMaterials, sizeof(short), file);
	AFileRead(&gltf->numTextures, sizeof(short), file);
	AFileRead(&gltf->numImages, sizeof(short), file);
	AFileRead(&gltf->numSamplers, sizeof(short), file);
	AFileRead(&gltf->numCameras, sizeof(short), file);
	AFileRead(&gltf->numScenes, sizeof(short), file);
	AFileRead(&gltf->defaultSceneIndex, sizeof(short), file);
	
	AFileRead(&gltf->totalIndices, sizeof(int), file);
	AFileRead(&gltf->totalVertices, sizeof(int), file);

	{
		uint64_t allVertexSize = gltf->totalVertices * sizeof(AVertex);
		uint64_t allIndexSize  = gltf->totalIndices * sizeof(uint32_t);

		gltf->allVertices = new AVertex[gltf->totalVertices]; // divide / 4 to get number of floats
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

			const int TypeToSize[8] = { 1, 1, 2, 2, 4, 4, 4 };
			uint64_t indexSize = uint64_t(TypeToSize[primitive.indexType]) * primitive.numIndices;
			primitive.indices = (void*)currIndices;
			currIndices += indexSize;
            
			uint64_t vertexSize = uint64_t(primitive.numVertices) * sizeof(AVertex);
			primitive.vertices = currVertices;
			currVertices += vertexSize;
			int material;
			AFileRead(&material, sizeof(int), file);
			primitive.material = material;
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

	AFileClose(file);

	gltf->stringAllocator = stringAllocator.TakeOwnership();
	gltf->intAllocator    = intAllocator.TakeOwnership();
	return 1;
}

