#include "vk/gx2vk_internal.h"

#include <stdlib.h>
#include <string.h>

static uint64_t gx2vk_hash_spirv_bytes(const uint8_t *bytes, size_t size) {
    uint64_t hash = 1469598103934665603ull;
    size_t i;

    for (i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }

    return hash;
}

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device,
                                                    const VkShaderModuleCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkShaderModule *pShaderModule) {
    gx2vk_shader_module_t *shader_module = NULL;
    (void)pAllocator;

    if (!device || !pCreateInfo || !pShaderModule || pCreateInfo->sType != VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!pCreateInfo->pCode || pCreateInfo->codeSize == 0 || (pCreateInfo->codeSize % sizeof(uint32_t)) != 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    shader_module = (gx2vk_shader_module_t *)calloc(1, sizeof(*shader_module));
    if (!shader_module) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    shader_module->code_words = (uint32_t *)malloc(pCreateInfo->codeSize);
    if (!shader_module->code_words) {
        free(shader_module);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    shader_module->device = (gx2vk_device_t *)device;
    shader_module->code_size = pCreateInfo->codeSize;
    memcpy(shader_module->code_words, pCreateInfo->pCode, pCreateInfo->codeSize);
    shader_module->spirv_hash =
        gx2vk_hash_spirv_bytes((const uint8_t *)pCreateInfo->pCode, pCreateInfo->codeSize);
    *pShaderModule = (VkShaderModule)shader_module;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice device,
                                                 VkShaderModule shaderModule,
                                                 const VkAllocationCallbacks *pAllocator) {
    gx2vk_shader_module_t *gx2vk_shader_module = (gx2vk_shader_module_t *)shaderModule;
    (void)device;
    (void)pAllocator;

    if (!gx2vk_shader_module) {
        return;
    }
    free(gx2vk_shader_module->code_words);
    free(gx2vk_shader_module);
}

}
