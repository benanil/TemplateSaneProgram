
/********************************************************************
*    Purpose: Creating Window, Keyboard and Mouse input, Main Loop  *
*    Author : Anilcan Gulkaya 2023 anilcangulkaya7@gmail.com        *
********************************************************************/
#if 0

#ifdef _WIN32
	#ifndef NOMINMAX
	#  define NOMINMAX
	#  define WIN32_LEAN_AND_MEAN 
	#  define VC_EXTRALEAN
	#endif
#endif

#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#else
    #define VK_USE_PLATFORM_ANDROID_KHR
#endif

#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include "../External/vulkan/vulkan.h"
#include "../External/vulkan/volk.h"
#include "../External/vulkan/shVulkan.h"
#include "../ASTL/IO.hpp"

#include "include/Platform.hpp"

#include <Windows.h>
#include <math.h>

#define SWAPCHAIN_IMAGE_COUNT          3
#define MAX_SWAPCHAIN_IMAGE_COUNT      6
#define RENDERPASS_ATTACHMENT_COUNT    3
#define SUBPASS_COLOR_ATTACHMENT_COUNT 1 

#define QUAD_VERTEX_COUNT     20
#define TRIANGLE_VERTEX_COUNT 15
#define QUAD_INDEX_COUNT      6

#define PER_VERTEX_BINDING    0
#define PER_INSTANCE_BINDING  1

#define DESCRIPTOR_SET_COUNT        1
#define INFO_DESCRIPTOR_SET_IDX     0
#define OPTIONAL_DESCRIPTOR_SET_IDX 1

namespace {
    VkInstance instance = VK_NULL_HANDLE;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR surface_capabilities = { 0 };

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physical_device_properties = { 0 };
    VkPhysicalDeviceFeatures   physical_device_features = { 0 };
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties = { 0 };

    uint32_t graphics_queue_family_index = 0;
    uint32_t present_queue_family_index = 0;

    VkDevice device = VK_NULL_HANDLE;
    uint32_t device_extension_count = 0;

    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;

    VkCommandPool graphics_cmd_pool = VK_NULL_HANDLE;
    VkCommandPool present_cmd_pool = VK_NULL_HANDLE;

    VkCommandBuffer graphics_cmd_buffers[MAX_SWAPCHAIN_IMAGE_COUNT] = { VK_NULL_HANDLE };
    VkCommandBuffer present_cmd_buffer = VK_NULL_HANDLE;

    VkFence graphics_cmd_fences[MAX_SWAPCHAIN_IMAGE_COUNT] = { VK_NULL_HANDLE };

    VkSemaphore current_image_acquired_semaphore = VK_NULL_HANDLE;
    VkSemaphore current_graphics_queue_finished_semaphore = VK_NULL_HANDLE;

	VkSharingMode swapchain_image_sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchain_image_format = VK_FORMAT_UNDEFINED;
    uint32_t swapchain_image_count = 0;

    int _width = 0, _height = 0;
	void* p_platformInstance, *p_window;

    uint32_t sample_count = 0;

	VkAttachmentDescription attachment_descriptions[RENDERPASS_ATTACHMENT_COUNT] = {};
    VkAttachmentDescription input_color_attachment = { 0 };
    VkAttachmentReference   input_color_attachment_reference = { 0 };
    VkAttachmentDescription depth_attachment = { 0 };
    VkAttachmentReference   depth_attachment_reference = { 0 };
    VkAttachmentDescription resolve_attachment = { 0 };
    VkAttachmentReference   resolve_attachment_reference = { 0 };
    VkSubpassDescription    subpass = { 0 };

	ShVkPipeline* p_pipeline;
	ShVkPipelinePool* p_pipeline_pool;
    VkRenderPass renderpass = VK_NULL_HANDLE;

    // SWAPCHAIN & FrameBuffer
    VkImage        swapchain_images[MAX_SWAPCHAIN_IMAGE_COUNT] = { VK_NULL_HANDLE };
    VkImageView    swapchain_image_views[MAX_SWAPCHAIN_IMAGE_COUNT] = { VK_NULL_HANDLE };
    VkImage        depth_image              = VK_NULL_HANDLE;
    VkDeviceMemory depth_image_memory       = VK_NULL_HANDLE;
    VkImageView    depth_image_view         = VK_NULL_HANDLE;
    VkImage        input_color_image        = VK_NULL_HANDLE;
    VkDeviceMemory input_color_image_memory = VK_NULL_HANDLE;
    VkImageView    input_color_image_view   = VK_NULL_HANDLE;
    
    VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGE_COUNT] = { VK_NULL_HANDLE };

    // Vertex index buffers, descriptors
    VkBuffer       staging_buffer     = VK_NULL_HANDLE;
	VkDeviceMemory staging_memory     = VK_NULL_HANDLE;

	VkBuffer       vertex_buffer      = VK_NULL_HANDLE;
	VkBuffer       instance_buffer    = VK_NULL_HANDLE;
	VkBuffer       index_buffer       = VK_NULL_HANDLE;
	VkBuffer       descriptors_buffer = VK_NULL_HANDLE;

	VkDeviceMemory vertex_memory      = VK_NULL_HANDLE;
	VkDeviceMemory instance_memory    = VK_NULL_HANDLE;
	VkDeviceMemory index_memory       = VK_NULL_HANDLE;
	VkDeviceMemory descriptors_memory = VK_NULL_HANDLE;

    float quad[QUAD_VERTEX_COUNT] = {
        -0.5f,-0.5f, 0.0f,  0.0f, 0.0f,
         0.5f,-0.5f, 0.0f,  0.0f, 0.0f,
         0.5f, 0.5f, 0.0f,  0.0f, 0.0f,
        -0.5f, 0.5f, 0.0f,  0.0f, 0.0f,
    };

    float triangle[TRIANGLE_VERTEX_COUNT] = {
        -1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 
         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 
         1.0f, 1.0f, 0.0f,  0.0f, 0.0f
    };

    float models[48] = {
        0.2f, 0.0f, 0.0f, 0.0f,//model 0
        0.0f, 1.3f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
       -0.4f,-0.2f, 0.3f, 1.0f,

        0.2f, 0.0f, 0.0f, 0.0f,//model 1
        0.0f, 1.3f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.4f,-0.2f, 0.2f, 1.0f,

        0.7f, 0.0f, 0.0f, 0.0f,//model 2
        0.0f, 0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.3f, 0.1f, 1.0f
    };

    uint32_t indices[QUAD_INDEX_COUNT] = {
        0, 1, 2,
        2, 3, 0
    };

    float light[8] = {
        0.0f,  2.0f, 0.0f, 1.0f, //position
        0.0f, 0.45f, 0.9f, 1.0f	 //color
    };

    float projection_view[32] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,

        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

}

static void CreateVKDevice()
{
    shGetPhysicalDeviceSurfaceCapabilities(
        physical_device, surface, &surface_capabilities
    );

    float default_queue_priority = 1.0f;
    VkDeviceQueueCreateInfo graphics_device_queue_info = { };
    shQueryForDeviceQueueInfo(
        graphics_queue_family_index,//queue_family_index
        1,//queue_count
        &default_queue_priority,//p_queue_priorities
        0,//protected
        &graphics_device_queue_info//p_device_queue_info
    );
    
    VkDeviceQueueCreateInfo present_device_queue_info = { };
    shQueryForDeviceQueueInfo(
        present_queue_family_index,//queue_family_index
        1,//queue_count
        &default_queue_priority,//p_queue_priorities
        0,//protected
        &present_device_queue_info//p_device_queue_info
    );
    
    VkDeviceQueueCreateInfo device_queue_infos[2] = {
        graphics_device_queue_info,
        present_device_queue_info
    };
    char* device_extensions[2]  = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    uint32_t device_queue_count = (graphics_queue_family_index == present_queue_family_index) ? 1 : 2;
    shSetLogicalDevice(
        physical_device,//physical_device
        &device,//p_device
        1,//extension_count
        device_extensions,//pp_extension_names
        device_queue_count,//device_queue_count
        device_queue_infos//p_device_queue_infos
    );
    
    shGetDeviceQueues(
        device,//device
        1,//queue_count
        &graphics_queue_family_index,//p_queue_family_indices
        &graphics_queue//p_queues
    );
    
    shGetDeviceQueues(
        device,//device
        1,//queue_count
        &present_queue_family_index,//p_queue_family_indices
        &present_queue//p_queues
    );
}

static void CreateFrameBufferImages(int width, int height, VkSampleCountFlagBits sample_count)
{
    // Create Depth texture
    shCreateImage(
        device,
        VK_IMAGE_TYPE_2D,
        width, height, 1,
        VK_FORMAT_D32_SFLOAT,
        1, sample_count,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        &depth_image
    );
    shAllocateImageMemory(
		device, physical_device, depth_image,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&depth_image_memory
	);
    shBindImageMemory(device, depth_image, 0, depth_image_memory);
	shCreateImageView(
		device, depth_image, VK_IMAGE_VIEW_TYPE_2D,
		VK_IMAGE_ASPECT_DEPTH_BIT, 1,
		VK_FORMAT_D32_SFLOAT, &depth_image_view
	);

    // create color texture
	shCreateImage(
		device, VK_IMAGE_TYPE_2D, width, height, 1,
		swapchain_image_format, 1, sample_count,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE,
		&input_color_image
	);
    shAllocateImageMemory(
		device, physical_device, input_color_image,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &input_color_image_memory
	);
    shBindImageMemory(device, input_color_image, 0, input_color_image_memory);

	shCreateImageView(
		device, input_color_image, VK_IMAGE_VIEW_TYPE_2D,
		VK_IMAGE_ASPECT_COLOR_BIT, 1, swapchain_image_format,
		&input_color_image_view
	);
}

