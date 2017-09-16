#include <libavutil/macros.h>

#include "utils.h"
#include "malloc.h"

const char* vk_err(VkResult res)
{
    switch (res) {
    // These are technically success codes, but include them nonetheless
    case VK_SUCCESS:     return "VK_SUCCESS";
    case VK_NOT_READY:   return "VK_NOT_READY";
    case VK_TIMEOUT:     return "VK_TIMEOUT";
    case VK_EVENT_SET:   return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE:  return "VK_INCOMPLETE";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";

    // Actual error codes
    case VK_ERROR_OUT_OF_HOST_MEMORY:    return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:  return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:           return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:     return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:     return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:   return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:   return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:      return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:  return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:       return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_INVALID_SHADER_NV:     return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_OUT_OF_DATE_KHR:       return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_SURFACE_LOST_KHR:      return "VK_ERROR_SURFACE_LOST_KHR";
    }

    return "Unknown error!";
}

static const char* vk_dbg_type(VkDebugReportObjectTypeEXT type)
{
    switch (type) {
    case VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT:
        return "VkInstance";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT:
        return "VkPhysicalDevice";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT:
        return "VkDevice";
    case VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT:
        return "VkQueue";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT:
        return "VkSemaphore";
    case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT:
        return "VkCommandBuffer";
    case VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT:
        return "VkFence";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT:
        return "VkDeviceMemory";
    case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT:
        return "VkBuffer";
    case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT:
        return "VkImage";
    case VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT:
        return "VkEvent";
    case VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT:
        return "VkQueryPool";
    case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT:
        return "VkBufferView";
    case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT:
        return "VkImageView";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT:
        return "VkShaderModule";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT:
        return "VkPipelineCache";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT:
        return "VkPipelineLayout";
    case VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT:
        return "VkRenderPass";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT:
        return "VkPipeline";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT:
        return "VkDescriptorSetLayout";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT:
        return "VkSampler";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT:
        return "VkDescriptorPool";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT:
        return "VkDescriptorSet";
    case VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT:
        return "VkFramebuffer";
    case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT:
        return "VkCommandPool";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT:
        return "VkSurfaceKHR";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT:
        return "VkSwapchainKHR";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_EXT:
        return "VkDebugReportCallbackEXT";
    case VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT:
    default:
        return "unknown object";
    }
}

static VkBool32 vk_dbg_callback(VkDebugReportFlagsEXT flags,
                                VkDebugReportObjectTypeEXT objType,
                                uint64_t obj, size_t loc, int32_t msgCode,
                                const char *layer, const char *msg, void *priv)
{
    struct mpvk_ctx *vk = priv;
    int lev = MSGL_V;

    switch (flags) {
    case VK_DEBUG_REPORT_ERROR_BIT_EXT:               lev = MSGL_ERR;   break;
    case VK_DEBUG_REPORT_WARNING_BIT_EXT:             lev = MSGL_WARN;  break;
    case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:         lev = MSGL_TRACE; break;
    case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT: lev = MSGL_WARN;  break;
    case VK_DEBUG_REPORT_DEBUG_BIT_EXT:               lev = MSGL_DEBUG; break;
    };

    MP_MSG(vk, lev, "vk [%s] %d: %s (obj 0x%lx (%s), loc 0x%lx)\n",
            layer, msgCode, msg, obj, vk_dbg_type(objType), loc);

    // The return value of this function determines whether the call will
    // be explicitly aborted (to prevent GPU errors) or not. In this case,
    // we generally want this to be on for the errors.
    return (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT);
}

static void vk_cmdpool_uninit(struct mpvk_ctx *vk, struct vk_cmdpool *pool)
{
    if (!pool)
        return;

    // also frees associated command buffers
    vkDestroyCommandPool(vk->dev, pool->pool, MPVK_ALLOCATOR);
    for (int n = 0; n < MPVK_MAX_CMDS; n++) {
        vkDestroyFence(vk->dev, pool->cmds[n].fence, MPVK_ALLOCATOR);
        vkDestroySemaphore(vk->dev, pool->cmds[n].done, MPVK_ALLOCATOR);
        talloc_free(pool->cmds[n].callbacks);
    }
    talloc_free(pool);
}

