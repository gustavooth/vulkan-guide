#include <stdio.h>
#include <stdlib.h>

#ifdef PLATFORM_WAYLAND
  #define VK_USE_PLATFORM_WAYLAND_KHR
  #include "platform/linux/platform_linux.h"
#endif
#include <vulkan/vulkan.h>
#include "defines.h"
#include "platform/platform.h"
#include "core/events.h"

typedef struct QueueIndex {
  u32 familyIndex;
  u32 index;
} QueueIndex;

const u32 MAX_FRAMES = 3;

typedef struct VkContext {
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;

  VkPhysicalDevice physicalDevice;
  VkDevice device;

  QueueIndex graphics_queue_index;
  VkQueue graphics_queue;

  VkCommandPool command_pool;
  VkCommandBuffer *command_buffers; // MAX FRAMES

  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  u32 swapchain_image_count;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  u32 image_index;

  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;
  VkPipeline graphics_pipeline;
  VkFramebuffer *framebuffers; //IMAGE COUNT

  VkSemaphore *image_available_semaphores; // MAX FRAMES
  VkSemaphore *render_finished_semaphores; // MAX FRAMES
  VkFence *in_flight_fences; // MAX FRAMES
  VkFence *images_in_flight; //IMAGE_COUNT
  u32 current_frame;

  u32 image_width;
  u32 image_height;

  u32 next_width;
  u32 next_height;
} VkContext;

VkContext ctx = {0};
Window window;
b8 running = true;

b8 create_instance() {
  printf("Creating instance ... ");

  VkApplicationInfo app_info = {0};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "VULKAN-GUIDE";
  app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 1);
  app_info.pEngineName = "VULKAN-GUIDE-ENGINE";
  app_info.engineVersion = VK_MAKE_VERSION(0, 1, 1);
  app_info.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo instance_info = {0};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

  u32 instance_ext_count = 3;
  const char **instance_extensions = malloc(sizeof(const char *) * instance_ext_count);
  const char *surface_ext = VK_KHR_SURFACE_EXTENSION_NAME;
  const char *debug_ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  const char *platform_ext = "VK_KHR_wayland_surface";
  instance_extensions[0] = surface_ext;
  instance_extensions[1] = debug_ext;
  instance_extensions[2] = platform_ext;

  instance_info.enabledExtensionCount = instance_ext_count;
  instance_info.ppEnabledExtensionNames = instance_extensions;

  if(vkCreateInstance(&instance_info, NULL, &ctx.instance) != VK_SUCCESS) {
    printf("FAIL\n");
    return false;
  }

  printf("SUCCESS\n");
  return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
  void *pUserData)
{
  printf("DEBUG MESSAGE: %s", pCallbackData->pMessage);
  return VK_FALSE;
}

b8 setup_debug_messenger()
{
  printf("Creating debug messenger ... ");
  VkDebugUtilsMessengerCreateInfoEXT debug_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug_info.pfnUserCallback = debug_callback;

  PFN_vkCreateDebugUtilsMessengerEXT func =
    (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx.instance, "vkCreateDebugUtilsMessengerEXT");

  if (func == 0)
  {
    printf("FAIL 1\n");
    return false;
  }

  if (func(ctx.instance, &debug_info, 0, &ctx.debug_messenger) != VK_SUCCESS)
  {
    printf("FAIL2\n");
    return false;
  }

  printf("SUCCESS \n");
  return true;
}

b8 choose_physical_device() {
  printf("Choosing physical device ... ");

  u32 device_count = 0;
  if(vkEnumeratePhysicalDevices(ctx.instance, &device_count, NULL) != VK_SUCCESS) {
    printf("FAIL 1\n");
    return false;
  };
  VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
  if(vkEnumeratePhysicalDevices(ctx.instance, &device_count, devices) != VK_SUCCESS) {
    printf("FAIL 2\n");
    return false;
  };

  b8 found = false;
  for (u32 i = 0; i < device_count; i++)
  {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(devices[i], &properties);
    printf("Checking device %s ... ", properties.deviceName);

    if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      continue;
    }

    ctx.physicalDevice = devices[i];
    found = true;
    break;
  }

  if(!found) {
    printf("FAIL 3\n");
    return false;
  }

  u32 queueFamilyPropertiesCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &queueFamilyPropertiesCount, NULL);
  VkQueueFamilyProperties *queue_properties = malloc(sizeof(VkQueueFamilyProperties) * queueFamilyPropertiesCount);
  vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &queueFamilyPropertiesCount, queue_properties);

  for (u32 i = 0; i < queueFamilyPropertiesCount; i++)
  {
    if(queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      ctx.graphics_queue_index.familyIndex = i;
      ctx.graphics_queue_index.index = 0;
      printf("Graphics Queue Family Index: %u ", i);
      break;
    }
  }

  printf("SUCCESS\n");
  return true;
}

