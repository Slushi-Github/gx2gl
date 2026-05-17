#include "vk/gx2vk_internal.h"

#include <stdlib.h>
#include <string.h>

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice device,
                                                   const VkCommandPoolCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkCommandPool *pCommandPool) {
    gx2vk_command_pool_t *pool = NULL;
    (void)pAllocator;

    if (!device || !pCreateInfo || !pCommandPool || pCreateInfo->queueFamilyIndex != 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    pool = (gx2vk_command_pool_t *)calloc(1, sizeof(*pool));
    if (!pool) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    pool->device = (gx2vk_device_t *)device;
    pool->queue_family_index = pCreateInfo->queueFamilyIndex;
    *pCommandPool = (VkCommandPool)pool;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice device,
                                                VkCommandPool commandPool,
                                                const VkAllocationCallbacks *pAllocator) {
    (void)device;
    (void)pAllocator;
    free((void *)commandPool);
}

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device,
                                                        const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                        VkCommandBuffer *pCommandBuffers) {
    uint32_t i;
    (void)device;

    if (!pAllocateInfo || !pCommandBuffers || !pAllocateInfo->commandPool || pAllocateInfo->commandBufferCount == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    for (i = 0; i < pAllocateInfo->commandBufferCount; ++i) {
        gx2vk_command_buffer_t *command_buffer =
            (gx2vk_command_buffer_t *)calloc(1, sizeof(*command_buffer));
        if (!command_buffer) {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        command_buffer->pool = (gx2vk_command_pool_t *)pAllocateInfo->commandPool;
        command_buffer->state = GX2VK_COMMAND_BUFFER_STATE_INITIAL;
        pCommandBuffers[i] = (VkCommandBuffer)command_buffer;
    }

    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(VkDevice device,
                                                VkCommandPool commandPool,
                                                uint32_t commandBufferCount,
                                                const VkCommandBuffer *pCommandBuffers) {
    uint32_t i;
    (void)device;
    (void)commandPool;

    if (!pCommandBuffers) {
        return;
    }
    for (i = 0; i < commandBufferCount; ++i) {
        free((void *)pCommandBuffers[i]);
    }
}

VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                    const VkCommandBufferBeginInfo *pBeginInfo) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;
    (void)pBeginInfo;

    if (!gx2vk_command_buffer) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    gx2vk_command_buffer->state = GX2VK_COMMAND_BUFFER_STATE_RECORDING;
    gx2vk_command_buffer->inside_rendering = false;
    gx2vk_command_buffer->bound_pipeline = NULL;
    memset(gx2vk_command_buffer->bound_descriptor_sets, 0, sizeof(gx2vk_command_buffer->bound_descriptor_sets));
    memset(gx2vk_command_buffer->vertex_buffers, 0, sizeof(gx2vk_command_buffer->vertex_buffers));
    memset(gx2vk_command_buffer->vertex_buffer_offsets, 0, sizeof(gx2vk_command_buffer->vertex_buffer_offsets));
    gx2vk_command_buffer->index_buffer = NULL;
    gx2vk_command_buffer->index_buffer_offset = 0;
    gx2vk_command_buffer->index_type = VK_INDEX_TYPE_UINT16;
    gx2vk_command_buffer->viewport_set = false;
    gx2vk_command_buffer->scissor_set = false;
    memset(&gx2vk_command_buffer->viewport, 0, sizeof(gx2vk_command_buffer->viewport));
    memset(&gx2vk_command_buffer->scissor, 0, sizeof(gx2vk_command_buffer->scissor));
    memset(&gx2vk_command_buffer->render_area, 0, sizeof(gx2vk_command_buffer->render_area));
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;

    if (!gx2vk_command_buffer ||
        gx2vk_command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_RECORDING ||
        gx2vk_command_buffer->inside_rendering) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    gx2vk_command_buffer->state = GX2VK_COMMAND_BUFFER_STATE_EXECUTABLE;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer,
                                                 const VkDependencyInfo *pDependencyInfo) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;
    (void)pDependencyInfo;

    if (!gx2vk_command_buffer || gx2vk_command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_RECORDING) {
        return;
    }
}

VKAPI_ATTR void VKAPI_CALL vkCmdBeginRendering(VkCommandBuffer commandBuffer,
                                               const VkRenderingInfo *pRenderingInfo) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;

    if (!gx2vk_command_buffer || !pRenderingInfo || gx2vk_command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_RECORDING) {
        return;
    }

    gx2vk_command_buffer->inside_rendering = true;
    gx2vk_command_buffer->render_area = pRenderingInfo->renderArea;
}

