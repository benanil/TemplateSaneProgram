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

#include "AssetManager.hpp"
#include "../ASTL/String.hpp"
#include "../ASTL/Math/Math.hpp"
#include "../ASTL/Array.hpp"
#include "../ASTL/Additional/GLTFParser.hpp"
#include "../ASTL/IO.hpp"

#include "../External/stb_image.h"
#include "../External/zstd.h"

#if !AX_GAME_BUILD
#define STB_DXT_IMPLEMENTATION
#include "../External/ProcessDxtc.hpp"
#include "../External/stb_dxt.h"
#include "../External/astc-encoder/astcenccli_internal.h"
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

const int g_AXTextureVersion = 1234;

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

static void ConvertRGBToRGBA(const unsigned char* RESTRICT rgb, unsigned char* rgba, int srcNumComp, int numPixels)
{
	for (int i = 0; i < numPixels; i++)
	{
		rgba[0] = rgb[0];
		rgba[1] = rgb[1];
		rgba[2] = rgb[2];
		rgba[3] = 255;

		rgb  += 3;
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
#endif // __ANDROID__

static void SaveSceneImagesGeneric(SubScene* scene, char* path, const bool isMobile)
{
#if !AX_GAME_BUILD
	if (IsTextureLastVersion(path)) {
		return;
	}
	AImage* images = scene->data.images;
	int numImages = scene->data.numImages;
	int currentInfo = 0;
    
	if (numImages == 0) {
		return;
	}
	
	ASSERT(numImages < 512);
	std::bitset<512> isNormalMap{};
	AMaterial* materials = scene->data.materials;
	int numMaterials = scene->data.numMaterials;
	// this makes always positive
	const short shortWithoutSign = (short)0x7FFF;
 
	// mark normal maps
	for (int i = 0; i < numMaterials; i++)
	{
		isNormalMap.set(materials[i].GetNormalTexture().index & shortWithoutSign); 
	}

	// if an normal map used as base color, unmark it. (causing problems on sponza)
	for (int i = 0; i < numMaterials; i++)
	{
		isNormalMap.reset(materials[i].baseColorTexture.index & shortWithoutSign);
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

			if (isMobile)
			{
				uint64_t numBytes = astcenc_main(imagePath, currentCompression);
				ASSERT(numBytes != 1);
				currentCompression += numBytes;
				continue;
			}
			
			// note: that stb image using new and delete instead of malloc free (I overloaded)
			ScopedPtr<unsigned char> stbImage = stbi_load(imagePath, &info.width, &info.height, &info.numComp, 0);
			ASSERT(stbImage.ptr != nullptr);

			int imageSize = info.width * info.height;
			textureLoadBuffer.Reserve(imageSize * 4);

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
				ConvertRGBToRGBA(stbImage.ptr, textureLoadBuffer.Data(), info.numComp, imageSize);
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
	ASSERT(version == g_AXTextureVersion);

	Array<ImageInfo> imageInfos(numImages);

	for (int i = 0; i < numImages; i++)
	{
		ImageInfo info;
		AFileRead(&info, sizeof(ImageInfo), file);
		imageInfos.PushBack(info);
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
		ImageInfo info = imageInfos[i];
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

static void SaveAndroidCompressedImagesFn(SubScene* scene, char* astcPath)
{
	SaveSceneImagesGeneric(scene, astcPath, true); // is mobile true
	delete[] astcPath;
}

void SaveSceneImages(SubScene* scene, char* path)
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