static void createPipelinesDataPool(
	VkDevice          device,
	VkBuffer          descriptors_buffer,
	uint32_t          swapchain_image_count,
	ShVkPipelinePool* p_pipeline_pool
) {
	//SAME BINDING FOR ALL
	
	shPipelinePoolCreateDescriptorSetLayoutBinding(
		0,                                 //binding
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, //descriptor_type
		1,                                 //descriptor_set_count
		VK_SHADER_STAGE_FRAGMENT_BIT,      //shader_stage
		p_pipeline_pool                    //p_pipeline_pool
	);

	for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT * swapchain_image_count; i += DESCRIPTOR_SET_COUNT) {
		//INFO
		shPipelinePoolSetDescriptorBufferInfos(
			i + INFO_DESCRIPTOR_SET_IDX, //first_descriptor
			1,                           //descriptor_count
			descriptors_buffer,          //buffer
			0,                           //buffer_offset
			sizeof(light),               //buffer_size
			p_pipeline_pool              //p_pipeline_pool
		);
#if DESCRIPTOR_SET_COUNT == 2
		//OPTIONAL
		shPipelinePoolSetDescriptorBufferInfos(
			i + OPTIONAL_DESCRIPTOR_SET_IDX, //first_descriptor
			1,                               //descriptor_count
			descriptors_buffer,              //buffer
			0,                               //buffer_offset
			sizeof(light) / 2,               //buffer_size
			p_pipeline_pool                  //p_pipeline_pool
		);
#endif
	}

	//SAME DESCRIPTOR SET LAYOUT FOR ALL
	shPipelinePoolCreateDescriptorSetLayout(
		device,         //device
		0,              //first_binding_idx
		1,              //binding_count
		0,              //set_layout_idx
		p_pipeline_pool //p_pipeline_pool
	);
	
	shPipelinePoolCopyDescriptorSetLayout(
		0,                                            //src_set_layout_idx
		0,                                            //first_dst_set_layout_idx
		DESCRIPTOR_SET_COUNT * swapchain_image_count, //dst_set_layout_count
		p_pipeline_pool                               //p_pipeline_pool
	);

	//SAME DESCRIPTOR POOL FOR ALL
	shPipelinePoolCreateDescriptorPool(
		device,                                       //device
		0,                                            //pool_idx
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            //descriptor_type
		DESCRIPTOR_SET_COUNT * swapchain_image_count, //decriptor_count
		p_pipeline_pool                               //p_pipeline_pool
	);

	shPipelinePoolAllocateDescriptorSetUnits(
		device,                                       //device,
		0,                                            //binding,
		0,                                            //pool_idx,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            //descriptor_type,
		0,                                            //first_descriptor_set_unit,
		DESCRIPTOR_SET_COUNT * swapchain_image_count, //descriptor_set_unit_count,
		p_pipeline_pool                               //p_pipeline
	);

	//UPDATE ALL
	shPipelinePoolUpdateDescriptorSetUnits(
		device, 0, DESCRIPTOR_SET_COUNT * swapchain_image_count, p_pipeline_pool
	);

	return;
}


static void createPipeline(
	VkDevice              device,
	VkRenderPass          renderpass,
	uint32_t              width,
	uint32_t              height,
	VkSampleCountFlagBits sample_count,
	uint32_t              swapchain_image_count
) {
	uint32_t attribute_0_offset = 0;
	uint32_t attribute_0_size   = 12;
	uint32_t attribute_1_offset = attribute_0_offset + attribute_0_size;
	uint32_t attribute_1_size   = 8;

	shPipelineSetVertexBinding(
		PER_VERTEX_BINDING,
		attribute_0_size + attribute_1_size,
		VK_VERTEX_INPUT_RATE_VERTEX,
		p_pipeline
	);

	shPipelineSetVertexAttribute(
		0,
		PER_VERTEX_BINDING,
		VK_FORMAT_R32G32B32_SFLOAT,
		attribute_0_offset,
		p_pipeline
	);

	shPipelineSetVertexAttribute(
		1,
		PER_VERTEX_BINDING,
		VK_FORMAT_R32G32_SFLOAT,
		attribute_1_offset,
		p_pipeline
	);

	shPipelineSetVertexBinding(
		PER_INSTANCE_BINDING,
		64,
		VK_VERTEX_INPUT_RATE_INSTANCE,
		p_pipeline
	);

	shPipelineSetVertexAttribute(
		2,
		PER_INSTANCE_BINDING,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		0,
		p_pipeline
	);

	shPipelineSetVertexAttribute(
		3,
		PER_INSTANCE_BINDING,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		16,
		p_pipeline
	);

	shPipelineSetVertexAttribute(
		4,
		PER_INSTANCE_BINDING,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		32,
		p_pipeline
	);

	shPipelineSetVertexAttribute(
		5,
		PER_INSTANCE_BINDING,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		48,
		p_pipeline
	);

	shPipelineSetVertexInputState(p_pipeline);

	shPipelineCreateInputAssembly(
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		SH_FALSE,
		p_pipeline
	);

	shPipelineCreateRasterizer(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, p_pipeline);

	shPipelineSetMultisampleState(sample_count, 0.0f, p_pipeline );

	shPipelineSetViewport(
		0, 0,
		width, height,
		0, 0,
		width, height,
		p_pipeline
	);

	shPipelineColorBlendSettings(SH_FALSE, SH_TRUE, SUBPASS_COLOR_ATTACHMENT_COUNT, p_pipeline);

	const char* vertPath = "Shaders/mesh.vert.spv";
	const char* fragPath = "Shaders/mesh.frag.spv";

	uint32_t shader_size = FileSize(vertPath);
	char* shader_code = ReadAllFile(vertPath);

	shPipelineCreateShaderModule(
		device,
		shader_size,
		shader_code,
		p_pipeline
	);

	FreeAllText(shader_code);

	shPipelineCreateShaderStage(VK_SHADER_STAGE_VERTEX_BIT, p_pipeline );

	shader_code = ReadAllFile(fragPath);

	shPipelineCreateShaderModule(
		device,
		shader_size,
		shader_code,
		p_pipeline
	);

	shPipelineCreateShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, p_pipeline );

	shPipelineSetPushConstants(
		VK_SHADER_STAGE_VERTEX_BIT,
		0,
		sizeof(projection_view),
		p_pipeline
	);

	shPipelineCreateLayout(
		device,
		0,
		DESCRIPTOR_SET_COUNT * swapchain_image_count,
		p_pipeline_pool,
		p_pipeline
	);

	shSetupGraphicsPipeline(device, renderpass, p_pipeline);
	return;
}

