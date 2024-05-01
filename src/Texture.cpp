/********************************************************************************
*    Purpose:                                                                   *
*        Gets all of the textures in GLTF or FBX scene compresses them to       *
*        to make textures smaller on GPU and Disk I'm compressing them          *
*        using BCn texture compression on Windows                               *
*        and using ASTC texture compression for storing textures on android     *
*        also compressing further with zstd to reduce the size on disk.         *
*                                                                               *
*    Textures and Corresponding Formats:                                        *
*        R  = BC4                                                               *
*        RG = BC5                                                               *
*        RGB, RGBA = DXT5                                                       *
*    Android:                                                                   *
*        All Textures are using ASTC 4X4 format because:                        *
*        android doesn't have normal maps I haven't use other than ASTC4X4      *
*        but in feature I might use ETC2 format because it has                  *
*        faster compile times and faster compress speed. (etcpak)               *
*    Author:                                                                    *
*        Anilcan Gulkaya 2024 anilcangulkaya7@gmail.com github @benanil         *
********************************************************************************/


#include <thread>
#include <bitset>

#include "include/AssetManager.hpp"
#include "include/Scene.hpp"
#include "include/Renderer.hpp"

#include "../ASTL/String.hpp"
#include "../ASTL/Math/Math.hpp"
#include "../ASTL/Array.hpp"
#include "../ASTL/Additional/GLTFParser.hpp"
#include "../ASTL/IO.hpp"

#include "../External/stb_image.h"
#include "../External/zstd.h"

#if !AX_GAME_BUILD
#define STB_DXT_IMPLEMENTATION
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "../External/ProcessDxtc.hpp"
#include "../External/stb_dxt.h"
#include "../External/astc-encoder/astcenccli_internal.h"
#include "../External/stb_image_resize2.h"
#endif

/*//////////////////////////////////////////////////////////////////////////*/
/*                          Image Save Load                                 */
/*//////////////////////////////////////////////////////////////////////////*/

namespace {
	struct ImageInfo
	{
		int width, height;
		int numComp;
		int isNormal;
	};
}

const int g_AXTextureVersion = 12345;

// note: maybe we will need to check for data changed or not.
bool IsTextureLastVersion(const char* path)
{
	AFile file = AFileOpen(path, AOpenFlag_Read);
	if (!AFileExist(file) || AFileSize(file) < 32) 
		return false;
	int version = 0;
	AFileRead(&version, sizeof(int), file);
	AFileClose(file);
	return version == g_AXTextureVersion;
}

#if !AX_GAME_BUILD

static std::thread CompressASTCImagesThread;

extern uint64_t astcenc_main(const char* input_filename, unsigned char* currentCompression);

// converts to rg, removes blue
static void MakeNormalTextureFromRGB(unsigned char* texture, int numPixels)
{
	const unsigned char* rgb = texture;

	for (int i = 0; i < numPixels * 3; i += 3)
	{
		texture[0] = rgb[0];
		texture[1] = rgb[1];

		texture += 2;
		rgb += 3;
	}
}

// converts to rg, removes blue and alpha
static void MakeNormalTextureFromRGBA(unsigned char* texture, int numPixels)
{
	const unsigned char* rgba = texture;

	for (int i = 0; i < numPixels * 4; i += 4)
	{
		texture[0] = rgba[0];
		texture[1] = rgba[1];

		texture += 2;
		rgba += 4;
	}	
}

template<int channelsBefore>
static void MakeRGBA(const unsigned char* RESTRICT from, unsigned char* rgba, int numPixels)
{
	for (int i = 0; i < numPixels; i++)
	{
		MemsetZero(rgba, 4 * sizeof(char));
		for (int p = 0; p < channelsBefore; p++)
		{
			rgba[p] = from[p];
		}
		from += channelsBefore;
		rgba += 4;
	}
}

