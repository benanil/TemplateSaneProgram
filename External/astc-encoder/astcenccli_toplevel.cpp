// SPDX-License-Identifier: Apache-2.0
// ----------------------------------------------------------------------------
// Copyright 2011-2023 Arm Limited
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at:
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.
// ----------------------------------------------------------------------------

/**
 * @brief Functions for codec library front-end.
 */

#include "astcenc.h"
#include "astcenccli_internal.h"

#include <assert.h>
#include <string.h>
#include <functional>
#include <string>
#include <sstream>
#include <memory.h>

#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "../stb_image_resize2.h"

 /* ============================================================================
	 Data structure definitions
 ============================================================================ */

typedef unsigned int astcenc_operation;

struct mode_entry
{
	const char* opt;
	astcenc_operation operation;
	astcenc_profile decode_mode;
};

/* ============================================================================
	Constants and literals
============================================================================ */

/** @brief Stage bit indicating we need to load a compressed image. */
static const unsigned int ASTCENC_STAGE_LD_COMP = 1 << 0;

/** @brief Stage bit indicating we need to store a compressed image. */
static const unsigned int ASTCENC_STAGE_ST_COMP = 1 << 1;

/** @brief Stage bit indicating we need to load an uncompressed image. */
static const unsigned int ASTCENC_STAGE_LD_NCOMP = 1 << 2;

/** @brief Stage bit indicating we need to store an uncompressed image. */
static const unsigned int ASTCENC_STAGE_ST_NCOMP = 1 << 3;

/** @brief Stage bit indicating we need compress an image. */
static const unsigned int ASTCENC_STAGE_COMPRESS = 1 << 4;

/** @brief Stage bit indicating we need to decompress an image. */
static const unsigned int ASTCENC_STAGE_DECOMPRESS = 1 << 5;

/** @brief Stage bit indicating we need to compare an image with the original input. */
static const unsigned int ASTCENC_STAGE_COMPARE = 1 << 6;

/** @brief Operation indicating an unknown request (should never happen). */
static const astcenc_operation ASTCENC_OP_UNKNOWN = 0;

/** @brief Operation indicating the user wants to print long-form help text and version info. */
static const astcenc_operation ASTCENC_OP_HELP = 1 << 7;

/** @brief Operation indicating the user wants to print short-form help text and version info. */
static const astcenc_operation ASTCENC_OP_VERSION = 1 << 8;

/** @brief Operation indicating the user wants to compress and store an image. */
static const astcenc_operation ASTCENC_OP_COMPRESS =
ASTCENC_STAGE_LD_NCOMP |
ASTCENC_STAGE_COMPRESS |
ASTCENC_STAGE_ST_COMP;

/** @brief Operation indicating the user wants to decompress and store an image. */
static const astcenc_operation ASTCENC_OP_DECOMPRESS =
ASTCENC_STAGE_LD_COMP |
ASTCENC_STAGE_DECOMPRESS |
ASTCENC_STAGE_ST_NCOMP;

/** @brief Operation indicating the user wants to test a compression setting on an image. */
static const astcenc_operation ASTCENC_OP_TEST =
ASTCENC_STAGE_LD_NCOMP |
ASTCENC_STAGE_COMPRESS |
ASTCENC_STAGE_DECOMPRESS |
ASTCENC_STAGE_COMPARE |
ASTCENC_STAGE_ST_NCOMP;

/**
 * @brief Image preprocesing tasks prior to encoding.
 */
enum astcenc_preprocess
{
	/** @brief No image preprocessing. */
	ASTCENC_PP_NONE = 0,
	/** @brief Normal vector unit-length normalization. */
	ASTCENC_PP_NORMALIZE,
	/** @brief Color data alpha premultiplication. */
	ASTCENC_PP_PREMULTIPLY
};