static void CreatePipeline(int width, int height, VkSampleCountFlagBits sample_count)
{
	p_pipeline_pool = shAllocatePipelinePool();
	p_pipeline = &p_pipeline_pool->pipelines[0];

	createPipelinesDataPool(
		device,
		descriptors_buffer,
		swapchain_image_count,
		p_pipeline_pool
	);

	createPipeline(
		device, renderpass, 
		width, height, sample_count,
		(VkSampleCountFlagBits)swapchain_image_count
	);
}

static void CreateVKSwapchains(int width, int height)
{
    swapchain_image_sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
	if (graphics_queue_family_index != present_queue_family_index) {
		swapchain_image_sharing_mode = VK_SHARING_MODE_CONCURRENT;
	}
	shCreateSwapchain(
		device,//device
		physical_device,//physical_device
		surface,//surface
		VK_FORMAT_R32G32B32A32_SFLOAT,//image_format
		&swapchain_image_format,//p_image_format
		SWAPCHAIN_IMAGE_COUNT,//swapchain_image_count
		swapchain_image_sharing_mode,//image_sharing_mode
		SH_FALSE,//vsync
		&swapchain_image_count,
		&swapchain//p_swapchain
	);//need p_swapchain_image_count

	shCreateCommandPool(
		device,//device
		graphics_queue_family_index,//queue_family_index
		&graphics_cmd_pool//p_cmd_pool
	);

	shAllocateCommandBuffers(
		device,//device
		graphics_cmd_pool,//cmd_pool
		swapchain_image_count,//cmd_buffer_count
		graphics_cmd_buffers//p_cmd_buffer
	);

	if (graphics_queue_family_index != present_queue_family_index) {
		shCreateCommandPool(
			device,//device
			present_queue_family_index,//queue_family_index
			&present_cmd_pool//p_cmd_pool
		);
	}
	else {
		present_cmd_pool   = graphics_cmd_pool;
	}
	shAllocateCommandBuffers(
		device,//device
		present_cmd_pool,//cmd_pool
		1,//cmd_buffer_count
		&present_cmd_buffer//p_cmd_buffer
	);

	shCreateFences(
		device,//device
		swapchain_image_count,//fence_count
		1,//signaled
		graphics_cmd_fences//p_fences
	);

	shGetSwapchainImages(
		device,//device
		swapchain,//swapchain
		&swapchain_image_count,//p_swapchain_image_count
		swapchain_images//p_swapchain_images
	);

	for (uint32_t i = 0; i < swapchain_image_count; i++) {
		shCreateImageView(
			device,//device
			swapchain_images[i],//image
			VK_IMAGE_VIEW_TYPE_2D,//view_type
			VK_IMAGE_ASPECT_COLOR_BIT,//image_aspect
			1,//mip_levels
			swapchain_image_format,//format
			&swapchain_image_views[i]//p_image_view
		);
	}

	shCombineMaxSamples(physical_device_properties, 64, 1, 1, &sample_count);

    // below is like frame buffer attachment
	// color attachment
    shCreateRenderpassAttachment(
		swapchain_image_format,//format
		sample_count,//sample_count
		VK_ATTACHMENT_LOAD_OP_CLEAR,//load_treatment
		VK_ATTACHMENT_STORE_OP_STORE,//store_treatment
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,//stencil_load_treatment
		VK_ATTACHMENT_STORE_OP_DONT_CARE,//stencil_store_treatment
		VK_IMAGE_LAYOUT_UNDEFINED,//initial_layout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,//final_layout
		&input_color_attachment//p_attachment_description
	);
	shCreateRenderpassAttachmentReference(
		0,//attachment_idx
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,//layout
		&input_color_attachment_reference//p_attachment_reference
	);

    // depth attachment
    shCreateRenderpassAttachment(
		VK_FORMAT_D32_SFLOAT,
		sample_count,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		&depth_attachment
	);
	shCreateRenderpassAttachmentReference(
		1,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		&depth_attachment_reference
	);
	
    // present attachment
	shCreateRenderpassAttachment(
		swapchain_image_format,
		1,//sample count
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		&resolve_attachment
	);
	shCreateRenderpassAttachmentReference(
		2,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		&resolve_attachment_reference
	);

    shCreateSubpass(
		VK_PIPELINE_BIND_POINT_GRAPHICS,//bind_point
		0,//input_attachment_count
		VK_NULL_HANDLE,//p_input_attachments_reference
		SUBPASS_COLOR_ATTACHMENT_COUNT,//color_attachment_count
		&input_color_attachment_reference,//p_color_attachments_reference
		&depth_attachment_reference,//p_depth_stencil_attachment_reference
		&resolve_attachment_reference,//p_resolve_attachment_reference
		0,//preserve_attachment_count
		VK_NULL_HANDLE,//p_preserve_attachments
		&subpass//p_subpass
	);

	attachment_descriptions[0] = input_color_attachment;
	attachment_descriptions[1] = depth_attachment;
	attachment_descriptions[2] = resolve_attachment;
	
	shCreateRenderpass(
		device,//device
		RENDERPASS_ATTACHMENT_COUNT,//attachment_count
		attachment_descriptions,//p_attachments_descriptions
		1,//subpass_count
		&subpass,//p_subpasses
		&renderpass//p_renderpass
	);

    CreateFrameBufferImages(width, height, VK_SAMPLE_COUNT_1_BIT);

    for (uint32_t i = 0; i < swapchain_image_count; i++) {
		VkImageView image_views[RENDERPASS_ATTACHMENT_COUNT] = {
			input_color_image_view, depth_image_view, swapchain_image_views[i]
		};
		shCreateFramebuffer(
			device,//device
			renderpass,//renderpass
			RENDERPASS_ATTACHMENT_COUNT,//image_view_count
			image_views,//p_image_views
			surface_capabilities.currentExtent.width,//x
			surface_capabilities.currentExtent.height,//y
			1,//z
			&framebuffers[i]//p_framebuffer
		);
	}
}