static void CompressBC4(const unsigned char* RESTRICT src, unsigned char* bc4, int width, int height)
{
	unsigned char r[4 * 4];

	for (int i = 0; i < height; i += 4)
	{
		for (int j = 0; j < width; j += 4)
		{
			SmallMemCpy(r +  0, src + ((i + 0) * width) + j, 4);
			SmallMemCpy(r +  4, src + ((i + 1) * width) + j, 4);
			SmallMemCpy(r +  8, src + ((i + 2) * width) + j, 4);
			SmallMemCpy(r + 12, src + ((i + 3) * width) + j, 4);
			stb_compress_bc4_block(bc4, r);
			bc4 += 8; // 8 byte per block
		}
	}
}

static void CompressBC5(const unsigned char* RESTRICT src, unsigned char* bc5, int width, int height)
{
	unsigned char rg[4 * 4 * 2];
	int width2 = width * sizeof(short);

	for (int i = 0; i < height; i += 4)
	{
		for (int j = 0; j < width; j += 4)
		{
			int j2 = (j * 2);

			SmallMemCpy(rg +  0, src + ((i + 0) * width2) + j2, 4 * 2);
			SmallMemCpy(rg +  8, src + ((i + 1) * width2) + j2, 4 * 2);
			SmallMemCpy(rg + 16, src + ((i + 2) * width2) + j2, 4 * 2);
			SmallMemCpy(rg + 24, src + ((i + 3) * width2) + j2, 4 * 2);
			stb_compress_bc5_block(bc5, rg);
			bc5 += 16; // 16 byte per block
		}
	}
}

uint64_t ASTCCompress(unsigned char* buffer, unsigned char* image, int dim_x, int dim_y)
{
	astcenc_profile profile = ASTCENC_PRF_LDR;

	// This has to come first, as the block size is in the file header
	astc_compressed_image image_comp{};
	astcenc_config config{};

	float        quality = ASTCENC_PRE_MEDIUM;
	unsigned int flags   = 0;
	unsigned int block_x = 4, block_y = 4, block_z = 1;
	astcenc_error error = astcenc_config_init(profile, block_x, block_y, block_z,
                                               quality, flags, &config);
	if (error) {
		return 1;
	}

	/** The  pre-encode swizzle. */
	astcenc_swizzle swz_encode = { ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A };

	astcenc_error    codec_status;
	astcenc_context* codec_context;

	int threadCount = 1;
	codec_status = astcenc_context_alloc(&config, threadCount, &codec_context);
	if (codec_status != ASTCENC_SUCCESS)
	{
		print_error("ERROR: Codec context alloc failed: %s\n", astcenc_get_error_string(codec_status));
		ASSERT(0);
		return 1;
	}

	int dim_z = 1;
	astcenc_image *uncomp = new astcenc_image;
	{
		uncomp->dim_x = dim_x;
		uncomp->dim_y = dim_y;
		uncomp->dim_z = dim_z;

		void** data = new void*[dim_z];
		uncomp->data = data;
		uncomp->data_type = ASTCENC_TYPE_U8;
		data[0] = image; // new uint8_t[dim_x * dim_y * 4];
	}

	// Compress an image
	// if (operation & ASTCENC_STAGE_COMPRESS)
	unsigned int blocks_x = (uncomp->dim_x + config.block_x - 1) / config.block_x;
	unsigned int blocks_y = (uncomp->dim_y + config.block_y - 1) / config.block_y;
	unsigned int blocks_z = (uncomp->dim_z + config.block_z - 1) / config.block_z;
	size_t buffer_size = blocks_x * blocks_y * blocks_z * 16;
	int numMips = MAX((int)Log2(uncomp->dim_x) >> 1, 1) - 1;
	
	unsigned char* compressBuffer = new unsigned char[(uncomp->dim_x * uncomp->dim_y * 4) >> 1];
	uint64_t compressedSize = 0;

	do {
		error = astcenc_compress_image(codec_context, uncomp, &swz_encode, buffer, buffer_size, 0);
		
		astcenc_compress_reset(codec_context);

		if (error != ASTCENC_SUCCESS)
		{
			print_error("ERROR: Codec compress failed: %s\n", astcenc_get_error_string(error));
			ASSERT(0);
			return 1;
		}
		
		int dim_x = uncomp->dim_x;
		int dim_y = uncomp->dim_y;

		compressedSize += buffer_size;
		buffer += buffer_size;

		if (numMips-- <= 0)
			break;

		stbir_resize(*uncomp->data, dim_x, dim_y, dim_x * 4, 
			         compressBuffer, dim_x >> 1, dim_y >> 1, (dim_x >> 1) * 4, 
			         STBIR_RGBA, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL);
		
		unsigned char* temp = (unsigned char*)*uncomp->data;
		*uncomp->data = compressBuffer;
		compressBuffer = temp;

		uncomp->dim_x >>= 1;
		uncomp->dim_y >>= 1;
		buffer_size = uncomp->dim_x * uncomp->dim_y;
	} while (true);


	delete[] uncomp->data; 
	astcenc_context_free(codec_context);

	delete[] compressBuffer;
	return compressedSize;
}