b8 create_logical_device() {
  printf("Creating logical device ... ");

  VkDeviceCreateInfo device_info = {0};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

  VkDeviceQueueCreateInfo graphics_queue_info = {0};
  graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  graphics_queue_info.queueFamilyIndex = ctx.graphics_queue_index.familyIndex;
  graphics_queue_info.queueCount = 1;
  f32 queue_priority[] = {1.f};
  graphics_queue_info.pQueuePriorities = queue_priority;

  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &graphics_queue_info;

  const char *swapchain_ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  device_info.enabledExtensionCount = 1;
  device_info.ppEnabledExtensionNames = &swapchain_ext;

  VkPhysicalDeviceFeatures features = {0};
  features.samplerAnisotropy = VK_TRUE;
  device_info.pEnabledFeatures = &features;

  if(vkCreateDevice(ctx.physicalDevice, &device_info, NULL, &ctx.device) != VK_SUCCESS) {
    printf("FAIL 1 \n");
    return false;
  }

  vkGetDeviceQueue(ctx.device, ctx.graphics_queue_index.familyIndex, ctx.graphics_queue_index.index, &ctx.graphics_queue);

  printf("SUCCESS\n");
  return true;
}

b8 allocate_command_buffers() {
  printf("Creating command pool ... ");

  VkCommandPoolCreateInfo pool_info = {0};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = ctx.graphics_queue_index.familyIndex;

  if(vkCreateCommandPool(ctx.device, &pool_info, NULL, &ctx.command_pool) != VK_SUCCESS) {
    printf("FAIL 1\n");
    return false;
  }

  printf("Allocating command buffers ... ");

  ctx.command_buffers = malloc(sizeof(VkCommandBuffer) * MAX_FRAMES);

  VkCommandBufferAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandPool = ctx.command_pool;
  alloc_info.commandBufferCount = MAX_FRAMES;

  if(vkAllocateCommandBuffers(ctx.device, &alloc_info, ctx.command_buffers) != VK_SUCCESS) {
    printf("FAIL 2\n");
    return false;
  }

  printf("SUCCESS\n");
  return true;
}

b8 create_surface() {
#ifdef PLATFORM_WAYLAND
  printf("Creating Linux Wayland Surface ... ");
  WaylandState *wayland_state = platform_linux_get_wayland_state(&window);

  VkWaylandSurfaceCreateInfoKHR surface_info = {0};
  surface_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
  surface_info.display = wayland_state->display;
  surface_info.surface = wayland_state->surface;

  if(vkCreateWaylandSurfaceKHR(ctx.instance, &surface_info, NULL, &ctx.surface) != VK_SUCCESS) {
    printf("vkCreateWaylandSurfaceKHR FAIL\n");
    return false;
  }
#endif

  printf("SUCCESS\n");
  return true;
}

b8 create_swapchain() {
  printf("Creating Swapchain ... ");

  VkSwapchainCreateInfoKHR swapchain_info = {0};
  swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.surface = ctx.surface;
  swapchain_info.minImageCount = MAX_FRAMES;
  swapchain_info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
  swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  swapchain_info.imageExtent.width = ctx.next_width;
  swapchain_info.imageExtent.height = ctx.next_height;
  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchain_info.queueFamilyIndexCount = 1;
  swapchain_info.pQueueFamilyIndices = &ctx.graphics_queue_index.familyIndex;
  swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  swapchain_info.clipped = VK_FALSE;

  if(vkCreateSwapchainKHR(ctx.device, &swapchain_info, NULL, &ctx.swapchain) != VK_SUCCESS) {
    printf("vkCreateSwapchainKHR FAIL\n");
    return false;
  }

  ctx.image_width = ctx.next_width;
  ctx.image_height = ctx.next_height;

  if(vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &ctx.swapchain_image_count, NULL) != VK_SUCCESS) {
    printf("vkGetSwapchainImagesKHR FAIL 1\n");
    return false;
  }
  ctx.swapchain_images = malloc(sizeof(VkImage) * ctx.swapchain_image_count);
  if(vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &ctx.swapchain_image_count, ctx.swapchain_images) != VK_SUCCESS) {
    printf("vkGetSwapchainImagesKHR FAIL 2\n");
    return false;
  }

  printf("Creating image views ... ");
  ctx.swapchain_image_views = malloc(sizeof(VkImageView) * ctx.swapchain_image_count);
  for (u32 i = 0; i < ctx.swapchain_image_count; i++)
  {
    VkImageViewCreateInfo view_info = {0};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = ctx.swapchain_images[i];
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_B8G8R8A8_SRGB;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    if(vkCreateImageView(ctx.device, &view_info, NULL, &ctx.swapchain_image_views[i]) != VK_SUCCESS) {
      printf("vkCreateImageView FAIL %u", i);
      return false;
    }
  }
  

  printf("SUCCESS\n");
  return true;
}