void mpvk_uninit(struct mpvk_ctx *vk)
{
    if (!vk->inst)
        return;

    if (vk->dev) {
        vk_cmdpool_uninit(vk, vk->pool);
        vk_cmdpool_uninit(vk, vk->pool_transfer);
        vk_malloc_uninit(vk);
        vkDestroyDevice(vk->dev, MPVK_ALLOCATOR);
    }

    if (vk->dbg) {
        // Same deal as creating the debug callback, we need to load this
        // first.
        VK_LOAD_PFN(vkDestroyDebugReportCallbackEXT)
        pfn_vkDestroyDebugReportCallbackEXT(vk->inst, vk->dbg, MPVK_ALLOCATOR);
    }

    vkDestroySurfaceKHR(vk->inst, vk->surf, MPVK_ALLOCATOR);
    vkDestroyInstance(vk->inst, MPVK_ALLOCATOR);

    *vk = (struct mpvk_ctx){0};
}

bool mpvk_instance_init(struct mpvk_ctx *vk, struct mp_log *log,
                        const char *surf_ext_name, bool debug)
{
    *vk = (struct mpvk_ctx) {
        .log = log,
    };

    VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    };

    if (debug) {
        // Enables the LunarG standard validation layer, which
        // is a meta-layer that loads lots of other validators
        static const char* layers[] = {
            "VK_LAYER_LUNARG_standard_validation",
        };

        info.ppEnabledLayerNames = layers;
        info.enabledLayerCount = MP_ARRAY_SIZE(layers);
    }

    // Enable whatever extensions were compiled in.
    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        surf_ext_name,

        // Extra extensions only used for debugging. These are toggled by
        // decreasing the enabledExtensionCount, so the number needs to be
        // synchronized with the code below.
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    };

    const int debugExtensionCount = 1;

    info.ppEnabledExtensionNames = extensions;
    info.enabledExtensionCount = MP_ARRAY_SIZE(extensions);

    if (!debug)
        info.enabledExtensionCount -= debugExtensionCount;

    VkResult res = vkCreateInstance(&info, MPVK_ALLOCATOR, &vk->inst);
    if (res != VK_SUCCESS) {
        MP_VERBOSE(vk, "failed creating instance: %s\n", vk_err(res));
        return false;
    }

    if (debug) {
        // Set up a debug callback to catch validation messages
        VkDebugReportCallbackCreateInfoEXT dinfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
            .flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
                     VK_DEBUG_REPORT_WARNING_BIT_EXT |
                     VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                     VK_DEBUG_REPORT_ERROR_BIT_EXT |
                     VK_DEBUG_REPORT_DEBUG_BIT_EXT,
            .pfnCallback = vk_dbg_callback,
            .pUserData = vk,
        };

        // Since this is not part of the core spec, we need to load it. This
        // can't fail because we've already successfully created an instance
        // with this extension enabled.
        VK_LOAD_PFN(vkCreateDebugReportCallbackEXT)
        pfn_vkCreateDebugReportCallbackEXT(vk->inst, &dinfo, MPVK_ALLOCATOR,
                                           &vk->dbg);
    }

    return true;
}

#define MPVK_MAX_DEVICES 16

static bool physd_supports_surface(struct mpvk_ctx *vk, VkPhysicalDevice physd)
{
    uint32_t qfnum;
    vkGetPhysicalDeviceQueueFamilyProperties(physd, &qfnum, NULL);

    for (int i = 0; i < qfnum; i++) {
        VkBool32 sup;
        VK(vkGetPhysicalDeviceSurfaceSupportKHR(physd, i, vk->surf, &sup));
        if (sup)
            return true;
    }

error:
    return false;
}

