#include "vk/gx2vk_internal.h"

#include <stdlib.h>
#include <string.h>

static int gx2vk_find_layout_binding_index(const gx2vk_descriptor_set_layout_t *layout, uint32_t binding) {
    uint32_t i;

    if (!layout) {
        return -1;
    }
    for (i = 0; i < layout->binding_count; ++i) {
        if (layout->bindings[i].binding == binding) {
            return (int)i;
        }
    }
    return -1;
}

static void gx2vk_free_descriptor_set(gx2vk_descriptor_set_t *descriptor_set) {
    uint32_t i;

    if (!descriptor_set) {
        return;
    }

    for (i = 0; i < descriptor_set->binding_count; ++i) {
        free(descriptor_set->binding_states[i].buffer_infos);
        free(descriptor_set->binding_states[i].image_infos);
        free(descriptor_set->binding_states[i].texel_buffer_views);
    }
    free(descriptor_set->binding_states);
    free(descriptor_set);
}

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice device,
                                                           const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                           const VkAllocationCallbacks *pAllocator,
                                                           VkDescriptorSetLayout *pSetLayout) {
    gx2vk_descriptor_set_layout_t *layout = NULL;
    uint32_t i;
    (void)pAllocator;

    if (!device || !pCreateInfo || !pSetLayout || pCreateInfo->sType != VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    layout = (gx2vk_descriptor_set_layout_t *)calloc(1, sizeof(*layout));
    if (!layout) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (pCreateInfo->bindingCount > 0) {
        layout->bindings = (VkDescriptorSetLayoutBinding *)calloc(
            pCreateInfo->bindingCount, sizeof(VkDescriptorSetLayoutBinding));
        if (!layout->bindings) {
            free(layout);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        memcpy(layout->bindings,
               pCreateInfo->pBindings,
               pCreateInfo->bindingCount * sizeof(VkDescriptorSetLayoutBinding));
    }

    layout->device = (gx2vk_device_t *)device;
    layout->binding_count = pCreateInfo->bindingCount;
    layout->max_binding = 0;
    for (i = 0; i < pCreateInfo->bindingCount; ++i) {
        if (layout->bindings[i].binding > layout->max_binding) {
            layout->max_binding = layout->bindings[i].binding;
        }
    }

    *pSetLayout = (VkDescriptorSetLayout)layout;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device,
                                                        VkDescriptorSetLayout descriptorSetLayout,
                                                        const VkAllocationCallbacks *pAllocator) {
    gx2vk_descriptor_set_layout_t *layout = (gx2vk_descriptor_set_layout_t *)descriptorSetLayout;
    (void)device;
    (void)pAllocator;

    if (!layout) {
        return;
    }
    free(layout->bindings);
    free(layout);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice device,
                                                      const VkDescriptorPoolCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator,
                                                      VkDescriptorPool *pDescriptorPool) {
    gx2vk_descriptor_pool_t *pool = NULL;
    (void)pAllocator;

    if (!device || !pCreateInfo || !pDescriptorPool || pCreateInfo->sType != VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    pool = (gx2vk_descriptor_pool_t *)calloc(1, sizeof(*pool));
    if (!pool) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (pCreateInfo->poolSizeCount > 0) {
        pool->pool_sizes = (VkDescriptorPoolSize *)calloc(pCreateInfo->poolSizeCount, sizeof(VkDescriptorPoolSize));
        if (!pool->pool_sizes) {
            free(pool);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        memcpy(pool->pool_sizes,
               pCreateInfo->pPoolSizes,
               pCreateInfo->poolSizeCount * sizeof(VkDescriptorPoolSize));
    }

    pool->device = (gx2vk_device_t *)device;
    pool->max_sets = pCreateInfo->maxSets;
    pool->pool_size_count = pCreateInfo->poolSizeCount;
    *pDescriptorPool = (VkDescriptorPool)pool;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(VkDevice device,
                                                   VkDescriptorPool descriptorPool,
                                                   const VkAllocationCallbacks *pAllocator) {
    gx2vk_descriptor_pool_t *pool = (gx2vk_descriptor_pool_t *)descriptorPool;
    (void)device;
    (void)pAllocator;

    if (!pool) {
        return;
    }
    free(pool->pool_sizes);
    free(pool);
}

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice device,
                                                        const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                        VkDescriptorSet *pDescriptorSets) {
    uint32_t i;
    (void)device;

    if (!pAllocateInfo || !pDescriptorSets || pAllocateInfo->sType != VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    {
        gx2vk_descriptor_pool_t *pool = (gx2vk_descriptor_pool_t *)pAllocateInfo->descriptorPool;
        if (!pool || (pool->allocated_sets + pAllocateInfo->descriptorSetCount) > pool->max_sets) {
            return VK_ERROR_OUT_OF_POOL_MEMORY;
        }

        for (i = 0; i < pAllocateInfo->descriptorSetCount; ++i) {
            gx2vk_descriptor_set_layout_t *layout =
                (gx2vk_descriptor_set_layout_t *)pAllocateInfo->pSetLayouts[i];
            gx2vk_descriptor_set_t *descriptor_set =
                (gx2vk_descriptor_set_t *)calloc(1, sizeof(*descriptor_set));
            uint32_t binding_index;

            if (!layout || !descriptor_set) {
                gx2vk_free_descriptor_set(descriptor_set);
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }

            descriptor_set->pool = pool;
            descriptor_set->layout = layout;
            descriptor_set->binding_count = layout->binding_count;
            descriptor_set->binding_states = (gx2vk_descriptor_binding_state_t *)calloc(
                layout->binding_count, sizeof(gx2vk_descriptor_binding_state_t));
            if (!descriptor_set->binding_states && layout->binding_count > 0) {
                gx2vk_free_descriptor_set(descriptor_set);
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }

            for (binding_index = 0; binding_index < layout->binding_count; ++binding_index) {
                const VkDescriptorSetLayoutBinding *layout_binding = &layout->bindings[binding_index];
                gx2vk_descriptor_binding_state_t *state = &descriptor_set->binding_states[binding_index];

                state->binding = layout_binding->binding;
                state->descriptor_type = layout_binding->descriptorType;
                state->descriptor_count = layout_binding->descriptorCount;
                if (layout_binding->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
                    layout_binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) {
                    state->texel_buffer_views = (VkBufferView *)calloc(
                        layout_binding->descriptorCount, sizeof(VkBufferView));
                } else if (layout_binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                           layout_binding->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                           layout_binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                           layout_binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                           layout_binding->descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
                    state->image_infos = (VkDescriptorImageInfo *)calloc(
                        layout_binding->descriptorCount, sizeof(VkDescriptorImageInfo));
                } else {
                    state->buffer_infos = (VkDescriptorBufferInfo *)calloc(
                        layout_binding->descriptorCount, sizeof(VkDescriptorBufferInfo));
                }
            }

            pDescriptorSets[i] = (VkDescriptorSet)descriptor_set;
            pool->allocated_sets += 1;
        }
    }

    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice device,
                                                    VkDescriptorPool descriptorPool,
                                                    uint32_t descriptorSetCount,
                                                    const VkDescriptorSet *pDescriptorSets) {
    uint32_t i;
    gx2vk_descriptor_pool_t *pool = (gx2vk_descriptor_pool_t *)descriptorPool;
    (void)device;

    if (!pool || (descriptorSetCount > 0 && !pDescriptorSets)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    for (i = 0; i < descriptorSetCount; ++i) {
        gx2vk_free_descriptor_set((gx2vk_descriptor_set_t *)pDescriptorSets[i]);
        if (pool->allocated_sets > 0) {
            pool->allocated_sets -= 1;
        }
    }

    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(VkDevice device,
                                                  uint32_t descriptorWriteCount,
                                                  const VkWriteDescriptorSet *pDescriptorWrites,
                                                  uint32_t descriptorCopyCount,
                                                  const VkCopyDescriptorSet *pDescriptorCopies) {
    uint32_t i;
    (void)device;
    (void)descriptorCopyCount;
    (void)pDescriptorCopies;

    if (descriptorWriteCount > 0 && !pDescriptorWrites) {
        return;
    }

    for (i = 0; i < descriptorWriteCount; ++i) {
        gx2vk_descriptor_set_t *descriptor_set = (gx2vk_descriptor_set_t *)pDescriptorWrites[i].dstSet;
        int binding_index = -1;
        gx2vk_descriptor_binding_state_t *state = NULL;
        uint32_t copy_count = 0;

        if (!descriptor_set) {
            continue;
        }

        binding_index = gx2vk_find_layout_binding_index(descriptor_set->layout, pDescriptorWrites[i].dstBinding);
        if (binding_index < 0) {
            continue;
        }

        state = &descriptor_set->binding_states[binding_index];
        if (pDescriptorWrites[i].dstArrayElement >= state->descriptor_count) {
            continue;
        }

        copy_count = pDescriptorWrites[i].descriptorCount;
        if ((pDescriptorWrites[i].dstArrayElement + copy_count) > state->descriptor_count) {
            copy_count = state->descriptor_count - pDescriptorWrites[i].dstArrayElement;
        }

        if (state->texel_buffer_views && pDescriptorWrites[i].pTexelBufferView) {
            memcpy(&state->texel_buffer_views[pDescriptorWrites[i].dstArrayElement],
                   pDescriptorWrites[i].pTexelBufferView,
                   copy_count * sizeof(VkBufferView));
        } else if (state->image_infos && pDescriptorWrites[i].pImageInfo) {
            memcpy(&state->image_infos[pDescriptorWrites[i].dstArrayElement],
                   pDescriptorWrites[i].pImageInfo,
                   copy_count * sizeof(VkDescriptorImageInfo));
        } else if (state->buffer_infos && pDescriptorWrites[i].pBufferInfo) {
            memcpy(&state->buffer_infos[pDescriptorWrites[i].dstArrayElement],
                   pDescriptorWrites[i].pBufferInfo,
                   copy_count * sizeof(VkDescriptorBufferInfo));
        }
    }
}

}
