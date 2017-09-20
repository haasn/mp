/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "options/m_config.h"
#include "video/out/gpu/spirv.h"

#include "context.h"
#include "ra_vk.h"
#include "utils.h"

enum {
    SWAP_AUTO = 0,
    SWAP_FIFO,
    SWAP_FIFO_RELAXED,
    SWAP_MAILBOX,
    SWAP_IMMEDIATE,
    SWAP_COUNT,
};

struct vulkan_opts {
    char *device; // force a specific GPU
    int swap_mode;
};

static int vk_validate_dev(struct mp_log *log, const struct m_option *opt,
                           struct bstr name, struct bstr param)
{
    int ret = M_OPT_INVALID;
    VkResult res;

    // Create a dummy instance to validate/list the devices
    VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    };

    VkInstance inst;
    VkPhysicalDevice *devices = NULL;
    uint32_t num = 0;

    res = vkCreateInstance(&info, MPVK_ALLOCATOR, &inst);
    if (res != VK_SUCCESS)
        goto error;

    res = vkEnumeratePhysicalDevices(inst, &num, NULL);
    if (res != VK_SUCCESS)
        goto error;

    devices = talloc_array(NULL, VkPhysicalDevice, num);
    vkEnumeratePhysicalDevices(inst, &num, devices);
    if (res != VK_SUCCESS)
        goto error;

    bool help = bstr_equals0(param, "help");
    if (help) {
        mp_info(log, "Available vulkan devices:\n");
        ret = M_OPT_EXIT;
    }

    for (int i = 0; i < num; i++) {
        VkPhysicalDeviceProperties prop;
        vkGetPhysicalDeviceProperties(devices[i], &prop);

        if (help) {
            mp_info(log, "  '%s' (GPU %d, ID %x:%x)\n", prop.deviceName, i,
                    prop.vendorID, prop.deviceID);
        } else if (bstr_equals0(param, prop.deviceName)) {
            ret = 0;
            break;
        }
    }

    if (!help)
        mp_err(log, "No device with name '%.*s'!\n", BSTR_P(param));

error:
    talloc_free(devices);
    return ret;
}

#define OPT_BASE_STRUCT struct vulkan_opts
const struct m_sub_options vulkan_conf = {
    .opts = (const struct m_option[]) {
        OPT_STRING_VALIDATE("vulkan-device", device, 0, vk_validate_dev),
        OPT_CHOICE("vulkan-swap-mode", swap_mode, 0,
                   ({"auto",        SWAP_AUTO},
                   {"fifo",         SWAP_FIFO},
                   {"fifo-relaxed", SWAP_FIFO_RELAXED},
                   {"mailbox",      SWAP_MAILBOX},
                   {"immediate",    SWAP_IMMEDIATE})),
        {0}
    },
    .size = sizeof(struct vulkan_opts)
};

struct priv {
    struct mpvk_ctx *vk;
    struct vulkan_opts *opts;
    // Swapchain metadata:
    int w, h;                 // current size
    VkSwapchainCreateInfoKHR protoInfo; // partially filled-in prototype
    VkSwapchainKHR swchain;
    int frames_in_flight;
    int swchain_depth;
    // state of the images:
    struct ra_tex **images;   // ra_tex wrappers for the vkimages
    int num_images;           // size of images
    VkSemaphore *acquired;    // pool of semaphores used to synchronize images
    int num_acquired;         // size of this pool
    int idx_acquired;         // index of next free semaphore within this pool
    int last_imgidx;          // the image index last acquired (for submit)
};

struct mpvk_ctx *ra_vk_ctx_get_vk(struct ra_ctx *ctx)
{
    struct priv *p = ctx->swchain->priv;
    return p->vk;
}