bool mpvk_find_phys_device(struct mpvk_ctx *vk, const char *name, bool sw)
{
    assert(vk->surf);

    MP_VERBOSE(vk, "Probing for vulkan devices..\n");

    VkPhysicalDevice *devices = NULL;
    uint32_t num = 0;
    VK(vkEnumeratePhysicalDevices(vk->inst, &num, NULL));
    devices = talloc_array(NULL, VkPhysicalDevice, num);
    VK(vkEnumeratePhysicalDevices(vk->inst, &num, devices));

    // Sorted by "priority". Reuses some m_opt code for convenience
    static const struct m_opt_choice_alternatives types[] = {
        {"discrete",   VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU},
        {"integrated", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU},
        {"virtual",    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU},
        {"software",   VK_PHYSICAL_DEVICE_TYPE_CPU},
        {"unknown",    VK_PHYSICAL_DEVICE_TYPE_OTHER},
        {0}
    };

    VkPhysicalDeviceProperties props[MPVK_MAX_DEVICES];
    for (int i = 0; i < num; i++) {
        vkGetPhysicalDeviceProperties(devices[i], &props[i]);
        MP_VERBOSE(vk, "GPU %d: %s (%s)\n", i, props[i].deviceName,
                   m_opt_choice_str(types, props[i].deviceType));
    }

    // Iterate through each type in order of decreasing preference
    for (int t = 0; types[t].name; t++) {
        // Disallow SW rendering unless explicitly enabled
        if (types[t].value == VK_PHYSICAL_DEVICE_TYPE_CPU && !sw)
            continue;

        for (int i = 0; i < num; i++) {
            VkPhysicalDeviceProperties prop = props[i];
            if (prop.deviceType != types[t].value)
                continue;
            if (name && strcmp(name, prop.deviceName) != 0)
                continue;
            if (!physd_supports_surface(vk, devices[i]))
                continue;

            MP_VERBOSE(vk, "Found device:\n");
            MP_VERBOSE(vk, "  Device Name: %s\n", prop.deviceName);
            MP_VERBOSE(vk, "  Device ID: %x:%x\n", prop.vendorID, prop.deviceID);
            MP_VERBOSE(vk, "  Driver version: %d\n", prop.driverVersion);
            MP_VERBOSE(vk, "  API version: %d.%d.%d\n",
                    VK_VERSION_MAJOR(prop.apiVersion),
                    VK_VERSION_MINOR(prop.apiVersion),
                    VK_VERSION_PATCH(prop.apiVersion));
            vk->physd = devices[i];
            vk->limits = prop.limits;
            talloc_free(devices);
            return true;
        }
    }

error:
    MP_VERBOSE(vk, "Found no suitable device, giving up.\n");
    talloc_free(devices);
    return false;
}

bool mpvk_pick_surface_format(struct mpvk_ctx *vk)
{
    assert(vk->physd);

    VkSurfaceFormatKHR *formats = NULL;
    int num;

    // Enumerate through the surface formats and find one that we can map to
    // a ra_format
    VK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physd, vk->surf, &num, NULL));
    formats = talloc_array(NULL, VkSurfaceFormatKHR, num);
    VK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physd, vk->surf, &num, formats));

    for (int i = 0; i < num; i++) {
        // A value of VK_FORMAT_UNDEFINED means we can pick anything we want
        if (formats[i].format == VK_FORMAT_UNDEFINED) {
            vk->surf_format = (VkSurfaceFormatKHR) {
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
            };
            break;
        }

        if (formats[i].colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            continue;

        vk->surf_format = formats[i];
        break;
    }

    talloc_free(formats);

    if (!vk->surf_format.format)
        goto error;

    return true;

error:
    MP_ERR(vk, "Failed picking surface format!\n");
    talloc_free(formats);
    return false;
}