/** @brief Decode table for command line operation modes. */
static const mode_entry modes[]{
	{"-cl",      ASTCENC_OP_COMPRESS,   ASTCENC_PRF_LDR},
	{"-dl",      ASTCENC_OP_DECOMPRESS, ASTCENC_PRF_LDR},
	{"-tl",      ASTCENC_OP_TEST,       ASTCENC_PRF_LDR},
	{"-cs",      ASTCENC_OP_COMPRESS,   ASTCENC_PRF_LDR_SRGB},
	{"-ds",      ASTCENC_OP_DECOMPRESS, ASTCENC_PRF_LDR_SRGB},
	{"-ts",      ASTCENC_OP_TEST,       ASTCENC_PRF_LDR_SRGB},
	{"-ch",      ASTCENC_OP_COMPRESS,   ASTCENC_PRF_HDR_RGB_LDR_A},
	{"-dh",      ASTCENC_OP_DECOMPRESS, ASTCENC_PRF_HDR_RGB_LDR_A},
	{"-th",      ASTCENC_OP_TEST,       ASTCENC_PRF_HDR_RGB_LDR_A},
	{"-cH",      ASTCENC_OP_COMPRESS,   ASTCENC_PRF_HDR},
	{"-dH",      ASTCENC_OP_DECOMPRESS, ASTCENC_PRF_HDR},
	{"-tH",      ASTCENC_OP_TEST,       ASTCENC_PRF_HDR},
	{"-h",       ASTCENC_OP_HELP,       ASTCENC_PRF_HDR},
	{"-help",    ASTCENC_OP_HELP,       ASTCENC_PRF_HDR},
	{"-v",       ASTCENC_OP_VERSION,    ASTCENC_PRF_HDR},
	{"-version", ASTCENC_OP_VERSION,    ASTCENC_PRF_HDR}
};

/**
 * @brief Compression workload definition for worker threads.
 */
struct compression_workload
{
	astcenc_context* context;
	astcenc_image* image;
	astcenc_swizzle swizzle;
	uint8_t* data_out;
	size_t data_len;
	astcenc_error error;
};

/**
 * @brief Test if a string argument is a well formed float.
 */
static bool is_float(
	std::string target
) {
	float test;
	std::istringstream stream(target);

	// Leading whitespace is an error
	stream >> std::noskipws >> test;

	// Ensure entire no remaining string in addition to parse failure
	return stream.eof() && !stream.fail();
}

/**
 * @brief Test if a string ends with a given suffix.
 */
static bool ends_with(
	const std::string& str,
	const std::string& suffix
) {
	return (str.size() >= suffix.size()) &&
		(0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix));
}

/**
 * @brief Runner callback function for a compression worker thread.
 *
 * @param thread_count   The number of threads in the worker pool.
 * @param thread_id      The index of this thread in the worker pool.
 * @param payload        The parameters for this thread.
 */
static void compression_workload_runner(
	int thread_count,
	int thread_id,
	void* payload
) {
	(void)thread_count;

	compression_workload* work = static_cast<compression_workload*>(payload);
	astcenc_error error = astcenc_compress_image(
		work->context, work->image, &work->swizzle,
		work->data_out, work->data_len, thread_id);

	// This is a racy update, so which error gets returned is a random, but it
	// will reliably report an error if an error occurs
	if (error != ASTCENC_SUCCESS)
	{
		work->error = error;
	}
}

/**
 * @brief Utility to generate a slice file name from a pattern.
 *
 * Convert "foo/bar.png" in to "foo/bar_<slice>.png"
 *
 * @param basename The base pattern; must contain a file extension.
 * @param index    The slice index.
 * @param error    Set to true on success, false on error (no extension found).
 *
 * @return The slice file name.
 */
static std::string get_slice_filename(
	const std::string& basename,
	unsigned int index,
	bool& error
) {
	size_t sep = basename.find_last_of('.');
	if (sep == std::string::npos)
	{
		error = true;
		return "";
	}

	std::string base = basename.substr(0, sep);
	std::string ext = basename.substr(sep);
	std::string name = base + "_" + std::to_string(index) + ext;
	error = false;
	return name;
}