#endif // __ANDROID__

static void SaveSceneImagesGeneric(Prefab* scene, char* path, const bool isMobile)
{
#if !AX_GAME_BUILD
	if (IsTextureLastVersion(path)) {
		return;
	}
	AImage* images = scene->images;
	int numImages = scene->numImages;
	int currentInfo = 0;
    
	if (numImages == 0) {
		return;
	}
	
	ASSERT(numImages < 512);
	std::bitset<512> isNormalMap{};
	AMaterial* materials = scene->materials;
	int numMaterials = scene->numMaterials;
	// this makes always positive
	const short shortWithoutSign = (short)0x7FFF;
 
	// mark normal maps
	for (int i = 0; i < numMaterials; i++)
	{
		isNormalMap.set(materials[i].GetNormalTexture().index & 511); 
	}

	// if an normal map used as base color, unmark it. (causing problems on sponza)
	for (int i = 0; i < numMaterials; i++)
	{
		isNormalMap.reset(materials[i].baseColorTexture.index & 511);
	}

	ScopedPtr<ImageInfo> imageInfos = new ImageInfo[numImages];
	ScopedPtr<uint64_t>  currentCompressions = new uint64_t[numImages];
	uint64_t beforeCompressedSize = 0;

	for (int i = 0; i < numImages; i++)
	{
		ImageInfo info;
		info.width    = 0, info.height = 0;
		info.numComp  = 4;
		info.isNormal = isNormalMap[i];
		
		bool imageInvalid = images[i].path == nullptr || !FileExist(images[i].path);

		// mobile can't have normal maps
		if ((info.isNormal && isMobile) || imageInvalid)
		{
			imageInfos[currentInfo] = info;
			currentCompressions[currentInfo++] = beforeCompressedSize;
			continue;
		}

		const char* imageFileName = images[i].path;
		int res = stbi_info(imageFileName, &info.width, &info.height, &info.numComp);
		ASSERT(res);
		currentCompressions[currentInfo] = beforeCompressedSize;
		imageInfos[currentInfo++] = info;

		int isBC1 = (isMobile == false) && (info.numComp == 1);
		int imageSize = (info.width * info.height) >> isBC1;
		
		// we have got to include mipmap sizes if mobile
		if (isMobile)
		{
			// 512->3, 1024->4, 2049->5
			int numMips = MAX(Log2((unsigned int)info.width) >> 1u, 1u) - 1;
			while (numMips--)
			{
				info.width  >>= 1;
				info.height >>= 1;
				imageSize += info.width * info.height;
			}
		}

		beforeCompressedSize += imageSize;
	}

	if (beforeCompressedSize == 0) {
		scene->numImages   = 0;
		scene->numTextures = 0;
		return;
	}
		
	ScopedPtr<unsigned char> toCompressionBuffer = new unsigned char[beforeCompressedSize];

	auto execFn = [&](int numTextures, uint64_t compressionStart, int i) -> void
	{
		Array<unsigned char> textureLoadBuffer(!isMobile * 1024 * 1024);
		unsigned char* currentCompression = toCompressionBuffer + compressionStart;

		for (int end = i + numTextures; i < end; i++)
		{
			ImageInfo info = imageInfos[i];
			const char* imagePath = images[i].path;

			if (info.width == 0)
				continue;

			// note: that stb image using new and delete instead of malloc free (I overloaded)
			ScopedPtr<unsigned char> stbImage = stbi_load(imagePath, &info.width, &info.height, &info.numComp, 0);

			if (stbImage.ptr == nullptr) {
				stbImage.ptr = new unsigned char[info.width * info.height * info.numComp];
			}

			int imageSize = info.width * info.height;
			textureLoadBuffer.Reserve(imageSize * 4);

			if (isMobile)
			{
				if (info.numComp == 3) MakeRGBA<3>(stbImage.ptr, textureLoadBuffer.Data(), imageSize);
				if (info.numComp == 2) MakeRGBA<2>(stbImage.ptr, textureLoadBuffer.Data(), imageSize);
				if (info.numComp == 1) MakeRGBA<1>(stbImage.ptr, textureLoadBuffer.Data(), imageSize);

				if (info.numComp != 4)
				{
					stbi_image_free(stbImage.ptr);
					stbImage.ptr = textureLoadBuffer.TakeOwnership();
					textureLoadBuffer.Resize(imageSize * 4);// reallocates the empty buffer
				}

				uint64_t numBytes = ASTCCompress(currentCompression, stbImage.ptr, info.width, info.height);
				ASSERT(numBytes != 1);
				currentCompression += numBytes;
				continue;
			}

			if (isNormalMap[i])
			{
				if (info.numComp == 3) MakeNormalTextureFromRGB(stbImage, imageSize);
				if (info.numComp == 4) MakeNormalTextureFromRGBA(stbImage, imageSize);
				imageInfos.ptr[i].numComp = 2;

				CompressBC5(stbImage.ptr, textureLoadBuffer.Data(), info.width, info.height);
				SmallMemCpy(currentCompression, textureLoadBuffer.Data(), imageSize);
				currentCompression += imageSize; 
				continue;
			}

			uint32_t numBlocks = (info.width >> 2) * (info.height >> 2); // 4x4 blocks, divide by 4 each axis

			if (info.numComp == 1)
			{
				CompressBC4(stbImage.ptr, textureLoadBuffer.Data(), info.width, info.height);
				imageSize >>= 1; // 0.5 byte per pixel
			}
			else if (info.numComp == 2)
			{
				CompressBC5(stbImage.ptr, textureLoadBuffer.Data(), info.width, info.height);
			}
			else if (info.numComp == 3)
			{
				MakeRGBA<3>(stbImage.ptr, textureLoadBuffer.Data(), imageSize);
				stbi_image_free(stbImage.ptr);
				stbImage.ptr = textureLoadBuffer.TakeOwnership();
				textureLoadBuffer.Resize(imageSize * 4);// reallocates the empty buffer
				// this is an rgba format, but use it for rgb textures as well, because there are not any better format for this I guess(quality, and compression vise)
				CompressDxt5((const uint32_t*)stbImage.ptr, (uint64_t*)textureLoadBuffer.Data(), numBlocks, info.width);
			}
			else if (info.numComp == 4)
			{
				CompressDxt5((const uint32_t*)stbImage.ptr, (uint64_t*)textureLoadBuffer.Data(), numBlocks, info.width);
			}
			
			SmallMemCpy(currentCompression, textureLoadBuffer.Data(), imageSize);
			currentCompression += imageSize;
		}
	};
	

	int numTask = numImages;
	int taskPerThread = MAX(numImages / 8, 1);
	std::thread threads[9];

	int numThreads = 0;
	for (int i = 0; i <= numTask - taskPerThread; i += taskPerThread)
	{
		int start = i;
		new (threads + numThreads)std::thread(execFn, taskPerThread, currentCompressions[start], start);
		numThreads++;
	}

	int remainingTasks = numTask % taskPerThread;
	if (remainingTasks > 0)
	{
		int start = numImages - remainingTasks;
		new (threads + numThreads)std::thread(execFn, remainingTasks, currentCompressions[start], start);
		numThreads++;
	}

	for (int i = 0; i < numThreads; i++)
	{
		threads[i].join();
	}
	
	AFile file = AFileOpen(path, AOpenFlag_Write);
	AFileWrite(&g_AXTextureVersion, sizeof(int), file);
	AFileWrite(imageInfos.ptr, numImages * sizeof(ImageInfo), file);

	uint64_t compressedSize = uint64_t(beforeCompressedSize * 0.90);
	ScopedPtr<char> compressedBuffer = new char[compressedSize];

	compressedSize = ZSTD_compress(compressedBuffer, compressedSize, toCompressionBuffer, beforeCompressedSize, 9);
	ASSERT(!ZSTD_isError(compressedSize));

	uint64_t decompressedSize = beforeCompressedSize;
	AFileWrite(&decompressedSize, sizeof(uint64_t), file);
	AFileWrite(&compressedSize, sizeof(uint64_t), file);
	AFileWrite(compressedBuffer, compressedSize, file);

	AFileClose(file);
#endif
}

