#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>
int main() {
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "Fable2 Vulkan Smoke";
  app.apiVersion = VK_API_VERSION_1_1;
  VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  create.pApplicationInfo = &app;
  VkInstance instance{};
  VkResult result = vkCreateInstance(&create, nullptr, &instance);
  if (result != VK_SUCCESS) { std::printf("vkCreateInstance failed: %d\n", result); return 1; }
  uint32_t count = 0;
  result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
  if (result != VK_SUCCESS || !count) { vkDestroyInstance(instance, nullptr); return 1; }
  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(instance, &count, devices.data());
  for (auto device : devices) {
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);
    std::printf("%s Vulkan %u.%u.%u geometryShader=%u fragmentStoresAndAtomics=%u shaderInt64=%u\n",
      properties.deviceName, VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion),
      VK_VERSION_PATCH(properties.apiVersion), features.geometryShader, features.fragmentStoresAndAtomics, features.shaderInt64);
  }
  vkDestroyInstance(instance, nullptr);
  return 0;
}