/**
 * @brief Load a non-astc image file from memory.
 *
 * @param filename            The file to load, or a pattern for array loads.
 * @param dim_z               The number of slices to load.
 * @param y_flip              Should this image be Y flipped?
 * @param[out] is_hdr         Is the loaded image HDR?
 * @param[out] component_count The number of components in the loaded image.
 *
 * @return The astc image file, or nullptr on error.
 */
static astcenc_image* load_uncomp_file(
	const char* filename,
	unsigned int dim_z,
	bool y_flip,
	bool& is_hdr,
	unsigned int& component_count
) {
	astcenc_image* image = nullptr;

	// For a 2D image just load the image directly
	image = load_ncimage(filename, y_flip, is_hdr, component_count);
	return image;
}

/**
 * @brief Parse the command line.
 *
 * @param      argc        Command line argument count.
 * @param[in]  argv        Command line argument vector.
 * @param[out] operation   Codec operation mode.
 * @param[out] profile     Codec color profile.
 *
 * @return 0 if everything is okay, 1 if there is some error
 */
static int parse_commandline_options(
	int argc,
	const char** argv,
	astcenc_operation& operation,
	astcenc_profile& profile
) {
	assert(argc >= 2); (void)argc;

	profile = ASTCENC_PRF_LDR;
	operation = ASTCENC_OP_COMPRESS;
	return 0;
}

/**
 * @brief Initialize the astcenc_config
 *
 * @param      argc         Command line argument count.
 * @param[in]  argv         Command line argument vector.
 * @param      operation    Codec operation mode.
 * @param[out] profile      Codec color profile.
 * @param      comp_image   Compressed image if a decompress operation.
 * @param[out] preprocess   Image preprocess operation.
 * @param[out] config       Codec configuration.
 *
 * @return 0 if everything is okay, 1 if there is some error
 */
