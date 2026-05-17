#ifndef GX2VK_INTERNAL_H
#define GX2VK_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <gx2r/buffer.h>
#include <vulkan/vulkan.h>

typedef struct gx2vk_instance_t {
    uint32_t api_version;
} gx2vk_instance_t;

typedef struct gx2vk_physical_device_t {
    gx2vk_instance_t *instance;
} gx2vk_physical_device_t;

typedef struct gx2vk_device_t gx2vk_device_t;

typedef struct gx2vk_queue_t {
    gx2vk_device_t *device;
    uint32_t family_index;
    uint32_t queue_index;
} gx2vk_queue_t;

struct gx2vk_device_t {
    gx2vk_physical_device_t *physical_device;
    gx2vk_queue_t graphics_queue;
    bool dynamic_rendering_enabled;
    bool synchronization2_enabled;
    bool maintenance4_enabled;
};

typedef struct gx2vk_command_pool_t {
    gx2vk_device_t *device;
    uint32_t queue_family_index;
} gx2vk_command_pool_t;

typedef struct gx2vk_memory_t gx2vk_memory_t;
typedef struct gx2vk_shader_module_t gx2vk_shader_module_t;
typedef struct gx2vk_descriptor_set_layout_t gx2vk_descriptor_set_layout_t;
typedef struct gx2vk_descriptor_pool_t gx2vk_descriptor_pool_t;
typedef struct gx2vk_descriptor_set_t gx2vk_descriptor_set_t;
typedef struct gx2vk_pipeline_layout_t gx2vk_pipeline_layout_t;
typedef struct gx2vk_pipeline_t gx2vk_pipeline_t;
typedef struct gx2vk_vertex_binding_desc_t gx2vk_vertex_binding_desc_t;
typedef struct gx2vk_vertex_attribute_desc_t gx2vk_vertex_attribute_desc_t;
typedef struct gx2vk_blend_attachment_state_t gx2vk_blend_attachment_state_t;

typedef struct gx2vk_buffer_t {
    gx2vk_device_t *device;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkSharingMode sharing_mode;
    GX2RResourceFlags gx2r_flags;
    GX2RBuffer gx2r_buffer;
    bool gx2r_created;
    gx2vk_memory_t *bound_memory;
    VkDeviceSize bound_offset;
    bool needs_uniform_word_swap;
} gx2vk_buffer_t;

struct gx2vk_memory_t {
    gx2vk_device_t *device;
    VkDeviceSize allocation_size;
    uint32_t memory_type_index;
    uint8_t *host_storage;
    bool mapped;
    VkDeviceSize mapped_offset;
    VkDeviceSize mapped_size;
    gx2vk_buffer_t *bound_buffer;
};

struct gx2vk_shader_module_t {
    gx2vk_device_t *device;
    size_t code_size;
    uint32_t *code_words;
    uint64_t spirv_hash;
    VkShaderStageFlagBits stage;
    uint32_t input_attribute_count;
    uint32_t ubo_binding_count;
    uint32_t sampler_binding_count;
};

typedef struct gx2vk_descriptor_binding_state_t {
    uint32_t binding;
    VkDescriptorType descriptor_type;
    uint32_t descriptor_count;
    VkDescriptorBufferInfo *buffer_infos;
    VkDescriptorImageInfo *image_infos;
    VkBufferView *texel_buffer_views;
} gx2vk_descriptor_binding_state_t;

struct gx2vk_descriptor_set_layout_t {
    gx2vk_device_t *device;
    uint32_t binding_count;
    uint32_t max_binding;
    VkDescriptorSetLayoutBinding *bindings;
};

struct gx2vk_descriptor_pool_t {
    gx2vk_device_t *device;
    uint32_t max_sets;
    uint32_t allocated_sets;
    uint32_t pool_size_count;
    VkDescriptorPoolSize *pool_sizes;
};

struct gx2vk_descriptor_set_t {
    gx2vk_descriptor_pool_t *pool;
    gx2vk_descriptor_set_layout_t *layout;
    uint32_t binding_count;
    gx2vk_descriptor_binding_state_t *binding_states;
};

struct gx2vk_pipeline_layout_t {
    gx2vk_device_t *device;
    uint32_t set_layout_count;
    gx2vk_descriptor_set_layout_t **set_layouts;
    uint32_t push_constant_range_count;
    VkPushConstantRange *push_constant_ranges;
};

struct gx2vk_pipeline_t {
    gx2vk_device_t *device;
    gx2vk_pipeline_layout_t *layout;
    uint32_t stage_count;
    VkShaderStageFlags stage_mask;
    VkPrimitiveTopology primitive_topology;
    uint32_t vertex_binding_count;
    gx2vk_vertex_binding_desc_t *vertex_bindings;
    uint32_t vertex_attribute_count;
    gx2vk_vertex_attribute_desc_t *vertex_attributes;
    VkCullModeFlags cull_mode;
    VkFrontFace front_face;
    uint32_t blend_attachment_count;
    gx2vk_blend_attachment_state_t *blend_attachments;
    VkBool32 depth_test_enabled;
    VkBool32 depth_write_enabled;
    VkCompareOp depth_compare_op;
    bool has_static_viewport;
    bool has_static_scissor;
    VkViewport viewport;
    VkRect2D scissor;
};

typedef enum gx2vk_command_buffer_state_t {
    GX2VK_COMMAND_BUFFER_STATE_INITIAL = 0,
    GX2VK_COMMAND_BUFFER_STATE_RECORDING = 1,
    GX2VK_COMMAND_BUFFER_STATE_EXECUTABLE = 2,
} gx2vk_command_buffer_state_t;

typedef struct gx2vk_command_buffer_t {
    gx2vk_command_pool_t *pool;
    gx2vk_command_buffer_state_t state;
    bool inside_rendering;
    VkRect2D render_area;
    gx2vk_pipeline_t *bound_pipeline;
    gx2vk_descriptor_set_t *bound_descriptor_sets[4];
    gx2vk_buffer_t *vertex_buffers[16];
    VkDeviceSize vertex_buffer_offsets[16];
    gx2vk_buffer_t *index_buffer;
    VkDeviceSize index_buffer_offset;
    VkIndexType index_type;
    bool viewport_set;
    bool scissor_set;
    VkViewport viewport;
    VkRect2D scissor;
} gx2vk_command_buffer_t;

struct gx2vk_vertex_binding_desc_t {
    uint32_t binding;
    uint32_t stride;
    VkVertexInputRate input_rate;
};

struct gx2vk_vertex_attribute_desc_t {
    uint32_t location;
    uint32_t binding;
    VkFormat format;
    uint32_t offset;
};

struct gx2vk_blend_attachment_state_t {
    VkBool32 blend_enable;
    VkBlendFactor src_color_blend_factor;
    VkBlendFactor dst_color_blend_factor;
    VkBlendOp color_blend_op;
    VkBlendFactor src_alpha_blend_factor;
    VkBlendFactor dst_alpha_blend_factor;
    VkBlendOp alpha_blend_op;
    VkColorComponentFlags color_write_mask;
};

extern gx2vk_physical_device_t g_gx2vk_physical_device;

#endif
