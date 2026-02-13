#include "stdio.h"
#include <vulkan/vulkan.h>
#include "defines.h"
#include <stdlib.h>

typedef struct QueueIndex {
  u32 familyIndex;
  u32 index;
} QueueIndex;

const i32 MAX_FRAMES = 3;

typedef struct VkContext {
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;

  VkPhysicalDevice physicalDevice;
  VkDevice device;

  QueueIndex graphics_queue_index;
  VkQueue graphics_queue;

  VkCommandPool command_pool;
  VkCommandBuffer *command_buffers; // MAX FRAMES
} VkContext;

VkContext ctx = {0};

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

void vk_init() {
  if(!create_instance()) {
    return;
  }
  if(!setup_debug_messenger()) {
    return;
  }
  if(!choose_physical_device()) {
    return;
  }
  if(!create_logical_device()) {
    return;
  }
  if(!allocate_command_buffers()) {
    return;
  }
}

void vk_cleanup() {
  vkDeviceWaitIdle(ctx.device);
  vkDestroyCommandPool(ctx.device, ctx.command_pool, NULL);

  vkDestroyDevice(ctx.device, NULL);

  PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
    func(ctx.instance, ctx.debug_messenger, 0);

  vkDestroyInstance(ctx.instance, NULL);
}

int main() {
  vk_init();
  vk_cleanup();
}