static bool vk_cmdpool_init(struct mpvk_ctx *vk, VkDeviceQueueCreateInfo qinfo,
                            VkQueueFamilyProperties props,
                            struct vk_cmdpool **out)
{
    struct vk_cmdpool *pool = *out = talloc_ptrtype(NULL, pool);
    *pool = (struct vk_cmdpool) {
        .qf = qinfo.queueFamilyIndex,
        .props = props,
        .qcount = qinfo.queueCount,
    };

    for (int n = 0; n < pool->qcount; n++)
        vkGetDeviceQueue(vk->dev, pool->qf, n, &pool->queues[n]);

    VkCommandPoolCreateInfo cinfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = pool->qf,
    };

    VK(vkCreateCommandPool(vk->dev, &cinfo, MPVK_ALLOCATOR, &pool->pool));

    VkCommandBufferAllocateInfo ainfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool->pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MPVK_MAX_CMDS,
    };

    VkCommandBuffer cmdbufs[MPVK_MAX_CMDS];
    VK(vkAllocateCommandBuffers(vk->dev, &ainfo, cmdbufs));

    for (int n = 0; n < MPVK_MAX_CMDS; n++) {
        struct vk_cmd *cmd = &pool->cmds[n];
        cmd->pool = pool;
        cmd->buf = cmdbufs[n];

        VkFenceCreateInfo finfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        VK(vkCreateFence(vk->dev, &finfo, MPVK_ALLOCATOR, &cmd->fence));

        VkSemaphoreCreateInfo sinfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        VK(vkCreateSemaphore(vk->dev, &sinfo, MPVK_ALLOCATOR, &cmd->done));
    }

    return true;

error:
    return false;
}

bool mpvk_device_init(struct mpvk_ctx *vk, int queue_depth)
{
    assert(vk->physd);

    VkQueueFamilyProperties *qfs = NULL;
    int qfnum;

    // Enumerate the queue families and find suitable families for each task
    vkGetPhysicalDeviceQueueFamilyProperties(vk->physd, &qfnum, NULL);
    qfs = talloc_array(NULL, VkQueueFamilyProperties, qfnum);
    vkGetPhysicalDeviceQueueFamilyProperties(vk->physd, &qfnum, qfs);

    MP_VERBOSE(vk, "Queue families supported by device:\n");

    for (int i = 0; i < qfnum; i++) {
        MP_VERBOSE(vk, "QF %d: flags 0x%x num %d\n", i, qfs[i].queueFlags,
                   qfs[i].queueCount);
    }

    // For most of our rendering operations, we want to use one "primary" pool,
    // so just pick the queue family with the most features.
    int idx = -1;
    for (int i = 0; i < qfnum; i++) {
        if (!(qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            continue;

        // QF supports more features
        if (idx < 0 || qfs[i].queueFlags > qfs[idx].queueFlags)
            idx = i;

        // QF supports more queues (at the same specialization level)
        if (qfs[i].queueFlags == qfs[idx].queueFlags &&
            qfs[i].queueCount > qfs[idx].queueCount)
        {
            idx = i;
        }
    }

    // Vulkan requires at least one GRAPHICS queue, so if this fails something
    // is horribly wrong.
    assert(idx >= 0);

    // Ensure we can actually present to the surface using this queue
    VkBool32 sup;
    VK(vkGetPhysicalDeviceSurfaceSupportKHR(vk->physd, idx, vk->surf, &sup));
    if (!sup) {
        MP_ERR(vk, "Queue family does not support surface presentation!\n");
        goto error;
    }

    // We could additionally try and pick a transfer queue if there is one.
    bool use_transfer = false;
    int idx_tf = 0;
    for (int i = 0; i < qfnum; i++) {
        if (i == idx)
            continue;
        if (!(qfs[i].queueFlags & VK_QUEUE_TRANSFER_BIT))
            continue;
        use_transfer = true;
        idx_tf = i;
        break;
    }

    // Now that we know which queue families we want, we can create the logical
    // device
    assert(queue_depth <= MPVK_MAX_QUEUES);
    static const float priorities[MPVK_MAX_QUEUES] = {0};
    VkDeviceQueueCreateInfo qinfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = idx,
        .queueCount = MPMIN(qfs[idx].queueCount, queue_depth),
        .pQueuePriorities = priorities,
    };

    VkDeviceQueueCreateInfo qinfo_tf = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = idx_tf,
        .queueCount = MPMIN(qfs[idx_tf].queueCount, queue_depth),
        .pQueuePriorities = priorities,
    };

    static const char *exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_NV_GLSL_SHADER_EXTENSION_NAME,
    };

    VkDeviceCreateInfo dinfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = use_transfer ? 2 : 1,
        .pQueueCreateInfos = (struct VkDeviceQueueCreateInfo[]){qinfo, qinfo_tf},
        .ppEnabledExtensionNames = exts,
        .enabledExtensionCount = MP_ARRAY_SIZE(exts),
    };

    MP_VERBOSE(vk, "Creating vulkan device...\n");
    VK(vkCreateDevice(vk->physd, &dinfo, MPVK_ALLOCATOR, &vk->dev));

    vk_malloc_init(vk);

    // Create the vk_cmdpools and all required queues / synchronization objects
    if (!vk_cmdpool_init(vk, qinfo, qfs[idx], &vk->pool))
        goto error;
    if (use_transfer) {
        MP_VERBOSE(vk, "Using async transfer (QF %d)\n", idx_tf);
        if (!vk_cmdpool_init(vk, qinfo_tf, qfs[idx_tf], &vk->pool_transfer))
            goto error;
    }

    talloc_free(qfs);
    return true;