b8 create_render_pass() {
  printf("Creating Render Pass ... ");

  VkAttachmentDescription color_attachment = {0};
  color_attachment.format = VK_FORMAT_B8G8R8A8_SRGB;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref = {0};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {0};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkSubpassDependency dependency = {0};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info = {0};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  if (vkCreateRenderPass(ctx.device, &render_pass_info, NULL, &ctx.render_pass) != VK_SUCCESS) {
    printf("vkCreateRenderPass FAIL\n");
    return false;
  }

  printf("SUCCESS\n");
  return true;
}

b8 read_file(const char* filename, char** buffer, u32* length) {
  FILE* file = fopen(filename, "rb");
  if (!file) return false;

  fseek(file, 0, SEEK_END);
  *length = ftell(file);
  rewind(file);

  *buffer = malloc(*length);
  size_t read_size = fread(*buffer, 1, *length, file);
  fclose(file);

  return (read_size == *length);
}

VkShaderModule create_shader_module(const char* filename) {
  char* code = NULL;
  u32 length = 0;

  if (!read_file(filename, &code, &length)) {
    printf("Falha ao ler shader: %s\n", filename);
    return VK_NULL_HANDLE;
  }

  VkShaderModuleCreateInfo create_info = {0};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = length;
  create_info.pCode = (u32*)code;

  VkShaderModule module;
  if (vkCreateShaderModule(ctx.device, &create_info, NULL, &module) != VK_SUCCESS) {
    printf("Falha ao criar modulo de shader\n");
    return VK_NULL_HANDLE;
  }

  free(code);
  return module;
}

b8 create_graphics_pipeline() {
  printf("Creating graphics pipeline ... ");

  VkShaderModule fragment_shader = create_shader_module("shaders/basic.frag.spv");
  VkShaderModule vertex_shader = create_shader_module("shaders/basic.vert.spv");

  if(fragment_shader == VK_NULL_HANDLE || vertex_shader == VK_NULL_HANDLE) {
    printf("Creating shader module FAIL\n");
    return false;
  }

  VkGraphicsPipelineCreateInfo pipeline_info = {0};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;

  VkPipelineShaderStageCreateInfo vertex_info = {0};
  vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertex_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_info.module = vertex_shader;
  vertex_info.pName = "main";
  VkPipelineShaderStageCreateInfo fragment_info = {0};
  fragment_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragment_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragment_info.module = fragment_shader;
  fragment_info.pName = "main";

  VkPipelineShaderStageCreateInfo stages[] = {fragment_info, vertex_info};

  pipeline_info.pStages = stages;

  VkPipelineVertexInputStateCreateInfo vertex_input = {0};
  vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewport_state = {0};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterization_state = {0};
  rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization_state.depthClampEnable = VK_FALSE;
  rasterization_state.rasterizerDiscardEnable = VK_FALSE;
  rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization_state.lineWidth = 1.f;
  rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisample_info = {0};
  multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisample_info.sampleShadingEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blend = {0};
  color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.logicOpEnable = VK_FALSE;
  color_blend.attachmentCount = 1;

  VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
  color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_TRUE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  color_blend.pAttachments = &color_blend_attachment;

  VkPipelineDynamicStateCreateInfo dynamic_state = {0};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 2;
  VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
  dynamic_state.pDynamicStates = dynamic_states;

  VkPipelineLayoutCreateInfo pipeline_layout_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  if (vkCreatePipelineLayout(ctx.device, &pipeline_layout_info, 0, &ctx.pipeline_layout) != VK_SUCCESS)
  {
    printf("vkCreatePipelineLayout FAIL\n");
    return false;
  }

  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterization_state;
  pipeline_info.pMultisampleState = &multisample_info;
  pipeline_info.pColorBlendState = &color_blend;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = ctx.pipeline_layout;
  pipeline_info.renderPass = ctx.render_pass;

  if(vkCreateGraphicsPipelines(ctx.device, NULL, 1, &pipeline_info, NULL, &ctx.graphics_pipeline) != VK_SUCCESS) {
    printf("vkCreateGraphicsPipelines FAIL\n");
    return false;
  }

  printf("SUCCESS\n");
  return true;
}

