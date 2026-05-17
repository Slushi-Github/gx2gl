#include <stdio.h>
#include <string.h>

#include <whb/gfx.h>
#include <whb/proc.h>

#include <vulkan/vulkan.h>

static void present_color(float r, float g, float b) {
    WHBGfxBeginRender();
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(r, g, b, 1.0f);
    WHBGfxFinishRenderTV();
    WHBGfxFinishRender();
}

static bool run_vulkan13_smoke(void) {
    PFN_vkCreateInstance pfn_create_instance = NULL;
    PFN_vkEnumerateInstanceVersion pfn_enumerate_instance_version = NULL;
    PFN_vkGetDeviceProcAddr pfn_get_device_proc_addr = NULL;
    PFN_vkCreateBuffer pfn_create_buffer = NULL;
    PFN_vkDestroyBuffer pfn_destroy_buffer = NULL;
    PFN_vkGetBufferMemoryRequirements pfn_get_buffer_memory_requirements = NULL;
    PFN_vkGetDeviceBufferMemoryRequirements pfn_get_device_buffer_memory_requirements = NULL;
    PFN_vkAllocateMemory pfn_allocate_memory = NULL;
    PFN_vkFreeMemory pfn_free_memory = NULL;
    PFN_vkBindBufferMemory pfn_bind_buffer_memory = NULL;
    PFN_vkMapMemory pfn_map_memory = NULL;
    PFN_vkUnmapMemory pfn_unmap_memory = NULL;
    PFN_vkFlushMappedMemoryRanges pfn_flush_mapped_memory_ranges = NULL;
    PFN_vkCmdPipelineBarrier2 pfn_cmd_pipeline_barrier2 = NULL;
    PFN_vkCmdBeginRendering pfn_cmd_begin_rendering = NULL;
    PFN_vkCmdEndRendering pfn_cmd_end_rendering = NULL;

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkResult result;
    uint32_t api_version = 0;
    uint32_t physical_device_count = 0;
    uint32_t *mapped_words = NULL;

    VkApplicationInfo app_info = {};
    VkInstanceCreateInfo instance_info = {};
    VkPhysicalDeviceVulkan13Features features13 = {};
    VkPhysicalDeviceFeatures2 features2 = {};
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {};
    VkDeviceCreateInfo device_info = {};
    VkDeviceBufferMemoryRequirements device_buffer_requirements = {};
    VkBufferCreateInfo buffer_info = {};
    VkMemoryAllocateInfo memory_allocate_info = {};
    VkMemoryRequirements2 memory_requirements = {};
    VkMemoryRequirements classic_memory_requirements = {};
    VkMappedMemoryRange mapped_range = {};
    VkCommandPoolCreateInfo command_pool_info = {};
    VkCommandBufferAllocateInfo command_buffer_info = {};
    VkCommandBufferBeginInfo command_buffer_begin = {};
    VkDependencyInfo dependency_info = {};
    VkRenderingInfo rendering_info = {};
    VkCommandBufferSubmitInfo command_submit_info = {};
    VkSubmitInfo2 submit_info = {};

    pfn_create_instance =
        (PFN_vkCreateInstance)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
    pfn_enumerate_instance_version =
        (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
    if (!pfn_create_instance || !pfn_enumerate_instance_version) {
        return false;
    }

    result = pfn_enumerate_instance_version(&api_version);
    if (result != VK_SUCCESS || VK_API_VERSION_MAJOR(api_version) != 1 || VK_API_VERSION_MINOR(api_version) < 3) {
        return false;
    }

    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "gx2vk smoke";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    app_info.pEngineName = "gx2vk";
    app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;

    result = pfn_create_instance(&instance_info, NULL, &instance);
    if (result != VK_SUCCESS || instance == VK_NULL_HANDLE) {
        return false;
    }

    result = vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL);
    if (result != VK_SUCCESS || physical_device_count != 1) {
        return false;
    }
    result = vkEnumeratePhysicalDevices(instance, &physical_device_count, &physical_device);
    if (result != VK_SUCCESS || physical_device == VK_NULL_HANDLE) {
        return false;
    }

    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features13;
    vkGetPhysicalDeviceFeatures2(physical_device, &features2);
    if (!features13.dynamicRendering || !features13.synchronization2 || !features13.maintenance4) {
        return false;
    }

    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    features13.maintenance4 = VK_TRUE;
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = 0;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = &features13;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;

    result = vkCreateDevice(physical_device, &device_info, NULL, &device);
    if (result != VK_SUCCESS || device == VK_NULL_HANDLE) {
        return false;
    }

    pfn_get_device_proc_addr =
        (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
    pfn_create_buffer =
        (PFN_vkCreateBuffer)vkGetDeviceProcAddr(device, "vkCreateBuffer");
    pfn_destroy_buffer =
        (PFN_vkDestroyBuffer)vkGetDeviceProcAddr(device, "vkDestroyBuffer");
    pfn_get_buffer_memory_requirements =
        (PFN_vkGetBufferMemoryRequirements)vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements");
    pfn_get_device_buffer_memory_requirements =
        (PFN_vkGetDeviceBufferMemoryRequirements)vkGetDeviceProcAddr(device, "vkGetDeviceBufferMemoryRequirements");
    pfn_allocate_memory =
        (PFN_vkAllocateMemory)vkGetDeviceProcAddr(device, "vkAllocateMemory");
    pfn_free_memory =
        (PFN_vkFreeMemory)vkGetDeviceProcAddr(device, "vkFreeMemory");
    pfn_bind_buffer_memory =
        (PFN_vkBindBufferMemory)vkGetDeviceProcAddr(device, "vkBindBufferMemory");
    pfn_map_memory =
        (PFN_vkMapMemory)vkGetDeviceProcAddr(device, "vkMapMemory");
    pfn_unmap_memory =
        (PFN_vkUnmapMemory)vkGetDeviceProcAddr(device, "vkUnmapMemory");
    pfn_flush_mapped_memory_ranges =
        (PFN_vkFlushMappedMemoryRanges)vkGetDeviceProcAddr(device, "vkFlushMappedMemoryRanges");
    pfn_cmd_pipeline_barrier2 =
        (PFN_vkCmdPipelineBarrier2)vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier2");
    pfn_cmd_begin_rendering =
        (PFN_vkCmdBeginRendering)vkGetDeviceProcAddr(device, "vkCmdBeginRendering");
    pfn_cmd_end_rendering =
        (PFN_vkCmdEndRendering)vkGetDeviceProcAddr(device, "vkCmdEndRendering");
    if (!pfn_get_device_proc_addr ||
        !pfn_create_buffer ||
        !pfn_destroy_buffer ||
        !pfn_get_buffer_memory_requirements ||
        !pfn_get_device_buffer_memory_requirements ||
        !pfn_allocate_memory ||
        !pfn_free_memory ||
        !pfn_bind_buffer_memory ||
        !pfn_map_memory ||
        !pfn_unmap_memory ||
        !pfn_flush_mapped_memory_ranges ||
        !pfn_cmd_pipeline_barrier2 ||
        !pfn_cmd_begin_rendering ||
        !pfn_cmd_end_rendering) {
        return false;
    }

    vkGetDeviceQueue(device, 0, 0, &queue);
    if (queue == VK_NULL_HANDLE) {
        return false;
    }

    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = 4096;
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    result = pfn_create_buffer(device, &buffer_info, NULL, &buffer);
    if (result != VK_SUCCESS || buffer == VK_NULL_HANDLE) {
        return false;
    }

    pfn_get_buffer_memory_requirements(device, buffer, &classic_memory_requirements);
    if (classic_memory_requirements.size < 4096 ||
        classic_memory_requirements.alignment < 256 ||
        classic_memory_requirements.memoryTypeBits == 0) {
        return false;
    }

    device_buffer_requirements.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS;
    device_buffer_requirements.pCreateInfo = &buffer_info;
    memory_requirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    pfn_get_device_buffer_memory_requirements(device, &device_buffer_requirements, &memory_requirements);
    if (memory_requirements.memoryRequirements.size < 4096 ||
        memory_requirements.memoryRequirements.alignment < 256 ||
        memory_requirements.memoryRequirements.memoryTypeBits == 0) {
        return false;
    }

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.allocationSize = memory_requirements.memoryRequirements.size;
    memory_allocate_info.memoryTypeIndex = 0;
    result = pfn_allocate_memory(device, &memory_allocate_info, NULL, &buffer_memory);
    if (result != VK_SUCCESS || buffer_memory == VK_NULL_HANDLE) {
        return false;
    }

    result = pfn_bind_buffer_memory(device, buffer, buffer_memory, 0);
    if (result != VK_SUCCESS) {
        return false;
    }

    result = pfn_map_memory(device, buffer_memory, 0, 16, 0, (void **)&mapped_words);
    if (result != VK_SUCCESS || !mapped_words) {
        return false;
    }
    mapped_words[0] = 0x3f800000u;
    mapped_words[1] = 0x40000000u;
    mapped_words[2] = 0x40400000u;
    mapped_words[3] = 0x40800000u;

    mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mapped_range.memory = buffer_memory;
    mapped_range.offset = 0;
    mapped_range.size = 16;
    result = pfn_flush_mapped_memory_ranges(device, 1, &mapped_range);
    if (result != VK_SUCCESS) {
        return false;
    }
    pfn_unmap_memory(device, buffer_memory);

    mapped_words = NULL;
    result = pfn_map_memory(device, buffer_memory, 0, 16, 0, (void **)&mapped_words);
    if (result != VK_SUCCESS || !mapped_words) {
        return false;
    }
    if (mapped_words[0] != 0x3f800000u ||
        mapped_words[1] != 0x40000000u ||
        mapped_words[2] != 0x40400000u ||
        mapped_words[3] != 0x40800000u) {
        return false;
    }
    pfn_unmap_memory(device, buffer_memory);

    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.queueFamilyIndex = 0;
    result = vkCreateCommandPool(device, &command_pool_info, NULL, &command_pool);
    if (result != VK_SUCCESS || command_pool == VK_NULL_HANDLE) {
        return false;
    }

    command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_info.commandPool = command_pool;
    command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_info.commandBufferCount = 1;
    result = vkAllocateCommandBuffers(device, &command_buffer_info, &command_buffer);
    if (result != VK_SUCCESS || command_buffer == VK_NULL_HANDLE) {
        return false;
    }

    command_buffer_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    result = vkBeginCommandBuffer(command_buffer, &command_buffer_begin);
    if (result != VK_SUCCESS) {
        return false;
    }

    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    pfn_cmd_pipeline_barrier2(command_buffer, &dependency_info);

    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.extent.width = 640;
    rendering_info.renderArea.extent.height = 480;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 0;
    pfn_cmd_begin_rendering(command_buffer, &rendering_info);
    pfn_cmd_end_rendering(command_buffer);

    result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        return false;
    }

    command_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_submit_info.commandBuffer = command_buffer;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_submit_info;
    result = vkQueueSubmit2(queue, 1, &submit_info, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        return false;
    }

    result = vkQueueWaitIdle(queue);
    if (result != VK_SUCCESS) {
        return false;
    }

    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    vkDestroyCommandPool(device, command_pool, NULL);
    pfn_destroy_buffer(device, buffer, NULL);
    pfn_free_memory(device, buffer_memory, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    return true;
}

int main(int argc, char **argv) {
    bool ok = false;
    (void)argc;
    (void)argv;

    WHBProcInit();
    WHBGfxInit();

    ok = run_vulkan13_smoke();
    present_color(ok ? 0.0f : 0.8f, ok ? 0.8f : 0.0f, 0.0f);

    while (WHBProcIsRunning()) {
    }

    WHBGfxShutdown();
    WHBProcShutdown();
    return ok ? 0 : 1;
}