static bool update_swchain_info(struct priv *p,
                                VkSwapchainCreateInfoKHR *info)
{
    struct mpvk_ctx *vk = p->vk;

    // Query the supported capabilities and update this struct as needed
    VkSurfaceCapabilitiesKHR caps;
    VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physd, vk->surf, &caps));

    // Sorted by preference
    static const VkCompositeAlphaFlagBitsKHR alphaModes[] = {
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    };

    for (int i = 0; i < MP_ARRAY_SIZE(alphaModes); i++) {
        if (caps.supportedCompositeAlpha & alphaModes[i]) {
            info->compositeAlpha = alphaModes[i];
            break;
        }
    }

    if (!info->compositeAlpha) {
        MP_ERR(vk, "Failed picking alpha compositing mode (caps: %d)\n",
               caps.supportedCompositeAlpha);
        goto error;
    }

    static const VkSurfaceTransformFlagBitsKHR rotModes[] = {
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR,
    };

    for (int i = 0; i < MP_ARRAY_SIZE(rotModes); i++) {
        if (caps.supportedTransforms & rotModes[i]) {
            info->preTransform = rotModes[i];
            break;
        }
    }

    if (!info->preTransform) {
        MP_ERR(vk, "Failed picking surface transform mode (caps: %d)\n",
               caps.supportedTransforms);
        goto error;
    }

    // Image count as required. The +1 is for the framebuffer
    int req_count = p->swchain_depth + 1;
    MP_VERBOSE(vk, "Requested image count: %d (min %d max %d)\n",
               req_count, caps.minImageCount, caps.maxImageCount);
    info->minImageCount = MPMAX(req_count, caps.minImageCount);
    if (caps.maxImageCount)
        info->minImageCount = MPMIN(info->minImageCount, caps.maxImageCount);

    // Check the extent against the allowed parameters
    if (caps.currentExtent.width != info->imageExtent.width &&
        caps.currentExtent.width != 0xFFFFFFFF)
    {
        MP_WARN(vk, "Requested width %d does not match current width %d\n",
               info->imageExtent.width, caps.currentExtent.width);
        info->imageExtent.width = caps.currentExtent.width;
    }

    if (caps.currentExtent.height != info->imageExtent.height &&
        caps.currentExtent.height != 0xFFFFFFFF)
    {
        MP_WARN(vk, "Requested height %d does not match current height %d\n",
               info->imageExtent.height, caps.currentExtent.height);
        info->imageExtent.height = caps.currentExtent.height;
    }

    if (caps.minImageExtent.width  > info->imageExtent.width ||
        caps.minImageExtent.height > info->imageExtent.height)
    {
        MP_ERR(vk, "Requested size %dx%d smaller than device minimum %d%d\n",
               info->imageExtent.width, info->imageExtent.height,
               caps.minImageExtent.width, caps.minImageExtent.height);
        goto error;
    }

    if (caps.maxImageExtent.width  < info->imageExtent.width ||
        caps.maxImageExtent.height < info->imageExtent.height)
    {
        MP_ERR(vk, "Requested size %dx%d larger than device maximum %d%d\n",
               info->imageExtent.width, info->imageExtent.height,
               caps.maxImageExtent.width, caps.maxImageExtent.height);
        goto error;
    }

    // We just request whatever usage we can, and let the ra_vk decide what
    // ra_tex_params that translates to. This makes the images as flexible
    // as possible.
    info->imageUsage = caps.supportedUsageFlags;
    return true;

error:
    return false;
}

void ra_vk_ctx_uninit(struct ra_ctx *ctx)
{
    if (ctx->ra) {
        struct priv *p = ctx->swchain->priv;
        struct mpvk_ctx *vk = p->vk;

        mpvk_pool_wait_idle(vk, vk->pool);

        for (int i = 0; i < p->num_images; i++)
            ra_tex_free(ctx->ra, &p->images[i]);
        for (int i = 0; i < p->num_acquired; i++)
            vkDestroySemaphore(vk->dev, p->acquired[i], MPVK_ALLOCATOR);

        vkDestroySwapchainKHR(vk->dev, p->swchain, MPVK_ALLOCATOR);

        talloc_free(p->images);
        talloc_free(p->acquired);
        ctx->ra->fns->destroy(ctx->ra);
        ctx->ra = NULL;
    }

    talloc_free(ctx->swchain);
    ctx->swchain = NULL;
}