static void LoadSceneImagesGeneric(const char* texturePath, Texture* textures, int numImages)
{
	if (numImages == 0) {
		return;
	}
	AFile file = AFileOpen(texturePath, AOpenFlag_Read);
	int version = 0;
	AFileRead(&version, sizeof(int), file);
	ASSERT(version == g_AXTextureVersion); // probably using old version, find newer version of texture or reload the gltf or fbx scene

	ScopedPtr<ImageInfo> imageInfos = new ImageInfo[numImages];

	for (int i = 0; i < numImages; i++)
	{
		AFileRead(&imageInfos.ptr[i], sizeof(ImageInfo), file);
	}

	uint64_t decompressedSize, compressedSize;
	AFileRead(&decompressedSize, sizeof(uint64_t), file);
	AFileRead(&compressedSize, sizeof(uint64_t), file);

	ScopedPtr<unsigned char> compressedBuffer = new unsigned char[compressedSize];
	AFileRead(compressedBuffer.ptr, compressedSize, file);

	ScopedPtr<unsigned char> decompressedBuffer = new unsigned char[decompressedSize];
	decompressedSize = ZSTD_decompress(decompressedBuffer.ptr, decompressedSize, compressedBuffer, compressedSize);
	ASSERT(!ZSTD_isError(decompressedSize));

	unsigned char* currentImage = decompressedBuffer.ptr;

	for (int i = 0; i < numImages; i++)
	{
		ImageInfo info = imageInfos.ptr[i];
		if (info.width == 0)
			continue;

		int imageSize = info.width * info.height;
		bool isBC4 = info.numComp == 1 && (IsAndroid() == false);
		
		TextureType textureType = TextureType_CompressedR + info.numComp-1;
		imageSize >>= (int)isBC4; // BC4 is 0.5 byte per pixel

		const TexFlags flags = TexFlags_Compressed | TexFlags_MipMap;
		textures[i] = rCreateTexture(info.width, info.height, currentImage, textureType, flags);
		currentImage += imageSize;

		if_constexpr (IsAndroid())
		{
			int mip = MAX((int)Log2((unsigned int)info.width) >> 1, 1) - 1;
			while (mip-- > 0)
			{
				info.width >>= 1;
				info.height >>= 1;
				currentImage += info.width * info.height;
			}
		}
	}

	AFileClose(file);
}

static void SaveAndroidCompressedImagesFn(Prefab* scene, char* astcPath)
{
	SaveSceneImagesGeneric(scene, astcPath, true); // is mobile true
	delete[] astcPath;
}

void SaveSceneImages(Prefab* scene, char* path)
{
#if !AX_GAME_BUILD
	// // save dxt textures for desktop
	ChangeExtension(path, StringLength(path), "dxt");
    SaveSceneImagesGeneric(scene, path, false); // is mobile false
	
	// save astc textures for android
	int len = StringLength(path);
	ChangeExtension(path, len, "astc");
	char* astcPath = new char[len + 2] {};
	SmallMemCpy(astcPath, path, len + 1);

	// save textures in other thread because we don't want to wait android textures while on windows platform
	new(&CompressASTCImagesThread)std::thread(SaveAndroidCompressedImagesFn, scene, astcPath);
#endif
}

void LoadSceneImages(char* path, Texture*& textures, int numImages)
{
	textures = new Texture[numImages]();
#ifdef __ANDROID__
	ChangeExtension(path, StringLength(path), "astc");
#else
	ChangeExtension(path, StringLength(path), "dxt");
#endif
	LoadSceneImagesGeneric(path, textures, numImages);
}