static int init_astcenc_config(
	int argc,
	const char** argv,
	astcenc_profile profile,
	astcenc_operation operation,
	astc_compressed_image& comp_image,
	astcenc_preprocess& preprocess,
	astcenc_config& config
) {
	unsigned int block_x = 0;
	unsigned int block_y = 0;
	unsigned int block_z = 1;

	// For decode the block size is set by the incoming image.
	if (operation == ASTCENC_OP_DECOMPRESS)
	{
		block_x = comp_image.block_x;
		block_y = comp_image.block_y;
		block_z = comp_image.block_z;
	}

	float quality = 0.0f;
	preprocess = ASTCENC_PP_NONE;

	// parse the command line's encoding options.
	int argidx = 4;
	if (operation & ASTCENC_STAGE_COMPRESS)
	{
		// Read and decode block size
		if (argc < 5)
		{
			print_error("ERROR: Block size must be specified\n");
			return 1;
		}

		int cnt2D, cnt3D;
		int dimensions = sscanf(argv[4], "%ux%u%nx%u%n",
			&block_x, &block_y, &cnt2D, &block_z, &cnt3D);
		// Character after the last match should be a NUL
		if (!(((dimensions == 2) && !argv[4][cnt2D]) || ((dimensions == 3) && !argv[4][cnt3D])))
		{
			print_error("ERROR: Block size '%s' is invalid\n", argv[4]);
			return 1;
		}

		// Read and decode search quality
		if (argc < 6)
		{
			print_error("ERROR: Search quality level must be specified\n");
			return 1;
		}

		if (!strcmp(argv[5], "-fastest"))
		{
			quality = ASTCENC_PRE_FASTEST;
		}
		else if (!strcmp(argv[5], "-fast"))
		{
			quality = ASTCENC_PRE_FAST;
		}
		else if (!strcmp(argv[5], "-medium"))
		{
			quality = ASTCENC_PRE_MEDIUM;
		}
		else if (!strcmp(argv[5], "-thorough"))
		{
			quality = ASTCENC_PRE_THOROUGH;
		}
		else if (!strcmp(argv[5], "-verythorough"))
		{
			quality = ASTCENC_PRE_VERYTHOROUGH;
		}
		else if (!strcmp(argv[5], "-exhaustive"))
		{
			quality = ASTCENC_PRE_EXHAUSTIVE;
		}
		else if (is_float(argv[5]))
		{
			quality = static_cast<float>(atof(argv[5]));
		}
		else
		{
			print_error("ERROR: Search quality/preset '%s' is invalid\n", argv[5]);
			return 1;
		}

		argidx = 6;
	}

	unsigned int flags = 0;

	// Gather the flags that we need
	while (argidx < argc)
	{
		if (!strcmp(argv[argidx], "-a"))
		{
			// Skip over the data value for now
			argidx++;
			flags |= ASTCENC_FLG_USE_ALPHA_WEIGHT;
		}
		else if (!strcmp(argv[argidx], "-normal"))
		{
			flags |= ASTCENC_FLG_MAP_NORMAL;
		}
		else if (!strcmp(argv[argidx], "-rgbm"))
		{
			// Skip over the data value for now
			argidx++;
			flags |= ASTCENC_FLG_MAP_RGBM;
		}
		else if (!strcmp(argv[argidx], "-perceptual"))
		{
			flags |= ASTCENC_FLG_USE_PERCEPTUAL;
		}
		else if (!strcmp(argv[argidx], "-pp-normalize"))
		{
			if (preprocess != ASTCENC_PP_NONE)
			{
				print_error("ERROR: Only a single image preprocess can be used\n");
				return 1;
			}
			preprocess = ASTCENC_PP_NORMALIZE;
		}
		else if (!strcmp(argv[argidx], "-pp-premultiply"))
		{
			if (preprocess != ASTCENC_PP_NONE)
			{
				print_error("ERROR: Only a single image preprocess can be used\n");
				return 1;
			}
			preprocess = ASTCENC_PP_PREMULTIPLY;
		}
		argidx++;
	}

#if defined(ASTCENC_DECOMPRESS_ONLY)
	flags |= ASTCENC_FLG_DECOMPRESS_ONLY;
#else
	// Decompression can skip some memory allocation, but need full tables
	if (operation == ASTCENC_OP_DECOMPRESS)
	{
		flags |= ASTCENC_FLG_DECOMPRESS_ONLY;
	}
	// Compression and test passes can skip some decimation initialization
	// as we know we are decompressing images that were compressed using the
	// same settings and heuristics ...
	else
	{
		flags |= ASTCENC_FLG_SELF_DECOMPRESS_ONLY;
	}
#endif

	astcenc_error status = astcenc_config_init(profile, block_x, block_y, block_z,
		quality, flags, &config);
	if (status == ASTCENC_ERR_BAD_BLOCK_SIZE)
	{
		print_error("ERROR: Block size '%s' is invalid\n", argv[4]);
		return 1;
	}
	else if (status == ASTCENC_ERR_BAD_CPU_FLOAT)
	{
		print_error("ERROR: astcenc must not be compiled with -ffast-math\n");
		return 1;
	}
	else if (status != ASTCENC_SUCCESS)
	{
		print_error("ERROR: Init config failed with %s\n", astcenc_get_error_string(status));
		return 1;
	}

	return 0;
}

/**
 * @brief Edit the astcenc_config
 *
 * @param         argc         Command line argument count.
 * @param[in]     argv         Command line argument vector.
 * @param         operation    Codec operation.
 * @param[out]    cli_config   Command line config.
 * @param[in,out] config       Codec configuration.
 *
 * @return 0 if everything is OK, 1 if there is some error
 */