error:
    MP_ERR(vk, "Failed creating logical device!\n");
    talloc_free(qfs);
    return false;
}

static void run_callbacks(struct mpvk_ctx *vk, struct vk_cmd *cmd)
{
    for (int i = 0; i < cmd->num_callbacks; i++) {
        struct vk_callback *cb = &cmd->callbacks[i];
        cb->run(cb->priv, cb->arg);
        *cb = (struct vk_callback){0};
    }

    cmd->num_callbacks = 0;

    // Also reset vk->last_cmd in case this was the last command to run
    if (vk->last_cmd == cmd)
        vk->last_cmd = NULL;
}

static void wait_for_cmds(struct mpvk_ctx *vk, struct vk_cmd cmds[], int num)
{
    if (!num)
        return;

    VkFence fences[MPVK_MAX_CMDS];
    for (int i = 0; i < num; i++)
        fences[i] = cmds[i].fence;

    vkWaitForFences(vk->dev, num, fences, true, UINT64_MAX);

    for (int i = 0; i < num; i++)
        run_callbacks(vk, &cmds[i]);
}

void mpvk_pool_wait_idle(struct mpvk_ctx *vk, struct vk_cmdpool *pool)
{
    if (!pool)
        return;

    int idx = pool->cindex, pidx = pool->cindex_pending;
    if (pidx < idx) { // range doesn't wrap
        wait_for_cmds(vk, &pool->cmds[pidx], idx - pidx);
    } else if (pidx > idx) { // range wraps
        wait_for_cmds(vk, &pool->cmds[pidx], MPVK_MAX_CMDS - pidx);
        wait_for_cmds(vk, &pool->cmds[0], idx);
    }
    pool->cindex_pending = pool->cindex;
}

void mpvk_dev_wait_idle(struct mpvk_ctx *vk)
{
    mpvk_pool_wait_idle(vk, vk->pool);
    mpvk_pool_wait_idle(vk, vk->pool_transfer);
}