static const struct ra_swchain_fns vulkan_swchain;

bool ra_vk_ctx_init(struct ra_ctx *ctx, struct mpvk_ctx *vk,
                    VkPresentModeKHR preferred_mode)
{
    struct ra_swchain *sw = ctx->swchain = talloc_zero(NULL, struct ra_swchain);
    sw->ctx = ctx;
    sw->fns = &vulkan_swchain;

    struct priv *p = sw->priv = talloc_zero(sw, struct priv);
    p->vk = vk;
    p->opts = mp_get_config_group(p, ctx->global, &vulkan_conf);
    p->swchain_depth = ctx->opts.swchain_depth;

    if (!mpvk_find_phys_device(vk, p->opts->device, ctx->opts.allow_sw))
        goto error;
    if (!spirv_compiler_init(ctx))
        goto error;
    if (!mpvk_pick_surface_format(vk))
        goto error;
    if (!mpvk_device_init(vk, ctx->opts.swchain_depth))
        goto error;

    ctx->ra = ra_create_vk(vk, ctx->log, ctx->spirv);
    if (!ctx->ra)
        goto error;

    static const VkPresentModeKHR present_modes[SWAP_COUNT] = {
        [SWAP_FIFO]         = VK_PRESENT_MODE_FIFO_KHR,
        [SWAP_FIFO_RELAXED] = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        [SWAP_MAILBOX]      = VK_PRESENT_MODE_MAILBOX_KHR,
        [SWAP_IMMEDIATE]    = VK_PRESENT_MODE_IMMEDIATE_KHR,
    };

    p->protoInfo = (VkSwapchainCreateInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk->surf,
        .imageFormat = vk->surf_format.format,
        .imageColorSpace = vk->surf_format.colorSpace,
        .imageArrayLayers = 1, // non-stereoscopic
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .presentMode = p->opts->swap_mode ? present_modes[p->opts->swap_mode]
                                          : preferred_mode,
        .clipped = true,
    };

    // Make sure the swapchain present mode is supported
    int num_modes;
    VK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physd, vk->surf,
                                                 &num_modes, NULL));
    VkPresentModeKHR *modes = talloc_array(NULL, VkPresentModeKHR, num_modes);
    VK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physd, vk->surf,
                                                 &num_modes, modes));
    bool supported = false;
    for (int i = 0; i < num_modes; i++)
        supported |= (modes[i] == p->protoInfo.presentMode);
    talloc_free(modes);

    if (!supported) {
        MP_ERR(ctx, "Requested swap mode unsupported by this device!\n");
        goto error;
    }

    return true;

error:
    ra_vk_ctx_uninit(ctx);
    return false;
}

static void destroy_swapchain(struct mpvk_ctx *vk, VkSwapchainKHR swchain)
{
    vkDestroySwapchainKHR(vk->dev, swchain, MPVK_ALLOCATOR);
}

