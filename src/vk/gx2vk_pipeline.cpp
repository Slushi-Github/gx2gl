#include "vk/gx2vk_internal.h"

#include <stdlib.h>
#include <string.h>

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device,
                                                      const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator,
                                                      VkPipelineLayout *pPipelineLayout) {
    gx2vk_pipeline_layout_t *layout = NULL;
    (void)pAllocator;

    if (!device || !pCreateInfo || !pPipelineLayout || pCreateInfo->sType != VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    layout = (gx2vk_pipeline_layout_t *)calloc(1, sizeof(*layout));
    if (!layout) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (pCreateInfo->setLayoutCount > 0) {
        layout->set_layouts = (gx2vk_descriptor_set_layout_t **)calloc(
            pCreateInfo->setLayoutCount, sizeof(gx2vk_descriptor_set_layout_t *));
        if (!layout->set_layouts) {
            free(layout);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        memcpy(layout->set_layouts,
               pCreateInfo->pSetLayouts,
               pCreateInfo->setLayoutCount * sizeof(VkDescriptorSetLayout));
    }

    if (pCreateInfo->pushConstantRangeCount > 0) {
        layout->push_constant_ranges = (VkPushConstantRange *)calloc(
            pCreateInfo->pushConstantRangeCount, sizeof(VkPushConstantRange));
        if (!layout->push_constant_ranges) {
            free(layout->set_layouts);
            free(layout);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        memcpy(layout->push_constant_ranges,
               pCreateInfo->pPushConstantRanges,
               pCreateInfo->pushConstantRangeCount * sizeof(VkPushConstantRange));
    }

    layout->device = (gx2vk_device_t *)device;
    layout->set_layout_count = pCreateInfo->setLayoutCount;
    layout->push_constant_range_count = pCreateInfo->pushConstantRangeCount;
    *pPipelineLayout = (VkPipelineLayout)layout;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(VkDevice device,
                                                   VkPipelineLayout pipelineLayout,
                                                   const VkAllocationCallbacks *pAllocator) {
    gx2vk_pipeline_layout_t *layout = (gx2vk_pipeline_layout_t *)pipelineLayout;
    (void)device;
    (void)pAllocator;

    if (!layout) {
        return;
    }
    free(layout->set_layouts);
    free(layout->push_constant_ranges);
    free(layout);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice device,
                                                         VkPipelineCache pipelineCache,
                                                         uint32_t createInfoCount,
                                                         const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                                         const VkAllocationCallbacks *pAllocator,
                                                         VkPipeline *pPipelines) {
    uint32_t i;
    VkResult failure_result = VK_SUCCESS;
    (void)pipelineCache;
    (void)pAllocator;

    if (!device || !pCreateInfos || !pPipelines) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    for (i = 0; i < createInfoCount; ++i) {
        pPipelines[i] = VK_NULL_HANDLE;
    }

    for (i = 0; i < createInfoCount; ++i) {
        const VkGraphicsPipelineCreateInfo *create_info = &pCreateInfos[i];
        gx2vk_pipeline_t *pipeline = (gx2vk_pipeline_t *)calloc(1, sizeof(*pipeline));
        uint32_t stage_index;

        if (!pipeline || create_info->sType != VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO ||
            !create_info->layout || create_info->stageCount == 0 || !create_info->pStages) {
            free(pipeline);
            failure_result = VK_ERROR_INITIALIZATION_FAILED;
            goto fail;
        }

        pipeline->device = (gx2vk_device_t *)device;
        pipeline->layout = (gx2vk_pipeline_layout_t *)create_info->layout;
        pipeline->stage_count = create_info->stageCount;
        pipeline->primitive_topology = create_info->pInputAssemblyState
            ? create_info->pInputAssemblyState->topology
            : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        if (create_info->pVertexInputState) {
            if (create_info->pVertexInputState->vertexBindingDescriptionCount > 0) {
                pipeline->vertex_bindings = (gx2vk_vertex_binding_desc_t *)calloc(
                    create_info->pVertexInputState->vertexBindingDescriptionCount,
                    sizeof(gx2vk_vertex_binding_desc_t));
                if (!pipeline->vertex_bindings) {
                    free(pipeline);
                    failure_result = VK_ERROR_OUT_OF_HOST_MEMORY;
                    goto fail;
                }
                pipeline->vertex_binding_count =
                    create_info->pVertexInputState->vertexBindingDescriptionCount;
                for (uint32_t binding_index = 0;
                     binding_index < pipeline->vertex_binding_count;
                     ++binding_index) {
                    pipeline->vertex_bindings[binding_index].binding =
                        create_info->pVertexInputState->pVertexBindingDescriptions[binding_index].binding;
                    pipeline->vertex_bindings[binding_index].stride =
                        create_info->pVertexInputState->pVertexBindingDescriptions[binding_index].stride;
                    pipeline->vertex_bindings[binding_index].input_rate =
                        create_info->pVertexInputState->pVertexBindingDescriptions[binding_index].inputRate;
                }
            }
            if (create_info->pVertexInputState->vertexAttributeDescriptionCount > 0) {
                pipeline->vertex_attributes = (gx2vk_vertex_attribute_desc_t *)calloc(
                    create_info->pVertexInputState->vertexAttributeDescriptionCount,
                    sizeof(gx2vk_vertex_attribute_desc_t));
                if (!pipeline->vertex_attributes) {
                    free(pipeline->vertex_bindings);
                    free(pipeline);
                    failure_result = VK_ERROR_OUT_OF_HOST_MEMORY;
                    goto fail;
                }
                pipeline->vertex_attribute_count =
                    create_info->pVertexInputState->vertexAttributeDescriptionCount;
                for (uint32_t attribute_index = 0;
                     attribute_index < pipeline->vertex_attribute_count;
                     ++attribute_index) {
                    pipeline->vertex_attributes[attribute_index].location =
                        create_info->pVertexInputState->pVertexAttributeDescriptions[attribute_index].location;
                    pipeline->vertex_attributes[attribute_index].binding =
                        create_info->pVertexInputState->pVertexAttributeDescriptions[attribute_index].binding;
                    pipeline->vertex_attributes[attribute_index].format =
                        create_info->pVertexInputState->pVertexAttributeDescriptions[attribute_index].format;
                    pipeline->vertex_attributes[attribute_index].offset =
                        create_info->pVertexInputState->pVertexAttributeDescriptions[attribute_index].offset;
                }
            }
        }
        pipeline->cull_mode = create_info->pRasterizationState
            ? create_info->pRasterizationState->cullMode
            : VK_CULL_MODE_NONE;
        pipeline->front_face = create_info->pRasterizationState
            ? create_info->pRasterizationState->frontFace
            : VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipeline->depth_test_enabled = create_info->pDepthStencilState
            ? create_info->pDepthStencilState->depthTestEnable
            : VK_FALSE;
        pipeline->depth_write_enabled = create_info->pDepthStencilState
            ? create_info->pDepthStencilState->depthWriteEnable
            : VK_FALSE;
        pipeline->depth_compare_op = create_info->pDepthStencilState
            ? create_info->pDepthStencilState->depthCompareOp
            : VK_COMPARE_OP_ALWAYS;

        if (create_info->pColorBlendState &&
            create_info->pColorBlendState->attachmentCount > 0 &&
            create_info->pColorBlendState->pAttachments) {
            pipeline->blend_attachment_count = create_info->pColorBlendState->attachmentCount;
            pipeline->blend_attachments = (gx2vk_blend_attachment_state_t *)calloc(
                pipeline->blend_attachment_count, sizeof(gx2vk_blend_attachment_state_t));
            if (!pipeline->blend_attachments) {
                free(pipeline->vertex_attributes);
                free(pipeline->vertex_bindings);
                free(pipeline);
                failure_result = VK_ERROR_OUT_OF_HOST_MEMORY;
                goto fail;
            }
            for (uint32_t attachment_index = 0;
                 attachment_index < pipeline->blend_attachment_count;
                 ++attachment_index) {
                const VkPipelineColorBlendAttachmentState *attachment =
                    &create_info->pColorBlendState->pAttachments[attachment_index];
                pipeline->blend_attachments[attachment_index].blend_enable = attachment->blendEnable;
                pipeline->blend_attachments[attachment_index].src_color_blend_factor = attachment->srcColorBlendFactor;
                pipeline->blend_attachments[attachment_index].dst_color_blend_factor = attachment->dstColorBlendFactor;
                pipeline->blend_attachments[attachment_index].color_blend_op = attachment->colorBlendOp;
                pipeline->blend_attachments[attachment_index].src_alpha_blend_factor = attachment->srcAlphaBlendFactor;
                pipeline->blend_attachments[attachment_index].dst_alpha_blend_factor = attachment->dstAlphaBlendFactor;
                pipeline->blend_attachments[attachment_index].alpha_blend_op = attachment->alphaBlendOp;
                pipeline->blend_attachments[attachment_index].color_write_mask = attachment->colorWriteMask;
            }
        }

        if (create_info->pViewportState && create_info->pViewportState->viewportCount > 0 &&
            create_info->pViewportState->pViewports) {
            pipeline->has_static_viewport = true;
            pipeline->viewport = create_info->pViewportState->pViewports[0];
        }
        if (create_info->pViewportState && create_info->pViewportState->scissorCount > 0 &&
            create_info->pViewportState->pScissors) {
            pipeline->has_static_scissor = true;
            pipeline->scissor = create_info->pViewportState->pScissors[0];
        }

        for (stage_index = 0; stage_index < create_info->stageCount; ++stage_index) {
            gx2vk_shader_module_t *module =
                (gx2vk_shader_module_t *)create_info->pStages[stage_index].module;
            if (!module) {
                free(pipeline->blend_attachments);
                free(pipeline->vertex_attributes);
                free(pipeline->vertex_bindings);
                free(pipeline);
                failure_result = VK_ERROR_INITIALIZATION_FAILED;
                goto fail;
            }
            module->stage = (VkShaderStageFlagBits)create_info->pStages[stage_index].stage;
            pipeline->stage_mask |= create_info->pStages[stage_index].stage;
        }

        pPipelines[i] = (VkPipeline)pipeline;
    }

    return VK_SUCCESS;

fail:
    for (uint32_t created_index = 0; created_index < createInfoCount; ++created_index) {
        if (pPipelines[created_index] != VK_NULL_HANDLE) {
            gx2vk_pipeline_t *created_pipeline = (gx2vk_pipeline_t *)pPipelines[created_index];
            free(created_pipeline->blend_attachments);
            free(created_pipeline->vertex_attributes);
            free(created_pipeline->vertex_bindings);
            free(created_pipeline);
            pPipelines[created_index] = VK_NULL_HANDLE;
        }
    }
    return failure_result;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice device,
                                             VkPipeline pipeline,
                                             const VkAllocationCallbacks *pAllocator) {
    (void)device;
    (void)pAllocator;
    if (pipeline) {
        gx2vk_pipeline_t *gx2vk_pipeline = (gx2vk_pipeline_t *)pipeline;
        free(gx2vk_pipeline->blend_attachments);
        free(gx2vk_pipeline->vertex_attributes);
        free(gx2vk_pipeline->vertex_bindings);
        free(gx2vk_pipeline);
    }
}

}