VKAPI_ATTR void VKAPI_CALL vkCmdEndRendering(VkCommandBuffer commandBuffer) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;

    if (!gx2vk_command_buffer || gx2vk_command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_RECORDING) {
        return;
    }

    gx2vk_command_buffer->inside_rendering = false;
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                             VkPipelineBindPoint pipelineBindPoint,
                                             VkPipeline pipeline) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;

    if (!gx2vk_command_buffer ||
        gx2vk_command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_RECORDING ||
        pipelineBindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
        return;
    }

    gx2vk_command_buffer->bound_pipeline = (gx2vk_pipeline_t *)pipeline;
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer,
                                                   VkPipelineBindPoint pipelineBindPoint,
                                                   VkPipelineLayout layout,
                                                   uint32_t firstSet,
                                                   uint32_t descriptorSetCount,
                                                   const VkDescriptorSet *pDescriptorSets,
                                                   uint32_t dynamicOffsetCount,
                                                   const uint32_t *pDynamicOffsets) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;
    uint32_t i;
    (void)layout;
    (void)dynamicOffsetCount;
    (void)pDynamicOffsets;

    if (!gx2vk_command_buffer ||
        gx2vk_command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_RECORDING ||
        pipelineBindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS ||
        (descriptorSetCount > 0 && !pDescriptorSets)) {
        return;
    }

    for (i = 0; i < descriptorSetCount && (firstSet + i) < 4; ++i) {
        gx2vk_command_buffer->bound_descriptor_sets[firstSet + i] =
            (gx2vk_descriptor_set_t *)pDescriptorSets[i];
    }
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer,
                                                  uint32_t firstBinding,
                                                  uint32_t bindingCount,
                                                  const VkBuffer *pBuffers,
                                                  const VkDeviceSize *pOffsets) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;
    uint32_t i;

    if (!gx2vk_command_buffer ||
        gx2vk_command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_RECORDING ||
        (bindingCount > 0 && (!pBuffers || !pOffsets))) {
        return;
    }

    for (i = 0; i < bindingCount && (firstBinding + i) < 16; ++i) {
        gx2vk_command_buffer->vertex_buffers[firstBinding + i] = (gx2vk_buffer_t *)pBuffers[i];
        gx2vk_command_buffer->vertex_buffer_offsets[firstBinding + i] = pOffsets[i];
    }
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer,
                                                VkBuffer buffer,
                                                VkDeviceSize offset,
                                                VkIndexType indexType) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;

    if (!gx2vk_command_buffer ||
        gx2vk_command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_RECORDING) {
        return;
    }

    gx2vk_command_buffer->index_buffer = (gx2vk_buffer_t *)buffer;
    gx2vk_command_buffer->index_buffer_offset = offset;
    gx2vk_command_buffer->index_type = indexType;
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer commandBuffer,
                                            uint32_t firstViewport,
                                            uint32_t viewportCount,
                                            const VkViewport *pViewports) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;
    (void)firstViewport;

    if (!gx2vk_command_buffer ||
        gx2vk_command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_RECORDING ||
        viewportCount == 0 ||
        !pViewports) {
        return;
    }

    gx2vk_command_buffer->viewport = pViewports[0];
    gx2vk_command_buffer->viewport_set = true;
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer commandBuffer,
                                           uint32_t firstScissor,
                                           uint32_t scissorCount,
                                           const VkRect2D *pScissors) {
    gx2vk_command_buffer_t *gx2vk_command_buffer = (gx2vk_command_buffer_t *)commandBuffer;
    (void)firstScissor;

    if (!gx2vk_command_buffer ||
        gx2vk_command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_RECORDING ||
        scissorCount == 0 ||
        !pScissors) {
        return;
    }

    gx2vk_command_buffer->scissor = pScissors[0];
    gx2vk_command_buffer->scissor_set = true;
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue,
                                              uint32_t submitCount,
                                              const VkSubmitInfo2 *pSubmits,
                                              VkFence fence) {
    uint32_t submit_index;
    (void)queue;
    (void)fence;

    for (submit_index = 0; submit_index < submitCount; ++submit_index) {
        uint32_t command_index;
        for (command_index = 0; command_index < pSubmits[submit_index].commandBufferInfoCount; ++command_index) {
            gx2vk_command_buffer_t *command_buffer =
                (gx2vk_command_buffer_t *)pSubmits[submit_index].pCommandBufferInfos[command_index].commandBuffer;
            if (!command_buffer || command_buffer->state != GX2VK_COMMAND_BUFFER_STATE_EXECUTABLE) {
                return VK_ERROR_INITIALIZATION_FAILED;
            }
            command_buffer->state = GX2VK_COMMAND_BUFFER_STATE_INITIAL;
        }
    }

    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue queue) {
    (void)queue;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device) {
    (void)device;
    return VK_SUCCESS;
}

}