b8 create_framebuffers() {
  printf("Creating Framebuffers... ");
  ctx.framebuffers = malloc(sizeof(VkFramebuffer) * ctx.swapchain_image_count);

  for (u32 i = 0; i < ctx.swapchain_image_count; i++) {
    VkImageView attachments[] = { ctx.swapchain_image_views[i] };

    VkFramebufferCreateInfo framebuffer_info = {0};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = ctx.render_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = ctx.next_width;
    framebuffer_info.height = ctx.next_height;
    framebuffer_info.layers = 1;

    if (vkCreateFramebuffer(ctx.device, &framebuffer_info, NULL, &ctx.framebuffers[i]) != VK_SUCCESS) {
      printf("vkCreateFramebuffer FAIL\n");
      return false;
    }
  }

  printf("SUCCESS\n");
  return true;
}

b8 create_sync_objects() {
  printf("Creating sync objects ... ");

  ctx.image_available_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES);
  ctx.render_finished_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES);
  ctx.in_flight_fences = malloc(sizeof(VkFence) * MAX_FRAMES);
  ctx.images_in_flight = malloc(sizeof(VkFence) * ctx.swapchain_image_count);

  VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (u32 i = 0; i < MAX_FRAMES; i++)
  {
    if (vkCreateSemaphore(ctx.device, &semaphore_info, NULL, &ctx.image_available_semaphores[i]) != VK_SUCCESS ||
      vkCreateSemaphore(ctx.device, &semaphore_info, NULL, &ctx.render_finished_semaphores[i]) != VK_SUCCESS ||
      vkCreateFence(ctx.device, &fence_info, NULL, &ctx.in_flight_fences[i]) != VK_SUCCESS) {
      printf("FAIL \n");
      return false;
    }
  }

  for (u32 i = 0; i < ctx.swapchain_image_count; i++)
  {
    if(vkCreateFence(ctx.device, &fence_info, NULL, &ctx.images_in_flight[i]) != VK_SUCCESS) {
      printf("FAIL \n");
      return false;
    }
  }

  printf("SUCCESS\n");
  return true;
}