static void CreateSemaphores()
{
    shCreateSemaphores(
		device,//device
		1,//semaphore_count
		&current_image_acquired_semaphore//p_semaphores
	);

	shCreateSemaphores(
		device,//device
		1,//semaphore_count
		&current_graphics_queue_finished_semaphore//p_semaphores
	);
}

static void CreateSurface()
{
#ifdef _WIN32
	VkWin32SurfaceCreateInfoKHR sci = {};
	sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	sci.hinstance = (HINSTANCE)p_platformInstance;
	sci.hwnd = (HWND)p_window;
	if (vkCreateWin32SurfaceKHR(instance, &sci, VK_NULL_HANDLE, &surface)) {
		AX_ERROR("vkCreateWin32SurfaceKHR failed!");
	}
#else
	VkAndroidSurfaceCreateInfoKHR sci = {};
	sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	sci.window = window;
	if (vkCreateAndroidSurfaceKHR(instance, &sci, VK_NULL_HANDLE, &surface)) {
		AX_ERROR("vkCreateAndroid32SurfaceKHR failed!");
	}
#endif
}

bool InitVulkan(void* platformInstance, void* window, int width, int height)
{
    /* Try to initialize volk. This might not work on CI builds, but the
     * above should have compiled at least. */
    VkResult r = volkInitialize();
    if (r != VK_SUCCESS) {
        AX_ERROR("volkInitialize failed!\n");
        return 0;
    }

    uint32_t version = volkGetInstanceVersion();
    AX_LOG("Vulkan version %d.%d.%d initialized.\n",
            VK_VERSION_MAJOR(version),
            VK_VERSION_MINOR(version),
            VK_VERSION_PATCH(version));
	
	const char* pp_instance_extensions[] = { "VK_KHR_surface"};
	uint32_t instance_extension_count = sizeof(pp_instance_extensions) / sizeof(const char*);

    shCreateInstance(
        //application_name, engine_name, enable_validation_layers,
        "vulkan app", "vulkan engine", 1,
        instance_extension_count, pp_instance_extensions,
        VK_MAKE_API_VERSION(1, 3, 0, 0),//api_version,
        &instance
    );
	
	p_platformInstance = platformInstance;
	p_window = window;
	CreateSurface();

    shSelectPhysicalDevice(
        instance,//instance,
        surface,//surface,
        VK_QUEUE_GRAPHICS_BIT |
        VK_QUEUE_COMPUTE_BIT |
        VK_QUEUE_TRANSFER_BIT,//requirements,
        &physical_device,//p_physical_device,
        &physical_device_properties,//p_physical_device_properties,
        &physical_device_features,//p_physical_device_features,
        &physical_device_memory_properties//p_physical_device_memory_properties
    );

    uint32_t graphics_queue_families_indices[SH_MAX_STACK_QUEUE_FAMILY_COUNT] = { 0 };
    uint32_t present_queue_families_indices [SH_MAX_STACK_QUEUE_FAMILY_COUNT] = { 0 };
    shGetPhysicalDeviceQueueFamilies(
        physical_device,//physical_device
        surface,//surface
        VK_NULL_HANDLE,//p_queue_family_count
        VK_NULL_HANDLE,//p_graphics_queue_family_count
        VK_NULL_HANDLE,//p_surface_queue_family_count
        VK_NULL_HANDLE,//p_compute_queue_family_count
        VK_NULL_HANDLE,//p_transfer_queue_family_count
        graphics_queue_families_indices,//p_graphics_queue_family_indices
        present_queue_families_indices,//p_surface_queue_family_indices
        VK_NULL_HANDLE,//p_compute_queue_family_indices
        VK_NULL_HANDLE,//p_transfer_queue_family_indices
        VK_NULL_HANDLE//p_queue_families_properties
    );
    graphics_queue_family_index = graphics_queue_families_indices[0];
    present_queue_family_index  = present_queue_families_indices [0];

    CreateVKDevice();

    CreateVKSwapchains(width, height);

    CreateSemaphores();

    CreatePipeline(width, height, VK_SAMPLE_COUNT_1_BIT);
    return 1;
}

