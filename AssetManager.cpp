
#include "Platform.hpp"
#include "AssetManager.hpp"
#include "External/ufbx.h"
#include "ASTL/Array.hpp"
#include "ASTL/String.hpp"
#include "ASTL/Math/Matrix.hpp"
#include "ASTL/Additional/GLTFParser.hpp"
// #include "External/stb_image.h"
#include "ASTL/IO.hpp"

/*//////////////////////////////////////////////////////////////////////////*/
/*                              FBX LOAD                                    */
/*//////////////////////////////////////////////////////////////////////////*/

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
			short textureIndex = IndexOf(uscene->textures.data, texture, uscene->textures.count);
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

int LoadFBX(const char* path, ParsedGLTF* fbxScene, float scale)
{
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

	fbxScene->numMeshes    = uscene->meshes.count;
	fbxScene->numNodes     = uscene->nodes.count;
	fbxScene->numMaterials = uscene->materials.count;
	fbxScene->numImages    = uscene->texture_files.count; 
	fbxScene->numTextures  = uscene->textures.count;
	fbxScene->numCameras   = uscene->cameras.count;
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
		primitive.numIndices  = umesh->num_triangles * 3;
		primitive.numVertices = umesh->num_vertices;
		primitive.indexType   = 5; //GraphicType_UnsignedInt;
		primitive.material    = 0; // todo
		primitive.indices     = currentIndex; 
		primitive.vertices    = currentVertex;
		
		currentIndex  += primitive.numIndices;
		currentVertex += primitive.numVertices;

		primitive.attributes |= AAttribType_POSITION;
		primitive.attributes |= umesh->vertex_uv.exists * AAttribType_TEXCOORD_0;
		primitive.attributes |= umesh->vertex_normal.exists * AAttribType_NORMAL;

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

	short numImages = uscene->texture_files.count;
	Array<AImage> images(uscene->texture_files.count, uscene->texture_files.count);
	
	for (int i = 0; i < numImages; i++)
	{
		images[i].path = GetNameFromFBX(uscene->texture_files[i].filename, stringAllocator);
	}
	
	// todo: textures
	short numTextures = uscene->textures.count;
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
			short normalIndex = IndexOf(uscene->textures.data, normalTexture, uscene->textures.count);
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
	
		amaterial.metallicFactor   = umaterial->pbr.metalness.value_real * 400;
		amaterial.roughnessFactor  = umaterial->pbr.roughness.value_real * 400;
		amaterial.baseColorFactor  = umaterial->pbr.base_factor.value_real * 400;
	
		amaterial.specularFactor   = umaterial->features.pbr.enabled ? umaterial->pbr.specular_factor.value_real : umaterial->fbx.specular_factor.value_real;
		amaterial.diffuseColor     = PackColorRGBU32(&umaterial->fbx.diffuse_color.value_real);
		amaterial.specularColor    = PackColorRGBU32(&umaterial->fbx.specular_color.value_real);
        
		amaterial.doubleSided = umaterial->features.double_sided.enabled;
	
		if (umaterial->pbr.emission_factor.value_components == 1)
		{
			amaterial.emissiveFactor[0] = amaterial.emissiveFactor[1] = amaterial.emissiveFactor[2] = umaterial->pbr.emission_factor.value_real;
		}
		else if (umaterial->pbr.emission_factor.value_components > 2)
		{
			amaterial.emissiveFactor[0] = umaterial->pbr.emission_factor.value_vec3.x * 400;
			amaterial.emissiveFactor[1] = umaterial->pbr.emission_factor.value_vec3.y * 400;
			amaterial.emissiveFactor[2] = umaterial->pbr.emission_factor.value_vec3.z * 400;
		}
	}

	// copy nodes
	short numNodes = uscene->nodes.count;
	fbxScene->numNodes = numNodes;
	
	if (numNodes) fbxScene->nodes = new ANode[numNodes]{};
	for (int i = 0; i < numNodes; i++)
	{
		ANode& anode = fbxScene->nodes[i];
		ufbx_node* unode = uscene->nodes[i];
		anode.type = unode->camera != nullptr;
		anode.name = GetNameFromFBX(unode->name, stringAllocator);
		anode.numChildren = unode->children.count;
		anode.children = intAllocator.AllocateUninitialized(anode.numChildren + 1);
		
		for (int j = 0; j < anode.numChildren; j++)
		{
			anode.children[j] = IndexOf(uscene->nodes.data, unode->children[j], uscene->nodes.count);
			ASSERT(anode.children[j] != -1);
		}

		SmallMemCpy(anode.translation, &unode->world_transform, sizeof(ufbx_transform));

		if (anode.type == 0)
		{
			anode.index = IndexOf(uscene->meshes.data, unode->mesh, uscene->meshes.count);
			if (unode->materials.count > 0)
				fbxScene->meshes[anode.index].primitives[0].material = IndexOf(uscene->materials.data, unode->materials[0], uscene->materials.count);
		}
		else
			anode.index = IndexOf(uscene->cameras.data, unode->camera, uscene->cameras.count);
	}

	fbxScene->numImages       = images.Size();
	fbxScene->images          = images.TakeOwnership();
	fbxScene->stringAllocator = stringAllocator.TakeOwnership();
	fbxScene->intAllocator    = intAllocator.TakeOwnership();
	ufbx_free_scene(uscene);
	return 1;
}