b8 record_command_buffer() {
  VkCommandBufferBeginInfo command_begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

  vkBeginCommandBuffer(ctx.command_buffers[ctx.current_frame], &command_begin_info);

  VkRenderPassBeginInfo renderpass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderpass_info.renderPass = ctx.render_pass;
  renderpass_info.framebuffer = ctx.framebuffers[ctx.image_index];
  renderpass_info.renderArea.offset = (VkOffset2D){0, 0};
  renderpass_info.renderArea.extent.width = ctx.image_width;
  renderpass_info.renderArea.extent.height = ctx.image_height;

  VkClearValue clear_color = {{{0.0f, 0.0f, 0.1f, 1.0f}}};
  renderpass_info.clearValueCount = 1;
  renderpass_info.pClearValues = &clear_color;

  vkCmdBeginRenderPass(ctx.command_buffers[ctx.current_frame], &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(ctx.command_buffers[ctx.current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.graphics_pipeline);

  VkViewport viewport = {0};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (f32)ctx.image_width;
  viewport.height = (f32)ctx.image_height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(ctx.command_buffers[ctx.current_frame], 0, 1, &viewport);

  VkRect2D scissor = {0};
  scissor.offset = (VkOffset2D){0, 0};
  scissor.extent = (VkExtent2D){ctx.image_width, ctx.image_height};
  vkCmdSetScissor(ctx.command_buffers[ctx.current_frame], 0, 1, &scissor);
  vkCmdDraw(ctx.command_buffers[ctx.current_frame], 3, 1, 0, 0);

  vkCmdEndRenderPass(ctx.command_buffers[ctx.current_frame]);
  vkEndCommandBuffer(ctx.command_buffers[ctx.current_frame]);

  return true;
}

void handle_resize() {
  printf("Resizing ...");

  vkDeviceWaitIdle(ctx.device);
  vkDestroySwapchainKHR(ctx.device, ctx.swapchain, NULL);
  for (u32 i = 0; i < ctx.swapchain_image_count; i++)
  {
    vkDestroyImageView(ctx.device, ctx.swapchain_image_views[i], NULL);
    vkDestroyFramebuffer(ctx.device, ctx.framebuffers[i], NULL);
  }

  create_swapchain();
  create_framebuffers();
}

b8 frame() {
  vkWaitForFences(ctx.device, 1, &ctx.in_flight_fences[ctx.current_frame], VK_TRUE, UINT64_MAX);
  vkResetFences(ctx.device, 1, &ctx.in_flight_fences[ctx.current_frame]);

  if(ctx.next_width != ctx.image_width || ctx.next_height != ctx.image_height) {
    handle_resize();
  }

  VkResult result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX, ctx.image_available_semaphores[ctx.current_frame], 0, &ctx.image_index);
  if(result == VK_ERROR_OUT_OF_DATE_KHR) {
    printf("Swapchain out of date! Recriacao necessaria.\n");
    handle_resize();
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    printf("Falha ao adquirir imagem! Error code %i\n", result);
    return false; 
  }
  

  vkResetCommandBuffer(ctx.command_buffers[ctx.current_frame], 0);

  record_command_buffer();

  VkSubmitInfo submit_info = {0};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  VkSemaphore wait_semaphores[] = {ctx.image_available_semaphores[ctx.current_frame]};
  VkSemaphore signal_semaphores[] = {ctx.render_finished_semaphores[ctx.current_frame]};
  VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &ctx.command_buffers[ctx.current_frame];
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal_semaphores;

  if(vkQueueSubmit(ctx.graphics_queue, 1, &submit_info, ctx.in_flight_fences[ctx.current_frame]) != VK_SUCCESS) {
    printf("Submit fail\n");
    return false;
  }

  VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = signal_semaphores;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &ctx.swapchain;
  present_info.pImageIndices = &ctx.image_index;

  if(vkQueuePresentKHR(ctx.graphics_queue, &present_info) != VK_SUCCESS) {
    printf("Present FAIL\n");
    return false;
  }
  ctx.current_frame = (ctx.current_frame+1) % MAX_FRAMES;
  return true;
}

b8 resize_event(u16 code, void* sender, EventContext data) {
  printf("Event code resized received!");
  ctx.next_width = data.data.u32[0];
  ctx.next_height = data.data.u32[1];

  return false;
}

b8 quit_event(u16 code, void* sender, EventContext data) {
  running = false;
  return false;
}

b8 vk_init() {
  if(!create_instance()) {
    return false;
  }
  if(!setup_debug_messenger()) {
    return false;
  }
  if(!choose_physical_device()) {
    return false;
  }
  if(!create_logical_device()) {
    return false;
  }
  if(!allocate_command_buffers()) {
    return false;
  }
  if(!create_surface()) {
    return false;
  }
  if(!create_swapchain()) {
    return false;
  }
  if(!create_render_pass()) {
    return false;
  }
  if(!create_graphics_pipeline()) {
    return false;
  }
  if(!create_framebuffers()) {
    return false;
  }
  if(!create_sync_objects()) {
    return false;
  }


  return true;
}

void vk_cleanup() {
  printf("\nClean\n");
  vkDeviceWaitIdle(ctx.device);

  vkDestroySwapchainKHR(ctx.device, ctx.swapchain, NULL);
  vkDestroyCommandPool(ctx.device, ctx.command_pool, NULL);

  vkDestroyDevice(ctx.device, NULL);

  PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
    func(ctx.instance, ctx.debug_messenger, 0);

  vkDestroyInstance(ctx.instance, NULL);
}

int main() {
  event_initialize();
  event_register(EVENT_CODE_RESIZED, NULL, resize_event);
  event_register(EVENT_CODE_APPLICATION_QUIT, NULL, quit_event);
  platform_create_window("My app", 0, 0, 1280, 720, &window);
  platform_show_window(&window);

  ctx.next_width = 800;
  ctx.next_height = 600;

  if(vk_init()) {
    while (running) {
      platform_process_window_messages(&window);
      frame();
      fflush(stdout);
    }
  }

  vk_cleanup();
}