static void VulkanLoop(int width, int height)
{
    uint32_t swapchain_image_idx  = 0;
	uint8_t  swapchain_suboptimal = 0;
    
    if (width < 8 || height < 8)
        return;
    
    // is window  resized
    if (_width != width || _height != height) 
    {
    	_width  = width;
    	_height = height;
    
    	resizeWindow(width, height);
    	shDestroyPipeline (device, p_pipeline->pipeline);
    	shPipelineSetViewport(0, 0,width, height, 0, 0,width, height, p_pipeline);
    	shSetupGraphicsPipeline(device, renderpass, p_pipeline);
    }
    
    shAcquireSwapchainImage(
        device,//device
        swapchain,//swapchain
        UINT64_MAX,//timeout_ns
        current_image_acquired_semaphore,//acquired_signal_semaphore
        VK_NULL_HANDLE,//acquired_signal_fence
        &swapchain_image_idx,//p_swapchain_image_index
        &swapchain_suboptimal//p_swapchain_suboptimal
    );
    
    if (swapchain_suboptimal) {
    	resizeWindow(width, height);
    	swapchain_suboptimal = 0;
    }
    
    shWaitForFences(
    	device,//device
    	1,//fence_count
    	&graphics_cmd_fences[swapchain_image_idx],//p_fences
    	1,//wait_for_all
    	UINT64_MAX//timeout_ns
    );
    
    shResetFences(
    	device,//device
    	1,//fence_count
    	&graphics_cmd_fences[swapchain_image_idx]//p_fences
    );
    
    VkCommandBuffer cmd_buffer = graphics_cmd_buffers[swapchain_image_idx];
    
    shBeginCommandBuffer(cmd_buffer);
    
    triangle[6] = (float)sin(TimeSinceStartup());
    shWriteMemory(
    	device,
    	staging_memory,
    	sizeof(quad),
    	sizeof(triangle),
    	triangle
    );
    shCopyBuffer(
    	cmd_buffer,
    	staging_buffer,
    	sizeof(quad), sizeof(quad), sizeof(triangle),
    	vertex_buffer
    );
    
    VkClearValue clear_values[2] = { 0 };
    float* p_colors = clear_values[0].color.float32;
    p_colors[0] = 0.1f;
    p_colors[1] = 0.1f;
    p_colors[2] = 0.1f;
    
    clear_values[1].depthStencil.depth = 1.0f;
    
    shBeginRenderpass(
    	cmd_buffer,//graphics_cmd_buffer
    	renderpass,//renderpass
    	0,//render_offset_x
    	0,//render_offset_y
    	surface_capabilities.currentExtent.width,//render_size_x
    	surface_capabilities.currentExtent.height,//render_size_y
    	2,//only attachments with VK_ATTACHMENT_LOAD_OP_CLEAR
    	clear_values,//p_clear_values
    	framebuffers[swapchain_image_idx]//framebuffer
    );
    
    VkDeviceSize vertex_offset = 0;
    VkDeviceSize vertex_offsets[2] = { 0, 0 };
    VkBuffer     vertex_buffers[2] = { vertex_buffer, instance_buffer };
    shBindVertexBuffers(cmd_buffer, 0, 2, vertex_buffers, vertex_offsets);
    
    shBindIndexBuffer(cmd_buffer, 0, index_buffer);
    
    shBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p_pipeline);
    
    shPipelinePushConstants(cmd_buffer, projection_view, p_pipeline);
    
    shPipelineBindDescriptorSetUnits(
    	cmd_buffer,                                 //cmd_buffer
    	INFO_DESCRIPTOR_SET_IDX,                    //first_descriptor_set
    	DESCRIPTOR_SET_COUNT * swapchain_image_idx, //first_descriptor_set_unit_idx
    	DESCRIPTOR_SET_COUNT,                       //descriptor_set_unit_count
    	VK_PIPELINE_BIND_POINT_GRAPHICS,            //bind_point
    	0,                                          //dynamic_descriptors_count
    	VK_NULL_HANDLE,                             //p_dynamic_offsets
    	p_pipeline_pool,                            //p_pipeline_pool
    	p_pipeline                                  //p_pipeline
    );
    
    shDrawIndexed(cmd_buffer, QUAD_INDEX_COUNT, 2, 0, 0, 0);
    
    shDraw(cmd_buffer, 3, 4, 1, 2);
    
    shEndRenderpass(cmd_buffer);
    
    shEndCommandBuffer(cmd_buffer);
    
    shQueueSubmit(
    	1,//cmd_buffer_count
    	&cmd_buffer,//p_cmd_buffers
    	graphics_queue,//queue
    	graphics_cmd_fences[swapchain_image_idx],//fence
    	1,//semaphores_to_wait_for_count
    	&current_image_acquired_semaphore,//p_semaphores_to_wait_for
    	VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,//wait_stage
    	1,//signal_semaphore_count
    	&current_graphics_queue_finished_semaphore//p_signal_semaphores
    );
    
    shQueuePresentSwapchainImage(
    	present_queue,//present_queue
    	1,//semaphores_to_wait_for_count
    	&current_graphics_queue_finished_semaphore,//p_semaphores_to_wait_for
    	swapchain,//swapchain
    	swapchain_image_idx//swapchain_image_idx
    );
    
    swapchain_image_idx = (swapchain_image_idx + 1) % swapchain_image_count;
}