bool ra_vk_ctx_resize(struct ra_swchain *sw, int w, int h)
{
    struct priv *p = sw->priv;
    if (w == p->w && h == p->h)
        return true;

    struct ra *ra = sw->ctx->ra;
    struct mpvk_ctx *vk = p->vk;
    VkImage *vkimages = NULL;
    bool ret = false;

    VkSwapchainCreateInfoKHR sinfo = p->protoInfo;
    sinfo.imageExtent  = (VkExtent2D){ w, h };
    sinfo.oldSwapchain = p->swchain;

    if (!update_swchain_info(p, &sinfo))
        goto error;

    VK(vkCreateSwapchainKHR(vk->dev, &sinfo, MPVK_ALLOCATOR, &p->swchain));
    p->w = w;
    p->h = h;

    // Freeing the old swapchain while it's still in use is an error, so do
    // it asynchronously once the device is idle.
    if (sinfo.oldSwapchain)
        vk_dev_callback(vk, (vk_cb) destroy_swapchain, vk, sinfo.oldSwapchain);

    // Get the new swapchain images
    int num;
    VK(vkGetSwapchainImagesKHR(vk->dev, p->swchain, &num, NULL));
    vkimages = talloc_array(NULL, VkImage, num);
    VK(vkGetSwapchainImagesKHR(vk->dev, p->swchain, &num, vkimages));

    // If needed, allocate some more semaphores
    while (num > p->num_acquired) {
        VkSemaphore sem;
        static const VkSemaphoreCreateInfo seminfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VK(vkCreateSemaphore(vk->dev, &seminfo, MPVK_ALLOCATOR, &sem));
        MP_TARRAY_APPEND(NULL, p->acquired, p->num_acquired, sem);
    }

    // Recreate the ra_tex wrappers
    for (int i = 0; i < p->num_images; i++)
        ra_tex_free(ra, &p->images[i]);

    p->num_images = num;
    MP_TARRAY_GROW(NULL, p->images, p->num_images);
    for (int i = 0; i < num; i++) {
        p->images[i] = ra_vk_wrap_swchain_img(ra, vkimages[i], sinfo);
        if (!p->images[i])
            goto error;
    }

    ret = true;

error:
    talloc_free(vkimages);
    return ret;
}

static void update_length(struct ra_swchain *sw, int depth)
{
    struct priv *p = sw->priv;
    p->swchain_depth = depth;
}

static int color_depth(struct ra_swchain *sw)
{
    struct priv *p = sw->priv;
    int bits = 0;

    if (!p->num_images)
        return bits;

    // The channel with the most bits is probably the most authoritative about
    // the actual color information (consider e.g. a2bgr10). Slight downside
    // in that it results in rounding r/b for e.g. rgb565, but we don't pick
    // surfaces with fewer than 8 bits anyway.
    const struct ra_format *fmt = p->images[0]->params.format;
    for (int i = 0; i < fmt->num_components; i++) {
        int depth = fmt->component_depth[i];
        bits = MPMAX(bits, depth ? depth : fmt->component_size[i]);
    }

    return bits;
}

static bool start_frame(struct ra_swchain *sw, struct ra_fbo *out_fbo)
{
    struct priv *p = sw->priv;
    struct mpvk_ctx *vk = p->vk;

    uint32_t imgidx = 0;
    MP_TRACE(vk, "vkAcquireNextImageKHR\n");
    VkResult res = vkAcquireNextImageKHR(vk->dev, p->swchain, UINT64_MAX,
                                         p->acquired[p->idx_acquired], NULL,
                                         &imgidx);
    if (res == VK_ERROR_OUT_OF_DATE_KHR)
        goto error; // just return in this case
    VK_ASSERT(res, "Failed acquiring swapchain image");

    p->last_imgidx = imgidx;
    *out_fbo = (struct ra_fbo){ p->images[imgidx] };
    return true;

error:
    return false;
}

static bool submit_frame(struct ra_swchain *sw, const struct vo_frame *frame)
{
    struct priv *p = sw->priv;
    struct ra *ra = sw->ctx->ra;

    VkSemaphore acquired = p->acquired[p->idx_acquired++];
    p->idx_acquired %= p->num_acquired;

    return ra_vk_present_frame(ra, p->images[p->last_imgidx], acquired,
                               p->swchain, p->last_imgidx, &p->frames_in_flight);
}

static void swap_buffers(struct ra_swchain *sw)
{
    struct priv *p = sw->priv;

    while (p->frames_in_flight >= p->swchain_depth)
        mpvk_dev_poll_cmds(p->vk, 100000); // 100 μs
}

static const struct ra_swchain_fns vulkan_swchain = {
    // .screenshot is not currently supported
    .update_length = update_length,
    .color_depth   = color_depth,
    .start_frame   = start_frame,
    .submit_frame  = submit_frame,
    .swap_buffers  = swap_buffers,
};
