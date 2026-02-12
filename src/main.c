#include "stdio.h"
#include <vulkan/vulkan.h>
#include "defines.h"

typedef struct VkContext {
  VkInstance instance;
} VkContext;

VkContext ctx;

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

  if(vkCreateInstance(&instance_info, NULL, &ctx.instance) != VK_SUCCESS) {
    printf("FAIL\n");
    return false;
  }

  printf("SUCCESS\n");
  return true;
}

void vk_init() {
  if(!create_instance()) {
    return;
  }
}

int main() {
  vk_init();
}