static void resizeWindow(uint32_t width, uint32_t height)
{
	shWaitDeviceIdle(device);

	shDestroyRenderpass(device, renderpass);
	shDestroyFramebuffers(device, swapchain_image_count, framebuffers);
	shDestroyImageViews(device, swapchain_image_count, swapchain_image_views);
	shDestroySwapchain(device, swapchain);
	shDestroySurface(instance, surface);

	shClearImageMemory(device, depth_image, depth_image_memory);
	shClearImageMemory(device, input_color_image, input_color_image_memory);
	shDestroyImageViews(device, 1, &depth_image_view);
	shDestroyImageViews(device, 1, &input_color_image_view);

	CreateSurface();
	shGetPhysicalDeviceSurfaceSupport(physical_device, graphics_queue_family_index, surface, NULL);
	shGetPhysicalDeviceSurfaceCapabilities(physical_device, surface, &surface_capabilities);
	shCreateSwapchain(
		device, physical_device, surface,
		swapchain_image_format,
		&swapchain_image_format,
		SWAPCHAIN_IMAGE_COUNT,
		swapchain_image_sharing_mode,
		SH_FALSE,
		&swapchain_image_count,
		&swapchain
	);
	shGetSwapchainImages(device, swapchain, &swapchain_image_count, swapchain_images);
	for (uint32_t i = 0; i < swapchain_image_count; i++) {
		shCreateImageView(
			device, swapchain_images[i],
			VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT,
			1, swapchain_image_format,
			&swapchain_image_views[i]
		);
	}

	shCreateImage(
		device, VK_IMAGE_TYPE_2D,
		width, height, 1,
		VK_FORMAT_D32_SFLOAT,
		1, (VkSampleCountFlagBits)sample_count,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE, &depth_image
	);
	shAllocateImageMemory(
		device, physical_device, depth_image,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&depth_image_memory
	);
	shBindImageMemory(
		device, depth_image, 0, depth_image_memory
	);
	shCreateImageView(
		device, depth_image, VK_IMAGE_VIEW_TYPE_2D,
		VK_IMAGE_ASPECT_DEPTH_BIT, 1,
		VK_FORMAT_D32_SFLOAT, &depth_image_view
	);

	shCreateImage(
		device, VK_IMAGE_TYPE_2D,
		width, height, 1,
		swapchain_image_format,
		1, (VkSampleCountFlagBits)sample_count,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		&input_color_image
	);
	shAllocateImageMemory(
		device, physical_device, input_color_image,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&input_color_image_memory
	);
	shBindImageMemory(
		device, input_color_image, 0, input_color_image_memory
	);
	shCreateImageView(
		device, input_color_image, VK_IMAGE_VIEW_TYPE_2D,
		VK_IMAGE_ASPECT_COLOR_BIT, 1, swapchain_image_format,
		&input_color_image_view
	);

	shCreateRenderpass(device, RENDERPASS_ATTACHMENT_COUNT, attachment_descriptions, 1, &subpass, &renderpass);
	for (uint32_t i = 0; i < (swapchain_image_count); i++) {
		VkImageView image_views[RENDERPASS_ATTACHMENT_COUNT] = {
			input_color_image_view, depth_image_view, swapchain_image_views[i]
		};
		shCreateFramebuffer(device, renderpass, RENDERPASS_ATTACHMENT_COUNT, image_views, width, height, 1, &framebuffers[i]);
	}

	return;
}