void mpvk_pool_poll_cmds(struct mpvk_ctx *vk, struct vk_cmdpool *pool,
                         uint64_t timeout)
{
    // If requested, hard block until at least one command completes
    if (timeout > 0 && pool->cindex_pending != pool->cindex) {
        vkWaitForFences(vk->dev, 1, &pool->cmds[pool->cindex_pending].fence,
                        true, timeout);
    }

    // Lazily garbage collect the commands based on their status
    while (pool->cindex_pending != pool->cindex) {
        struct vk_cmd *cmd = &pool->cmds[pool->cindex_pending];
        VkResult res = vkGetFenceStatus(vk->dev, cmd->fence);
        if (res != VK_SUCCESS)
            break;
        run_callbacks(vk, cmd);
        pool->cindex_pending++;
        pool->cindex_pending %= MPVK_MAX_CMDS;
    }
}

void vk_dev_callback(struct mpvk_ctx *vk, vk_cb callback, void *p, void *arg)
{
    if (vk->last_cmd) {
        vk_cmd_callback(vk->last_cmd, callback, p, arg);
    } else {
        // The device was already idle, so we can just immediately call it
        callback(p, arg);
    }
}

void vk_cmd_callback(struct vk_cmd *cmd, vk_cb callback, void *p, void *arg)
{
    MP_TARRAY_GROW(NULL, cmd->callbacks, cmd->num_callbacks);
    cmd->callbacks[cmd->num_callbacks++] = (struct vk_callback) {
        .run  = callback,
        .priv = p,
        .arg  = arg,
    };
}

void vk_cmd_dep(struct vk_cmd *cmd, VkSemaphore dep,
                VkPipelineStageFlagBits depstage)
{
    assert(cmd->num_deps < MPVK_MAX_CMD_DEPS);
    cmd->deps[cmd->num_deps] = dep;
    cmd->depstages[cmd->num_deps++] = depstage;
}

struct vk_cmd *vk_cmd_begin(struct mpvk_ctx *vk, struct vk_cmdpool *pool)
{
    // Garbage collect the cmdpool first
    mpvk_pool_poll_cmds(vk, pool, 0);

    int next = (pool->cindex + 1) % MPVK_MAX_CMDS;
    if (next == pool->cindex_pending) {
        MP_ERR(vk, "No free command buffers!\n");
        goto error;
    }

    struct vk_cmd *cmd = &pool->cmds[pool->cindex];
    pool->cindex = next;

    VK(vkResetCommandBuffer(cmd->buf, 0));

    VkCommandBufferBeginInfo binfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VK(vkBeginCommandBuffer(cmd->buf, &binfo));

    return cmd;

error:
    return NULL;
}

bool vk_cmd_submit(struct mpvk_ctx *vk, struct vk_cmd *cmd, VkSemaphore *done)
{
    VK(vkEndCommandBuffer(cmd->buf));

    struct vk_cmdpool *pool = cmd->pool;
    VkQueue queue = pool->queues[pool->qindex++];
    pool->qindex %= pool->qcount;

    VkSubmitInfo sinfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd->buf,
        .waitSemaphoreCount = cmd->num_deps,
        .pWaitSemaphores = cmd->deps,
        .pWaitDstStageMask = cmd->depstages,
    };

    if (done) {
        sinfo.signalSemaphoreCount = 1;
        sinfo.pSignalSemaphores = &cmd->done;
        *done = cmd->done;
    }

    VK(vkResetFences(vk->dev, 1, &cmd->fence));
    VK(vkQueueSubmit(queue, 1, &sinfo, cmd->fence));
    MP_TRACE(vk, "Submitted command on queue %p (QF %d)\n", (void *)queue,
             pool->qf);

    for (int i = 0; i < cmd->num_deps; i++)
        cmd->deps[i] = NULL;
    cmd->num_deps = 0;

    vk->last_cmd = cmd;
    return true;

error:
    return false;
}

const VkImageSubresourceRange vk_range = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .levelCount = 1,
    .layerCount = 1,
};

const VkImageSubresourceLayers vk_layers = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .layerCount = 1,
};
