#include "vk/gx2vk_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

gx2vk_physical_device_t g_gx2vk_physical_device = {};

static const VkExtensionProperties g_gx2vk_instance_extensions[] = {
    {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
     VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_SPEC_VERSION},
};

static const VkExtensionProperties g_gx2vk_device_extensions[] = {
    {VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_SPEC_VERSION},
    {VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_SPEC_VERSION},
    {VK_KHR_MAINTENANCE_4_EXTENSION_NAME, VK_KHR_MAINTENANCE_4_SPEC_VERSION},
};

static bool gx2vk_supports_extension(const VkExtensionProperties *extensions,
                                     uint32_t extension_count,
                                     const char *name) {
    uint32_t i;
    for (i = 0; i < extension_count; ++i) {
        if (strcmp(name, extensions[i].extensionName) == 0) {
            return true;
        }
    }
    return false;
}

static VkResult gx2vk_enumerate_extensions(const VkExtensionProperties *properties,
                                           uint32_t property_count,
                                           uint32_t *p_property_count,
                                           VkExtensionProperties *p_properties) {
    uint32_t write_count;

    if (!p_property_count) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!p_properties) {
        *p_property_count = property_count;
        return VK_SUCCESS;
    }

    write_count = (*p_property_count < property_count) ? *p_property_count : property_count;
    if (write_count > 0) {
        memcpy(p_properties, properties, write_count * sizeof(VkExtensionProperties));
    }

    *p_property_count = property_count;
    return (write_count < property_count) ? VK_INCOMPLETE : VK_SUCCESS;
}

static void gx2vk_fill_physical_device_properties(VkPhysicalDeviceProperties *properties) {
    memset(properties, 0, sizeof(*properties));
    properties->apiVersion = VK_API_VERSION_1_3;
    properties->driverVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    properties->vendorID = 0x057E;
    properties->deviceID = 0x0001;
    properties->deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    strncpy(properties->deviceName, "gx2vk (Nintendo Wii U GX2)", VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);

    properties->limits.maxImageDimension1D = 4096;
    properties->limits.maxImageDimension2D = 4096;
    properties->limits.maxImageDimension3D = 2048;
    properties->limits.maxImageArrayLayers = 2048;
    properties->limits.maxTexelBufferElements = 1 << 20;
    properties->limits.maxUniformBufferRange = 64 * 1024;
    properties->limits.maxStorageBufferRange = 128 * 1024 * 1024;
    properties->limits.maxPushConstantsSize = 128;
    properties->limits.maxMemoryAllocationCount = 4096;
    properties->limits.maxSamplerAllocationCount = 4096;
    properties->limits.bufferImageGranularity = 256;
    properties->limits.maxBoundDescriptorSets = 4;
    properties->limits.maxPerStageDescriptorSamplers = 16;
    properties->limits.maxPerStageDescriptorUniformBuffers = 12;
    properties->limits.maxPerStageDescriptorStorageBuffers = 4;
    properties->limits.maxPerStageDescriptorSampledImages = 16;
    properties->limits.maxPerStageResources = 32;
    properties->limits.maxDescriptorSetSamplers = 32;
    properties->limits.maxDescriptorSetUniformBuffers = 16;
    properties->limits.maxDescriptorSetStorageBuffers = 8;
    properties->limits.maxDescriptorSetSampledImages = 32;
    properties->limits.maxVertexInputAttributes = 16;
    properties->limits.maxVertexInputBindings = 16;
    properties->limits.maxVertexInputAttributeOffset = 2047;
    properties->limits.maxVertexInputBindingStride = 2048;
    properties->limits.maxVertexOutputComponents = 64;
    properties->limits.maxFragmentInputComponents = 64;
    properties->limits.maxFragmentOutputAttachments = 8;
    properties->limits.maxFragmentCombinedOutputResources = 8;
    properties->limits.maxComputeSharedMemorySize = 0;
    properties->limits.maxComputeWorkGroupCount[0] = 1;
    properties->limits.maxComputeWorkGroupCount[1] = 1;
    properties->limits.maxComputeWorkGroupCount[2] = 1;
    properties->limits.maxComputeWorkGroupInvocations = 1;
    properties->limits.maxFramebufferWidth = 1920;
    properties->limits.maxFramebufferHeight = 1080;
    properties->limits.maxFramebufferLayers = 1;
    properties->limits.framebufferColorSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;
    properties->limits.framebufferDepthSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;
    properties->limits.framebufferStencilSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;
    properties->limits.framebufferNoAttachmentsSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;
    properties->limits.maxColorAttachments = 8;
    properties->limits.sampledImageColorSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;
    properties->limits.sampledImageDepthSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;
    properties->limits.sampledImageIntegerSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    properties->limits.storageImageSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    properties->limits.timestampComputeAndGraphics = VK_FALSE;
    properties->limits.timestampPeriod = 1.0f;
    properties->limits.maxClipDistances = 8;
    properties->limits.maxCullDistances = 8;
    properties->limits.maxCombinedClipAndCullDistances = 8;
    properties->limits.discreteQueuePriorities = 2;
    properties->limits.pointSizeRange[0] = 1.0f;
    properties->limits.pointSizeRange[1] = 64.0f;
    properties->limits.lineWidthRange[0] = 1.0f;
    properties->limits.lineWidthRange[1] = 8.0f;
    properties->limits.pointSizeGranularity = 1.0f;
    properties->limits.lineWidthGranularity = 1.0f;
    properties->limits.strictLines = VK_FALSE;
    properties->limits.standardSampleLocations = VK_TRUE;
    properties->limits.optimalBufferCopyOffsetAlignment = 256;
    properties->limits.optimalBufferCopyRowPitchAlignment = 64;
    properties->limits.nonCoherentAtomSize = 64;
    properties->limits.minUniformBufferOffsetAlignment = 256;
    properties->limits.minStorageBufferOffsetAlignment = 256;
}