static void writeMemory(VkCommandBuffer cmd_buffer, VkFence fence, VkQueue transfer_queue) 
{	
	//USEFUL VARIABLES
	uint32_t quad_vertices_offset     = 0;
	uint32_t triangle_vertices_offset = quad_vertices_offset     + sizeof(quad);
	uint32_t instance_models_offset   = triangle_vertices_offset + sizeof(triangle);
	uint32_t quad_indices_offset      = instance_models_offset   + sizeof(models);
	uint32_t light_offset             = quad_indices_offset      + sizeof(indices);

	uint32_t staging_size = light_offset + sizeof(light);

	//WRITE ALL DATA TO STAGING BUFFER
	shCreateBuffer(
		device,
		staging_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		&staging_buffer
	);
	shAllocateBufferMemory(
		device,
		physical_device,
		staging_buffer,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_memory
	);
	shWriteMemory(device, staging_memory, quad_vertices_offset,     sizeof(quad),     quad);
	shWriteMemory(device, staging_memory, triangle_vertices_offset, sizeof(triangle), triangle);
	shWriteMemory(device, staging_memory, instance_models_offset,   sizeof(models),   models);
	shWriteMemory(device, staging_memory, quad_indices_offset,      sizeof(indices),  indices);
	shWriteMemory(device, staging_memory, light_offset,             sizeof(light),    light);

	shBindBufferMemory(device, staging_buffer, 0, staging_memory);

	//SETUP DEVICE LOCAL DESTINATION BUFFERS
	shCreateBuffer(device, sizeof(quad) + sizeof(triangle), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, &vertex_buffer);
	shAllocateBufferMemory(device, physical_device, vertex_buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertex_memory);
	shBindBufferMemory(device, vertex_buffer, 0, vertex_memory);

	shCreateBuffer(device, sizeof(models), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, &instance_buffer);
	shAllocateBufferMemory(device, physical_device, instance_buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &instance_memory);
	shBindBufferMemory(device, instance_buffer, 0, instance_memory);

	shCreateBuffer(device, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, &index_buffer);
	shAllocateBufferMemory(device, physical_device, index_buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &index_memory);
	shBindBufferMemory(device, index_buffer, 0, index_memory);

	shCreateBuffer(device, sizeof(light), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, &descriptors_buffer);
	shAllocateBufferMemory(device, physical_device, descriptors_buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &descriptors_memory);
	shBindBufferMemory(device, descriptors_buffer, 0, descriptors_memory);

	//COPY STAGING BUFFER TO DEVICE LOCAL MEMORY
	shResetFences(device, 1, &fence);//to signaled
	shBeginCommandBuffer(cmd_buffer);
	shCopyBuffer(cmd_buffer, staging_buffer, quad_vertices_offset,   0, sizeof(quad) + sizeof(triangle), vertex_buffer);
	shCopyBuffer(cmd_buffer, staging_buffer, instance_models_offset, 0, sizeof(models),                  instance_buffer);
	shCopyBuffer(cmd_buffer, staging_buffer, quad_indices_offset,    0, sizeof(indices),                 index_buffer);
	shCopyBuffer(cmd_buffer, staging_buffer, light_offset,           0, sizeof(light),                   descriptors_buffer);
	shEndCommandBuffer(cmd_buffer);

	shQueueSubmit(1, &cmd_buffer, transfer_queue, fence, 0, VK_NULL_HANDLE, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_NULL_HANDLE);
	shWaitForFences(device, 1, &fence, 1, UINT64_MAX);

	return;
}

static void VulkanDestroy()
{
	shWaitDeviceIdle(device);

	shPipelinePoolDestroyDescriptorPools(device, 0, 1, p_pipeline_pool);
	shPipelinePoolDestroyDescriptorSetLayouts(device, 0, 1, p_pipeline_pool);

	shPipelineDestroyShaderModules(device, 0, 2, p_pipeline);
	shPipelineDestroyLayout(device, p_pipeline);
	shDestroyPipeline(device, p_pipeline->pipeline);

	shClearPipeline(p_pipeline);

	shFreePipelinePool(p_pipeline_pool);

	shDestroySemaphores(device, 1, &current_image_acquired_semaphore);

	shDestroySemaphores(device, 1, &current_graphics_queue_finished_semaphore);

	shDestroyFences(device, swapchain_image_count, graphics_cmd_fences);

	shDestroyCommandBuffers(device, graphics_cmd_pool, swapchain_image_count, graphics_cmd_buffers);

	shDestroyCommandBuffers(device, present_cmd_pool, 1, &present_cmd_buffer);

	shDestroyCommandPool(device, graphics_cmd_pool);
	if (graphics_queue_family_index != present_queue_family_index) {
		shDestroyCommandPool(device, present_cmd_pool);
	}

	// releaseMemory
	shWaitDeviceIdle(device);
	shClearBufferMemory(device, staging_buffer, staging_memory);
	shClearBufferMemory(device, vertex_buffer, vertex_memory);
	shClearBufferMemory(device, instance_buffer, instance_memory);
	shClearBufferMemory(device, index_buffer, index_memory);
	shClearBufferMemory(device, descriptors_buffer, descriptors_memory);

	shClearImageMemory(device, depth_image, depth_image_memory);
	shClearImageMemory(device, input_color_image, input_color_image_memory);
	shDestroyImageViews(device, 1, &depth_image_view);
	shDestroyImageViews(device, 1, &input_color_image_view);

	shDestroyRenderpass(device, renderpass);

	shDestroyFramebuffers(device, swapchain_image_count, framebuffers);

	shDestroyImageViews(device, swapchain_image_count, swapchain_image_views);

	shDestroySwapchain(device, swapchain);

	shDestroyDevice(device);

	shDestroySurface(instance, surface);

	shDestroyInstance(instance);
}

#endif