static int edit_astcenc_config(
	int argc,
	const char** argv,
	const astcenc_operation operation,
	cli_config_options& cli_config,
	astcenc_config& config
) {

	int argidx = (operation & ASTCENC_STAGE_COMPRESS) ? 6 : 4;

	while (argidx < argc)
	{
		if (!strcmp(argv[argidx], "-silent"))
		{
			argidx++;
			cli_config.silentmode = 1;
		}
		else if (!strcmp(argv[argidx], "-cw"))
		{
			argidx += 5;
			if (argidx > argc)
			{
				print_error("ERROR: -cw switch with less than 4 arguments\n");
				return 1;
			}

			config.cw_r_weight = static_cast<float>(atof(argv[argidx - 4]));
			config.cw_g_weight = static_cast<float>(atof(argv[argidx - 3]));
			config.cw_b_weight = static_cast<float>(atof(argv[argidx - 2]));
			config.cw_a_weight = static_cast<float>(atof(argv[argidx - 1]));
		}
		else if (!strcmp(argv[argidx], "-a"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -a switch with no argument\n");
				return 1;
			}

			config.a_scale_radius = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-esw"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -esw switch with no argument\n");
				return 1;
			}

			if (strlen(argv[argidx - 1]) != 4)
			{
				print_error("ERROR: -esw pattern does not contain 4 characters\n");
				return 1;
			}

			astcenc_swz swizzle_components[4];
			for (int i = 0; i < 4; i++)
			{
				switch (argv[argidx - 1][i])
				{
				case 'r':
					swizzle_components[i] = ASTCENC_SWZ_R;
					break;
				case 'g':
					swizzle_components[i] = ASTCENC_SWZ_G;
					break;
				case 'b':
					swizzle_components[i] = ASTCENC_SWZ_B;
					break;
				case 'a':
					swizzle_components[i] = ASTCENC_SWZ_A;
					break;
				case '0':
					swizzle_components[i] = ASTCENC_SWZ_0;
					break;
				case '1':
					swizzle_components[i] = ASTCENC_SWZ_1;
					break;
				default:
					print_error("ERROR: -esw component '%c' is not valid\n", argv[argidx - 1][i]);
					return 1;
				}
			}

			cli_config.swz_encode.r = swizzle_components[0];
			cli_config.swz_encode.g = swizzle_components[1];
			cli_config.swz_encode.b = swizzle_components[2];
			cli_config.swz_encode.a = swizzle_components[3];
		}
		else if (!strcmp(argv[argidx], "-ssw"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -ssw switch with no argument\n");
				return 1;
			}

			size_t char_count = strlen(argv[argidx - 1]);
			if (char_count == 0)
			{
				print_error("ERROR: -ssw pattern contains no characters\n");
				return 1;
			}

			if (char_count > 4)
			{
				print_error("ERROR: -ssw pattern contains more than 4 characters\n");
				return 1;
			}

			bool found_r = false;
			bool found_g = false;
			bool found_b = false;
			bool found_a = false;

			for (size_t i = 0; i < char_count; i++)
			{
				switch (argv[argidx - 1][i])
				{
				case 'r':
					found_r = true;
					break;
				case 'g':
					found_g = true;
					break;
				case 'b':
					found_b = true;
					break;
				case 'a':
					found_a = true;
					break;
				default:
					print_error("ERROR: -ssw component '%c' is not valid\n", argv[argidx - 1][i]);
					return 1;
				}
			}

			config.cw_r_weight = found_r ? 1.0f : 0.0f;
			config.cw_g_weight = found_g ? 1.0f : 0.0f;
			config.cw_b_weight = found_b ? 1.0f : 0.0f;
			config.cw_a_weight = found_a ? 1.0f : 0.0f;
		}
		else if (!strcmp(argv[argidx], "-dsw"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -dsw switch with no argument\n");
				return 1;
			}

			if (strlen(argv[argidx - 1]) != 4)
			{
				print_error("ERROR: -dsw switch does not contain 4 characters\n");
				return 1;
			}

			astcenc_swz swizzle_components[4];
			for (int i = 0; i < 4; i++)
			{
				switch (argv[argidx - 1][i])
				{
				case 'r':
					swizzle_components[i] = ASTCENC_SWZ_R;
					break;
				case 'g':
					swizzle_components[i] = ASTCENC_SWZ_G;
					break;
				case 'b':
					swizzle_components[i] = ASTCENC_SWZ_B;
					break;
				case 'a':
					swizzle_components[i] = ASTCENC_SWZ_A;
					break;
				case '0':
					swizzle_components[i] = ASTCENC_SWZ_0;
					break;
				case '1':
					swizzle_components[i] = ASTCENC_SWZ_1;
					break;
				case 'z':
					swizzle_components[i] = ASTCENC_SWZ_Z;
					break;
				default:
					print_error("ERROR: ERROR: -dsw component '%c' is not valid\n", argv[argidx - 1][i]);
					return 1;
				}
			}

			cli_config.swz_decode.r = swizzle_components[0];
			cli_config.swz_decode.g = swizzle_components[1];
			cli_config.swz_decode.b = swizzle_components[2];
			cli_config.swz_decode.a = swizzle_components[3];
		}
		// presets begin here
		else if (!strcmp(argv[argidx], "-normal"))
		{
			argidx++;

			cli_config.swz_encode.r = ASTCENC_SWZ_R;
			cli_config.swz_encode.g = ASTCENC_SWZ_R;
			cli_config.swz_encode.b = ASTCENC_SWZ_R;
			cli_config.swz_encode.a = ASTCENC_SWZ_G;

			cli_config.swz_decode.r = ASTCENC_SWZ_R;
			cli_config.swz_decode.g = ASTCENC_SWZ_A;
			cli_config.swz_decode.b = ASTCENC_SWZ_Z;
			cli_config.swz_decode.a = ASTCENC_SWZ_1;
		}
		else if (!strcmp(argv[argidx], "-rgbm"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -rgbm switch with no argument\n");
				return 1;
			}

			config.rgbm_m_scale = static_cast<float>(atof(argv[argidx - 1]));
			config.cw_a_weight = 2.0f * config.rgbm_m_scale;
		}
		else if (!strcmp(argv[argidx], "-perceptual"))
		{
			argidx++;
		}
		else if (!strcmp(argv[argidx], "-pp-normalize"))
		{
			argidx++;
		}
		else if (!strcmp(argv[argidx], "-pp-premultiply"))
		{
			argidx++;
		}
		else if (!strcmp(argv[argidx], "-blockmodelimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -blockmodelimit switch with no argument\n");
				return 1;
			}

			config.tune_block_mode_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-partitioncountlimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -partitioncountlimit switch with no argument\n");
				return 1;
			}

			config.tune_partition_count_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-2partitionindexlimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -2partitionindexlimit switch with no argument\n");
				return 1;
			}

			config.tune_2partition_index_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-3partitionindexlimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -3partitionindexlimit switch with no argument\n");
				return 1;
			}

			config.tune_3partition_index_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-4partitionindexlimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -4partitionindexlimit switch with no argument\n");
				return 1;
			}

			config.tune_4partition_index_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-2partitioncandidatelimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -2partitioncandidatelimit switch with no argument\n");
				return 1;
			}

			config.tune_2partitioning_candidate_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-3partitioncandidatelimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -3partitioncandidatelimit switch with no argument\n");
				return 1;
			}

			config.tune_3partitioning_candidate_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-4partitioncandidatelimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -4partitioncandidatelimit switch with no argument\n");
				return 1;
			}

			config.tune_4partitioning_candidate_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-dblimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -dblimit switch with no argument\n");
				return 1;
			}

			if ((config.profile == ASTCENC_PRF_LDR) || (config.profile == ASTCENC_PRF_LDR_SRGB))
			{
				config.tune_db_limit = static_cast<float>(atof(argv[argidx - 1]));
			}
		}
		else if (!strcmp(argv[argidx], "-2partitionlimitfactor"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -2partitionlimitfactor switch with no argument\n");
				return 1;
			}

			config.tune_2partition_early_out_limit_factor = static_cast<float>(atof(argv[argidx - 1]));
		}
		else if (!strcmp(argv[argidx], "-3partitionlimitfactor"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -3partitionlimitfactor switch with no argument\n");
				return 1;
			}

			config.tune_3partition_early_out_limit_factor = static_cast<float>(atof(argv[argidx - 1]));
		}
		else if (!strcmp(argv[argidx], "-2planelimitcorrelation"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -2planelimitcorrelation switch with no argument\n");
				return 1;
			}

			config.tune_2plane_early_out_limit_correlation = static_cast<float>(atof(argv[argidx - 1]));
		}
		else if (!strcmp(argv[argidx], "-refinementlimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -refinementlimit switch with no argument\n");
				return 1;
			}

			config.tune_refinement_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-candidatelimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -candidatelimit switch with no argument\n");
				return 1;
			}

			config.tune_candidate_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-j"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -j switch with no argument\n");
				return 1;
			}

			cli_config.thread_count = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-repeats"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -repeats switch with no argument\n");
				return 1;
			}

			cli_config.repeat_count = atoi(argv[argidx - 1]);
			if (cli_config.repeat_count <= 0)
			{
				print_error("ERROR: -repeats value must be at least one\n");
				return 1;
			}
		}
		else if (!strcmp(argv[argidx], "-yflip"))
		{
			argidx++;
			cli_config.y_flip = 1;
		}
		else if (!strcmp(argv[argidx], "-mpsnr"))
		{
			argidx += 3;
			if (argidx > argc)
			{
				print_error("ERROR: -mpsnr switch with less than 2 arguments\n");
				return 1;
			}

			cli_config.low_fstop = atoi(argv[argidx - 2]);
			cli_config.high_fstop = atoi(argv[argidx - 1]);
			if (cli_config.high_fstop < cli_config.low_fstop)
			{
				print_error("ERROR: -mpsnr switch <low> is greater than the <high>\n");
				return 1;
			}
		}
		// Option: Encode a 3D image from a sequence of 2D images.
		else if (!strcmp(argv[argidx], "-zdim"))
		{
			// Only supports compressing
			if (!(operation & ASTCENC_STAGE_COMPRESS))
			{
				print_error("ERROR: -zdim switch is only valid for compression\n");
				return 1;
			}

			// Image depth must be specified.
			if (argidx + 2 > argc)
			{
				print_error("ERROR: -zdim switch with no argument\n");
				return 1;
			}
			argidx++;

			// Read array size (image depth).
			if (!sscanf(argv[argidx], "%u", &cli_config.array_size) || cli_config.array_size == 0)
			{
				print_error("ERROR: -zdim size '%s' is invalid\n", argv[argidx]);
				return 1;
			}

			if ((cli_config.array_size > 1) && (config.block_z == 1))
			{
				print_error("ERROR: -zdim with 3D input data for a 2D output format\n");
				return 1;
			}
			argidx++;
		}
