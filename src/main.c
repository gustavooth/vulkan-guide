#include <stdio.h>
#include <stdlib.h>

#ifdef PLATFORM_WAYLAND
  #define VK_USE_PLATFORM_WAYLAND_KHR
  #include "platform/linux/platform_linux.h"
#endif
#include <vulkan/vulkan.h>
#include "defines.h"
#include "platform/platform.h"

typedef struct QueueIndex {
  u32 familyIndex;
  u32 index;
} QueueIndex;

const u32 MAX_FRAMES = 3;
const u32 WIDTH = 1280;
const u32 HEIGHT = 720;

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

  VkRenderPass render_pass;
} VkContext;

VkContext ctx = {0};
Window window;

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
  swapchain_info.imageExtent.width = WIDTH;
  swapchain_info.imageExtent.height = HEIGHT;
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

b8 frame() {
  vkResetCommandBuffer(ctx.command_buffers[0], 0);

  VkCommandBufferBeginInfo begin_info = {0};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if(vkBeginCommandBuffer(ctx.command_buffers[0], &begin_info) != VK_SUCCESS) {
    printf("vkBeginCommandBuffer FAIL\n");
    return false;
  }

  if(vkEndCommandBuffer(ctx.command_buffers[0]) != VK_SUCCESS) {
    printf("vkEndCommandBuffer FAIL\n");
    return false;
  }

  VkSubmitInfo submit_info = {0};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = ctx.command_buffers;

  if(vkQueueSubmit(ctx.graphics_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
    printf("vkQueueSubmit FAIL\n");
    return false;
  }

  vkQueueWaitIdle(ctx.graphics_queue);
  return true;
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

  return true;
}

void vk_cleanup() {
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
  platform_create_window("My app", 0, 0, 1280, 720, &window);
  platform_show_window(&window);

  if(vk_init()) {
    while (frame()) {}
  }

  vk_cleanup();
}