static void gx2vk_fill_physical_device_features(VkPhysicalDeviceFeatures *features) {
    memset(features, 0, sizeof(*features));
    features->fullDrawIndexUint32 = VK_TRUE;
    features->independentBlend = VK_TRUE;
    features->geometryShader = VK_FALSE;
    features->tessellationShader = VK_FALSE;
    features->sampleRateShading = VK_TRUE;
    features->dualSrcBlend = VK_FALSE;
    features->logicOp = VK_TRUE;
    features->multiDrawIndirect = VK_TRUE;
    features->drawIndirectFirstInstance = VK_TRUE;
    features->depthClamp = VK_TRUE;
    features->depthBiasClamp = VK_TRUE;
    features->fillModeNonSolid = VK_TRUE;
    features->depthBounds = VK_FALSE;
    features->wideLines = VK_TRUE;
    features->largePoints = VK_TRUE;
    features->alphaToOne = VK_TRUE;
    features->multiViewport = VK_FALSE;
    features->samplerAnisotropy = VK_TRUE;
    features->textureCompressionBC = VK_TRUE;
    features->occlusionQueryPrecise = VK_TRUE;
    features->pipelineStatisticsQuery = VK_FALSE;
    features->vertexPipelineStoresAndAtomics = VK_FALSE;
    features->fragmentStoresAndAtomics = VK_FALSE;
    features->shaderClipDistance = VK_TRUE;
    features->shaderCullDistance = VK_TRUE;
    features->shaderFloat64 = VK_FALSE;
    features->shaderInt64 = VK_FALSE;
    features->shaderInt16 = VK_FALSE;
}

static void gx2vk_fill_vulkan13_features(VkPhysicalDeviceVulkan13Features *features13) {
    memset(features13, 0, sizeof(*features13));
    features13->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13->dynamicRendering = VK_TRUE;
    features13->synchronization2 = VK_TRUE;
    features13->maintenance4 = VK_TRUE;
}

static void gx2vk_fill_memory_properties(VkPhysicalDeviceMemoryProperties *properties) {
    memset(properties, 0, sizeof(*properties));
    properties->memoryHeapCount = 1;
    properties->memoryHeaps[0].size = 320ull * 1024ull * 1024ull;
    properties->memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    properties->memoryTypeCount = 1;
    properties->memoryTypes[0].propertyFlags =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    properties->memoryTypes[0].heapIndex = 0;
}