#if defined(ASTCENC_DIAGNOSTICS)
		else if (!strcmp(argv[argidx], "-dtrace"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				print_error("ERROR: -dtrace switch with no argument\n");
				return 1;
			}

			config.trace_file_path = argv[argidx - 1];
		}
#endif
		else if (!strcmp(argv[argidx], "-dimage"))
		{
			argidx += 1;
			cli_config.diagnostic_images = true;
		}
		else // check others as well
		{
			print_error("ERROR: Argument '%s' not recognized\n", argv[argidx]);
			return 1;
		}
	}

	if (cli_config.thread_count <= 0)
	{
		cli_config.thread_count = get_cpu_count();
	}

#if defined(ASTCENC_DIAGNOSTICS)
	// Force single threaded for diagnostic builds
	cli_config.thread_count = 1;

	if (!config.trace_file_path)
	{
		print_error("ERROR: Diagnostics builds must set -dtrace\n");
		return 1;
	}
#endif

	return 0;
}

/**
 * @brief The main entry point.
 *
 * @param argc   The number of arguments.
 * @param argv   The vector of arguments.
 *
 * @return 0 on success, non-zero otherwise.
 */
uint64_t astcenc_main(const char* input_filename, unsigned char* buffer)
{
	const char* argv[] = { "astcenc", "-cl", "not used", "not used", "4x4", "-medium" };
	int argc = 6;

	astcenc_operation operation;
	astcenc_profile profile;
	int error = parse_commandline_options(argc, argv, operation, profile);
	if (error)
	{
		return 1;
	}

	// TODO: Handle RAII resources so they get freed when out of scope
	// Load the compressed input file if needed

	// This has to come first, as the block size is in the file header
	astc_compressed_image image_comp{};
	astcenc_config config{};
	astcenc_preprocess preprocess;
	error = init_astcenc_config(argc, argv, profile, operation, image_comp, preprocess, config);
	if (error)
	{
		return 1;
	}

	// Initialize cli_config_options with default values
	cli_config_options cli_config{ 0, 1, 1, false, false, false, -10, 10,
		{ ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A },
		{ ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A } };

	error = edit_astcenc_config(argc, argv, operation, cli_config, config);
	if (error)
	{
		return 1;
	}

	astcenc_image* image_uncomp_in = nullptr;
	unsigned int image_uncomp_in_component_count = 0;
	bool image_uncomp_in_is_hdr = false;

	// TODO: Handle RAII resources so they get freed when out of scope
	astcenc_error    codec_status;
	astcenc_context* codec_context;

	codec_status = astcenc_context_alloc(&config, cli_config.thread_count, &codec_context);
	if (codec_status != ASTCENC_SUCCESS)
	{
		print_error("ERROR: Codec context alloc failed: %s\n", astcenc_get_error_string(codec_status));
		return 1;
	}

	// Load the uncompressed input file if needed
	if (operation & ASTCENC_STAGE_LD_NCOMP)
	{
		image_uncomp_in = load_uncomp_file(
			input_filename, cli_config.array_size, cli_config.y_flip,
			image_uncomp_in_is_hdr, image_uncomp_in_component_count);
		if (!image_uncomp_in)
		{
			print_error("ERROR: Failed to load uncompressed image file\n");
			return 1;
		}
	}

	// Compress an image
	// if (operation & ASTCENC_STAGE_COMPRESS)
	unsigned int blocks_x = (image_uncomp_in->dim_x + config.block_x - 1) / config.block_x;
	unsigned int blocks_y = (image_uncomp_in->dim_y + config.block_y - 1) / config.block_y;
	unsigned int blocks_z = (image_uncomp_in->dim_z + config.block_z - 1) / config.block_z;
	size_t buffer_size = blocks_x * blocks_y * blocks_z * 16;
	int numMips = astc::max((int)log2f(image_uncomp_in->dim_x) >> 1, 1) - 1;
	
	unsigned char* compressBuffer = new unsigned char[(image_uncomp_in->dim_x * image_uncomp_in->dim_y *4) >> 1];
	uint64_t compressedSize = 0;

	do
	{
		compression_workload work;
		work.context  = codec_context;
		work.image    = image_uncomp_in;
		work.swizzle  = cli_config.swz_encode;
		work.data_out = buffer;
		work.data_len = buffer_size;
		work.error = ASTCENC_SUCCESS;

		work.error = astcenc_compress_image(
			work.context, work.image, &work.swizzle,
			work.data_out, work.data_len, 0);
		
		astcenc_compress_reset(codec_context);

		if (work.error != ASTCENC_SUCCESS)
		{
			print_error("ERROR: Codec compress failed: %s\n", astcenc_get_error_string(work.error));
			return 1;
		}
		
		int dim_x = image_uncomp_in->dim_x;
		int dim_y = image_uncomp_in->dim_y;

		compressedSize += buffer_size;
		buffer += buffer_size;

		if (numMips-- <= 0)
			break;

		stbir_resize(*image_uncomp_in->data, dim_x, dim_y, dim_x * 4, 
			         compressBuffer, dim_x >> 1, dim_y >> 1, (dim_x >> 1) * 4, 
			         STBIR_RGBA, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL);
		
		unsigned char* temp = (unsigned char*)*image_uncomp_in->data;
		*image_uncomp_in->data = compressBuffer;
		compressBuffer = temp;

		image_uncomp_in->dim_x >>= 1;
		image_uncomp_in->dim_y >>= 1;
		buffer_size = image_uncomp_in->dim_x * image_uncomp_in->dim_y;
	} while (true);

	// compressedSize += buffer_size;

	free_image(image_uncomp_in);
	astcenc_context_free(codec_context);

	// delete[] image_comp.data;
	delete[] compressBuffer;

	return  compressedSize;
}