static void gx2vk_fill_queue_family(VkQueueFamilyProperties *properties) {
    memset(properties, 0, sizeof(*properties));
    properties->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;
    properties->queueCount = 1;
    properties->timestampValidBits = 0;
    properties->minImageTransferGranularity.width = 1;
    properties->minImageTransferGranularity.height = 1;
    properties->minImageTransferGranularity.depth = 1;
}

static void gx2vk_copy_features_chain(void *p_next) {
    VkBaseOutStructure *base = (VkBaseOutStructure *)p_next;

    while (base) {
        switch (base->sType) {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES: {
                VkPhysicalDeviceVulkan13Features *features13 = (VkPhysicalDeviceVulkan13Features *)base;
                gx2vk_fill_vulkan13_features(features13);
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES: {
                VkPhysicalDeviceDynamicRenderingFeatures *features =
                    (VkPhysicalDeviceDynamicRenderingFeatures *)base;
                features->dynamicRendering = VK_TRUE;
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES: {
                VkPhysicalDeviceSynchronization2Features *features =
                    (VkPhysicalDeviceSynchronization2Features *)base;
                features->synchronization2 = VK_TRUE;
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES: {
                VkPhysicalDeviceMaintenance4Features *features =
                    (VkPhysicalDeviceMaintenance4Features *)base;
                features->maintenance4 = VK_TRUE;
                break;
            }
            default:
                break;
        }
        base = (VkBaseOutStructure *)base->pNext;
    }
}

static bool gx2vk_validate_requested_base_features(const VkPhysicalDeviceFeatures *requested_features) {
    VkPhysicalDeviceFeatures supported_features;
    const VkBool32 *requested_values = (const VkBool32 *)requested_features;
    const VkBool32 *supported_values;
    size_t count = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
    size_t i;

    if (!requested_features) {
        return true;
    }

    gx2vk_fill_physical_device_features(&supported_features);
    supported_values = (const VkBool32 *)&supported_features;
    for (i = 0; i < count; ++i) {
        if (requested_values[i] == VK_TRUE && supported_values[i] != VK_TRUE) {
            return false;
        }
    }

    return true;
}

static bool gx2vk_validate_requested_features(const VkDeviceCreateInfo *create_info,
                                              bool *dynamic_rendering,
                                              bool *synchronization2,
                                              bool *maintenance4) {
    const VkBaseInStructure *base = (const VkBaseInStructure *)create_info->pNext;

    *dynamic_rendering = false;
    *synchronization2 = false;
    *maintenance4 = false;

    if (!gx2vk_validate_requested_base_features(create_info->pEnabledFeatures)) {
        return false;
    }

    while (base) {
        switch (base->sType) {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES: {
                const VkPhysicalDeviceVulkan13Features *features13 =
                    (const VkPhysicalDeviceVulkan13Features *)base;
                if (features13->robustImageAccess ||
                    features13->inlineUniformBlock ||
                    features13->descriptorBindingInlineUniformBlockUpdateAfterBind ||
                    features13->pipelineCreationCacheControl ||
                    features13->privateData ||
                    features13->shaderDemoteToHelperInvocation ||
                    features13->shaderTerminateInvocation ||
                    features13->subgroupSizeControl ||
                    features13->computeFullSubgroups ||
                    features13->textureCompressionASTC_HDR ||
                    features13->shaderZeroInitializeWorkgroupMemory ||
                    features13->shaderIntegerDotProduct) {
                    return false;
                }
                *dynamic_rendering = features13->dynamicRendering == VK_TRUE;
                *synchronization2 = features13->synchronization2 == VK_TRUE;
                *maintenance4 = features13->maintenance4 == VK_TRUE;
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES: {
                const VkPhysicalDeviceDynamicRenderingFeatures *features =
                    (const VkPhysicalDeviceDynamicRenderingFeatures *)base;
                *dynamic_rendering = features->dynamicRendering == VK_TRUE;
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES: {
                const VkPhysicalDeviceSynchronization2Features *features =
                    (const VkPhysicalDeviceSynchronization2Features *)base;
                *synchronization2 = features->synchronization2 == VK_TRUE;
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES: {
                const VkPhysicalDeviceMaintenance4Features *features =
                    (const VkPhysicalDeviceMaintenance4Features *)base;
                *maintenance4 = features->maintenance4 == VK_TRUE;
                break;
            }
            default:
                break;
        }
        base = base->pNext;
    }

    return true;
}

extern "C" {
VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(VkDevice device,
                                              const VkBufferCreateInfo *pCreateInfo,
                                              const VkAllocationCallbacks *pAllocator,
                                              VkBuffer *pBuffer);
VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(VkDevice device,
                                           VkBuffer buffer,
                                           const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements(VkDevice device,
                                                         VkBuffer buffer,
                                                         VkMemoryRequirements *pMemoryRequirements);
VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements2(VkDevice device,
                                                          const VkBufferMemoryRequirementsInfo2 *pInfo,
                                                          VkMemoryRequirements2 *pMemoryRequirements);
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(VkDevice device,
                                                const VkMemoryAllocateInfo *pAllocateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkDeviceMemory *pMemory);
VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice device,
                                        VkDeviceMemory memory,
                                        const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(VkDevice device,
                                                  VkBuffer buffer,
                                                  VkDeviceMemory memory,
                                                  VkDeviceSize memoryOffset);
VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory2(VkDevice device,
                                                   uint32_t bindInfoCount,
                                                   const VkBindBufferMemoryInfo *pBindInfos);
VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(VkDevice device,
                                           VkDeviceMemory memory,
                                           VkDeviceSize offset,
                                           VkDeviceSize size,
                                           VkMemoryMapFlags flags,
                                           void **ppData);
VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(VkDevice device,
                                         VkDeviceMemory memory);
VKAPI_ATTR VkResult VKAPI_CALL vkFlushMappedMemoryRanges(VkDevice device,
                                                         uint32_t memoryRangeCount,
                                                         const VkMappedMemoryRange *pMemoryRanges);
VKAPI_ATTR VkResult VKAPI_CALL vkInvalidateMappedMemoryRanges(VkDevice device,
                                                              uint32_t memoryRangeCount,
                                                              const VkMappedMemoryRange *pMemoryRanges);
VKAPI_ATTR void VKAPI_CALL vkGetDeviceBufferMemoryRequirements(VkDevice device,
                                                               const VkDeviceBufferMemoryRequirements *pInfo,
                                                               VkMemoryRequirements2 *pMemoryRequirements);
VKAPI_ATTR void VKAPI_CALL vkGetDeviceImageMemoryRequirements(VkDevice device,
                                                              const VkDeviceImageMemoryRequirements *pInfo,
                                                              VkMemoryRequirements2 *pMemoryRequirements);
VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice device,
                                                   const VkCommandPoolCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkCommandPool *pCommandPool);
VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice device,
                                                VkCommandPool commandPool,
                                                const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device,
                                                        const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                        VkCommandBuffer *pCommandBuffers);
VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(VkDevice device,
                                                VkCommandPool commandPool,
                                                uint32_t commandBufferCount,
                                                const VkCommandBuffer *pCommandBuffers);
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                    const VkCommandBufferBeginInfo *pBeginInfo);
VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer commandBuffer);
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer,
                                                 const VkDependencyInfo *pDependencyInfo);
VKAPI_ATTR void VKAPI_CALL vkCmdBeginRendering(VkCommandBuffer commandBuffer,
                                               const VkRenderingInfo *pRenderingInfo);
VKAPI_ATTR void VKAPI_CALL vkCmdEndRendering(VkCommandBuffer commandBuffer);
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                             VkPipelineBindPoint pipelineBindPoint,
                                             VkPipeline pipeline);
VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer,
                                                   VkPipelineBindPoint pipelineBindPoint,
                                                   VkPipelineLayout layout,
                                                   uint32_t firstSet,
                                                   uint32_t descriptorSetCount,
                                                   const VkDescriptorSet *pDescriptorSets,
                                                   uint32_t dynamicOffsetCount,
                                                   const uint32_t *pDynamicOffsets);
VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer,
                                                  uint32_t firstBinding,
                                                  uint32_t bindingCount,
                                                  const VkBuffer *pBuffers,
                                                  const VkDeviceSize *pOffsets);
VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer,
                                                VkBuffer buffer,
                                                VkDeviceSize offset,
                                                VkIndexType indexType);
VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer commandBuffer,
                                            uint32_t firstViewport,
                                            uint32_t viewportCount,
                                            const VkViewport *pViewports);
VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer commandBuffer,
                                           uint32_t firstScissor,
                                           uint32_t scissorCount,
                                           const VkRect2D *pScissors);
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue,
                                              uint32_t submitCount,
                                              const VkSubmitInfo2 *pSubmits,
                                              VkFence fence);
VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue queue);
VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device);
VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device,
                                                    const VkShaderModuleCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkShaderModule *pShaderModule);
VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice device,
                                                 VkShaderModule shaderModule,
                                                 const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice device,
                                                           const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                           const VkAllocationCallbacks *pAllocator,
                                                           VkDescriptorSetLayout *pSetLayout);
VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device,
                                                        VkDescriptorSetLayout descriptorSetLayout,
                                                        const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice device,
                                                      const VkDescriptorPoolCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator,
                                                      VkDescriptorPool *pDescriptorPool);
VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(VkDevice device,
                                                   VkDescriptorPool descriptorPool,
                                                   const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice device,
                                                        const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                        VkDescriptorSet *pDescriptorSets);
VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice device,
                                                    VkDescriptorPool descriptorPool,
                                                    uint32_t descriptorSetCount,
                                                    const VkDescriptorSet *pDescriptorSets);
VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(VkDevice device,
                                                  uint32_t descriptorWriteCount,
                                                  const VkWriteDescriptorSet *pDescriptorWrites,
                                                  uint32_t descriptorCopyCount,
                                                  const VkCopyDescriptorSet *pDescriptorCopies);
VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device,
                                                      const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator,
                                                      VkPipelineLayout *pPipelineLayout);
VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(VkDevice device,
                                                   VkPipelineLayout pipelineLayout,
                                                   const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice device,
                                                         VkPipelineCache pipelineCache,
                                                         uint32_t createInfoCount,
                                                         const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                                         const VkAllocationCallbacks *pAllocator,
                                                         VkPipeline *pPipelines);
VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice device,
                                             VkPipeline pipeline,
                                             const VkAllocationCallbacks *pAllocator);
}

extern "C" {

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName);

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceVersion(uint32_t *pApiVersion) {
    if (!pApiVersion) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pApiVersion = VK_API_VERSION_1_3;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkInstance *pInstance) {
    gx2vk_instance_t *instance = NULL;
    uint32_t i;

    (void)pAllocator;

    if (!pCreateInfo || !pInstance || pCreateInfo->sType != VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (pCreateInfo->enabledLayerCount > 0) {
        return VK_ERROR_LAYER_NOT_PRESENT;
    }
    for (i = 0; i < pCreateInfo->enabledExtensionCount; ++i) {
        if (!gx2vk_supports_extension(g_gx2vk_instance_extensions,
                                      (uint32_t)(sizeof(g_gx2vk_instance_extensions) / sizeof(g_gx2vk_instance_extensions[0])),
                                      pCreateInfo->ppEnabledExtensionNames[i])) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
    if (pCreateInfo->pApplicationInfo &&
        pCreateInfo->pApplicationInfo->apiVersion > VK_API_VERSION_1_3) {
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }

    instance = (gx2vk_instance_t *)calloc(1, sizeof(*instance));
    if (!instance) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    instance->api_version = (pCreateInfo->pApplicationInfo && pCreateInfo->pApplicationInfo->apiVersion != 0)
        ? pCreateInfo->pApplicationInfo->apiVersion
        : VK_API_VERSION_1_3;

    *pInstance = (VkInstance)instance;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance instance,
                                             const VkAllocationCallbacks *pAllocator) {
    (void)pAllocator;
    free((void *)instance);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(const char *pLayerName,
                                                                      uint32_t *pPropertyCount,
                                                                      VkExtensionProperties *pProperties) {
    (void)pLayerName;
    return gx2vk_enumerate_extensions(
        g_gx2vk_instance_extensions,
        (uint32_t)(sizeof(g_gx2vk_instance_extensions) / sizeof(g_gx2vk_instance_extensions[0])),
        pPropertyCount,
        pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t *pPropertyCount,
                                                                  VkLayerProperties *pProperties) {
    if (!pPropertyCount) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!pProperties) {
        *pPropertyCount = 0;
        return VK_SUCCESS;
    }
    *pPropertyCount = 0;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                                                    const char *pLayerName,
                                                                    uint32_t *pPropertyCount,
                                                                    VkExtensionProperties *pProperties) {
    (void)physicalDevice;
    (void)pLayerName;
    return gx2vk_enumerate_extensions(
        g_gx2vk_device_extensions,
        (uint32_t)(sizeof(g_gx2vk_device_extensions) / sizeof(g_gx2vk_device_extensions[0])),
        pPropertyCount,
        pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice,
                                                                uint32_t *pPropertyCount,
                                                                VkLayerProperties *pProperties) {
    (void)physicalDevice;
    if (!pPropertyCount) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!pProperties) {
        *pPropertyCount = 0;
        return VK_SUCCESS;
    }
    *pPropertyCount = 0;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance instance,
                                                          uint32_t *pPhysicalDeviceCount,
                                                          VkPhysicalDevice *pPhysicalDevices) {
    if (!instance || !pPhysicalDeviceCount) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    g_gx2vk_physical_device.instance = (gx2vk_instance_t *)instance;

    if (!pPhysicalDevices) {
        *pPhysicalDeviceCount = 1;
        return VK_SUCCESS;
    }
    if (*pPhysicalDeviceCount == 0) {
        return VK_INCOMPLETE;
    }

    pPhysicalDevices[0] = (VkPhysicalDevice)&g_gx2vk_physical_device;
    *pPhysicalDeviceCount = 1;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                                                         VkPhysicalDeviceProperties *pProperties) {
    (void)physicalDevice;
    if (pProperties) {
        gx2vk_fill_physical_device_properties(pProperties);
    }
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                                          VkPhysicalDeviceProperties2 *pProperties) {
    (void)physicalDevice;
    if (!pProperties) {
        return;
    }
    pProperties->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    gx2vk_fill_physical_device_properties(&pProperties->properties);
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice,
                                                       VkPhysicalDeviceFeatures *pFeatures) {
    (void)physicalDevice;
    if (pFeatures) {
        gx2vk_fill_physical_device_features(pFeatures);
    }
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice,
                                                        VkPhysicalDeviceFeatures2 *pFeatures) {
    (void)physicalDevice;
    if (!pFeatures) {
        return;
    }

    pFeatures->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    gx2vk_fill_physical_device_features(&pFeatures->features);
    gx2vk_copy_features_chain((void *)pFeatures->pNext);
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice,
                                                                    uint32_t *pQueueFamilyPropertyCount,
                                                                    VkQueueFamilyProperties *pQueueFamilyProperties) {
    VkQueueFamilyProperties properties;
    (void)physicalDevice;

    if (!pQueueFamilyPropertyCount) {
        return;
    }
    if (!pQueueFamilyProperties) {
        *pQueueFamilyPropertyCount = 1;
        return;
    }

    gx2vk_fill_queue_family(&properties);
    pQueueFamilyProperties[0] = properties;
    *pQueueFamilyPropertyCount = 1;
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice,
                                                                     uint32_t *pQueueFamilyPropertyCount,
                                                                     VkQueueFamilyProperties2 *pQueueFamilyProperties) {
    VkQueueFamilyProperties properties;
    (void)physicalDevice;

    if (!pQueueFamilyPropertyCount) {
        return;
    }
    if (!pQueueFamilyProperties) {
        *pQueueFamilyPropertyCount = 1;
        return;
    }

    gx2vk_fill_queue_family(&properties);
    pQueueFamilyProperties[0].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    pQueueFamilyProperties[0].queueFamilyProperties = properties;
    *pQueueFamilyPropertyCount = 1;
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                                               VkPhysicalDeviceMemoryProperties *pMemoryProperties) {
    (void)physicalDevice;
    if (pMemoryProperties) {
        gx2vk_fill_memory_properties(pMemoryProperties);
    }
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                                                VkPhysicalDeviceMemoryProperties2 *pMemoryProperties) {
    (void)physicalDevice;
    if (!pMemoryProperties) {
        return;
    }
    pMemoryProperties->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    gx2vk_fill_memory_properties(&pMemoryProperties->memoryProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice,
                                              const VkDeviceCreateInfo *pCreateInfo,
                                              const VkAllocationCallbacks *pAllocator,
                                              VkDevice *pDevice) {
    gx2vk_device_t *device = NULL;
    bool dynamic_rendering = false;
    bool synchronization2 = false;
    bool maintenance4 = false;
    uint32_t i;

    (void)pAllocator;

    if (!physicalDevice || !pCreateInfo || !pDevice || pCreateInfo->sType != VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (pCreateInfo->queueCreateInfoCount == 0 ||
        !pCreateInfo->pQueueCreateInfos ||
        pCreateInfo->pQueueCreateInfos[0].queueFamilyIndex != 0 ||
        pCreateInfo->pQueueCreateInfos[0].queueCount == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    for (i = 0; i < pCreateInfo->enabledExtensionCount; ++i) {
        if (!gx2vk_supports_extension(g_gx2vk_device_extensions,
                                      (uint32_t)(sizeof(g_gx2vk_device_extensions) / sizeof(g_gx2vk_device_extensions[0])),
                                      pCreateInfo->ppEnabledExtensionNames[i])) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    if (!gx2vk_validate_requested_features(pCreateInfo,
                                           &dynamic_rendering,
                                           &synchronization2,
                                           &maintenance4)) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    device = (gx2vk_device_t *)calloc(1, sizeof(*device));
    if (!device) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    device->physical_device = (gx2vk_physical_device_t *)physicalDevice;
    device->dynamic_rendering_enabled = dynamic_rendering;
    device->synchronization2_enabled = synchronization2;
    device->maintenance4_enabled = maintenance4;
    device->graphics_queue.device = device;
    device->graphics_queue.family_index = 0;
    device->graphics_queue.queue_index = 0;
    *pDevice = (VkDevice)device;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device,
                                           const VkAllocationCallbacks *pAllocator) {
    (void)pAllocator;
    free((void *)device);
}

VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(VkDevice device,
                                            uint32_t queueFamilyIndex,
                                            uint32_t queueIndex,
                                            VkQueue *pQueue) {
    gx2vk_device_t *gx2vk_device = (gx2vk_device_t *)device;
    if (!gx2vk_device || !pQueue || queueFamilyIndex != 0 || queueIndex != 0) {
        return;
    }
    *pQueue = (VkQueue)&gx2vk_device->graphics_queue;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName) {
    (void)device;
    if (!pName) {
        return NULL;
    }

#define GX2VK_DEVICE_PROC(name) \
    if (strcmp(pName, #name) == 0) { \
        return reinterpret_cast<PFN_vkVoidFunction>(name); \
    }

    GX2VK_DEVICE_PROC(vkDestroyDevice)
    GX2VK_DEVICE_PROC(vkGetDeviceQueue)
    GX2VK_DEVICE_PROC(vkCreateBuffer)
    GX2VK_DEVICE_PROC(vkDestroyBuffer)
    GX2VK_DEVICE_PROC(vkGetBufferMemoryRequirements)
    GX2VK_DEVICE_PROC(vkGetBufferMemoryRequirements2)
    GX2VK_DEVICE_PROC(vkAllocateMemory)
    GX2VK_DEVICE_PROC(vkFreeMemory)
    GX2VK_DEVICE_PROC(vkBindBufferMemory)
    GX2VK_DEVICE_PROC(vkBindBufferMemory2)
    GX2VK_DEVICE_PROC(vkMapMemory)
    GX2VK_DEVICE_PROC(vkUnmapMemory)
    GX2VK_DEVICE_PROC(vkFlushMappedMemoryRanges)
    GX2VK_DEVICE_PROC(vkInvalidateMappedMemoryRanges)
    GX2VK_DEVICE_PROC(vkGetDeviceBufferMemoryRequirements)
    GX2VK_DEVICE_PROC(vkGetDeviceImageMemoryRequirements)
    GX2VK_DEVICE_PROC(vkCreateCommandPool)
    GX2VK_DEVICE_PROC(vkDestroyCommandPool)
    GX2VK_DEVICE_PROC(vkAllocateCommandBuffers)
    GX2VK_DEVICE_PROC(vkFreeCommandBuffers)
    GX2VK_DEVICE_PROC(vkBeginCommandBuffer)
    GX2VK_DEVICE_PROC(vkEndCommandBuffer)
    GX2VK_DEVICE_PROC(vkCmdPipelineBarrier2)
    GX2VK_DEVICE_PROC(vkCmdBeginRendering)
    GX2VK_DEVICE_PROC(vkCmdEndRendering)
    GX2VK_DEVICE_PROC(vkCmdBindPipeline)
    GX2VK_DEVICE_PROC(vkCmdBindDescriptorSets)
    GX2VK_DEVICE_PROC(vkCmdBindVertexBuffers)
    GX2VK_DEVICE_PROC(vkCmdBindIndexBuffer)
    GX2VK_DEVICE_PROC(vkCmdSetViewport)
    GX2VK_DEVICE_PROC(vkCmdSetScissor)
    GX2VK_DEVICE_PROC(vkQueueSubmit2)
    GX2VK_DEVICE_PROC(vkQueueWaitIdle)
    GX2VK_DEVICE_PROC(vkDeviceWaitIdle)
    GX2VK_DEVICE_PROC(vkCreateShaderModule)
    GX2VK_DEVICE_PROC(vkDestroyShaderModule)
    GX2VK_DEVICE_PROC(vkCreateDescriptorSetLayout)
    GX2VK_DEVICE_PROC(vkDestroyDescriptorSetLayout)
    GX2VK_DEVICE_PROC(vkCreateDescriptorPool)
    GX2VK_DEVICE_PROC(vkDestroyDescriptorPool)
    GX2VK_DEVICE_PROC(vkAllocateDescriptorSets)
    GX2VK_DEVICE_PROC(vkFreeDescriptorSets)
    GX2VK_DEVICE_PROC(vkUpdateDescriptorSets)
    GX2VK_DEVICE_PROC(vkCreatePipelineLayout)
    GX2VK_DEVICE_PROC(vkDestroyPipelineLayout)
    GX2VK_DEVICE_PROC(vkCreateGraphicsPipelines)
    GX2VK_DEVICE_PROC(vkDestroyPipeline)

#undef GX2VK_DEVICE_PROC

    return NULL;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName) {
    (void)instance;
    if (!pName) {
        return NULL;
    }

#define GX2VK_INSTANCE_PROC(name) \
    if (strcmp(pName, #name) == 0) { \
        return reinterpret_cast<PFN_vkVoidFunction>(name); \
    }

    GX2VK_INSTANCE_PROC(vkGetInstanceProcAddr)
    GX2VK_INSTANCE_PROC(vkCreateInstance)
    GX2VK_INSTANCE_PROC(vkDestroyInstance)
    GX2VK_INSTANCE_PROC(vkEnumerateInstanceVersion)
    GX2VK_INSTANCE_PROC(vkEnumerateInstanceExtensionProperties)
    GX2VK_INSTANCE_PROC(vkEnumerateInstanceLayerProperties)
    GX2VK_INSTANCE_PROC(vkEnumeratePhysicalDevices)
    GX2VK_INSTANCE_PROC(vkEnumerateDeviceExtensionProperties)
    GX2VK_INSTANCE_PROC(vkEnumerateDeviceLayerProperties)
    GX2VK_INSTANCE_PROC(vkGetPhysicalDeviceProperties)
    GX2VK_INSTANCE_PROC(vkGetPhysicalDeviceProperties2)
    GX2VK_INSTANCE_PROC(vkGetPhysicalDeviceFeatures)
    GX2VK_INSTANCE_PROC(vkGetPhysicalDeviceFeatures2)
    GX2VK_INSTANCE_PROC(vkGetPhysicalDeviceQueueFamilyProperties)
    GX2VK_INSTANCE_PROC(vkGetPhysicalDeviceQueueFamilyProperties2)
    GX2VK_INSTANCE_PROC(vkGetPhysicalDeviceMemoryProperties)
    GX2VK_INSTANCE_PROC(vkGetPhysicalDeviceMemoryProperties2)
    GX2VK_INSTANCE_PROC(vkCreateDevice)
    GX2VK_INSTANCE_PROC(vkGetDeviceProcAddr)

#undef GX2VK_INSTANCE_PROC

    return NULL;
}

}
