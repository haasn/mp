#include "common/msg.h"
#include "video/out/vo.h"
#include "utils.h"

// Standard parallel 2D projection, except y1 < y0 means that the coordinate
// system is flipped, not the projection.
void gl_transform_ortho(struct gl_transform *t, float x0, float x1,
                        float y0, float y1)
{
    if (y1 < y0) {
        float tmp = y0;
        y0 = tmp - y1;
        y1 = tmp;
    }

    t->m[0][0] = 2.0f / (x1 - x0);
    t->m[0][1] = 0.0f;
    t->m[1][0] = 0.0f;
    t->m[1][1] = 2.0f / (y1 - y0);
    t->t[0] = -(x1 + x0) / (x1 - x0);
    t->t[1] = -(y1 + y0) / (y1 - y0);
}

// Apply the effects of one transformation to another, transforming it in the
// process. In other words: post-composes t onto x
void gl_transform_trans(struct gl_transform t, struct gl_transform *x)
{
    struct gl_transform xt = *x;
    x->m[0][0] = t.m[0][0] * xt.m[0][0] + t.m[0][1] * xt.m[1][0];
    x->m[1][0] = t.m[1][0] * xt.m[0][0] + t.m[1][1] * xt.m[1][0];
    x->m[0][1] = t.m[0][0] * xt.m[0][1] + t.m[0][1] * xt.m[1][1];
    x->m[1][1] = t.m[1][0] * xt.m[0][1] + t.m[1][1] * xt.m[1][1];
    gl_transform_vec(t, &x->t[0], &x->t[1]);
}

void gl_transform_ortho_fbo(struct gl_transform *t, struct ra_fbo fbo)
{
    int y_dir = fbo.flip ? -1 : 1;
    gl_transform_ortho(t, 0, fbo.tex->params.w, 0, fbo.tex->params.h * y_dir);
}

void ra_buf_pool_uninit(struct ra *ra, struct ra_buf_pool *pool)
{
    for (int i = 0; i < pool->num_buffers; i++)
        ra_buf_free(ra, &pool->buffers[i]);

    talloc_free(pool->buffers);
    *pool = (struct ra_buf_pool){0};
}

static bool ra_buf_params_compatible(const struct ra_buf_params *new,
                                     const struct ra_buf_params *old)
{
    return new->type == old->type &&
           new->size <= old->size &&
           new->host_mapped  == old->host_mapped &&
           new->host_mutable == old->host_mutable;
}

static bool ra_buf_pool_grow(struct ra *ra, struct ra_buf_pool *pool)
{
    struct ra_buf *buf = ra_buf_create(ra, &pool->current_params);
    if (!buf)
        return false;

    MP_TARRAY_INSERT_AT(NULL, pool->buffers, pool->num_buffers, pool->index, buf);
    MP_VERBOSE(ra, "Resized buffer pool of type %u to size %d\n",
               pool->current_params.type, pool->num_buffers);
    return true;
}

struct ra_buf *ra_buf_pool_get(struct ra *ra, struct ra_buf_pool *pool,
                               const struct ra_buf_params *params)
{
    assert(!params->initial_data);

    if (!ra_buf_params_compatible(params, &pool->current_params)) {
        ra_buf_pool_uninit(ra, pool);
        pool->current_params = *params;
    }

    // Make sure we have at least one buffer available
    if (!pool->buffers && !ra_buf_pool_grow(ra, pool))
        return NULL;

    // Make sure the next buffer is available for use
    if (!ra->fns->buf_poll(ra, pool->buffers[pool->index]) &&
        !ra_buf_pool_grow(ra, pool))
    {
        return NULL;
    }

    struct ra_buf *buf = pool->buffers[pool->index++];
    pool->index %= pool->num_buffers;

    return buf;
}

bool ra_tex_upload_pbo(struct ra *ra, struct ra_buf_pool *pbo,
                       const struct ra_tex_upload_params *params)
{
    if (params->buf)
        return ra->fns->tex_upload(ra, params);

    struct ra_tex *tex = params->tex;
    size_t row_size = tex->params.dimensions == 2 ? params->stride :
                      tex->params.w * tex->params.format->pixel_size;

    struct ra_buf_params bufparams = {
        .type = RA_BUF_TYPE_TEX_UPLOAD,
        .size = row_size * tex->params.h * tex->params.d,
        .host_mutable = true,
    };

    struct ra_buf *buf = ra_buf_pool_get(ra, pbo, &bufparams);
    if (!buf)
        return false;

    ra->fns->buf_update(ra, buf, 0, params->src, bufparams.size);

    struct ra_tex_upload_params newparams = *params;
    newparams.buf = buf;
    newparams.src = NULL;

    return ra->fns->tex_upload(ra, &newparams);
}

struct ra_layout std140_layout(struct ra_renderpass_input *inp)
{
    size_t el_size = ra_vartype_size(inp->type);

    // std140 packing rules:
    // 1. The alignment of generic values is their size in bytes
    // 2. The alignment of vectors is the vector length * the base count, with
    // the exception of vec3 which is always aligned like vec4
    // 3. The alignment of arrays is that of the element size rounded up to
    // the nearest multiple of vec4
    // 4. Matrices are treated like arrays of vectors
    // 5. Arrays/matrices are laid out with a stride equal to the alignment
    size_t size = el_size * inp->dim_v;
    if (inp->dim_v == 3)
        size += el_size;
    if (inp->dim_m > 1)
        size = MP_ALIGN_UP(size, sizeof(float[4]));

    return (struct ra_layout) {
        .align  = size,
        .stride = size,
        .size   = size * inp->dim_m,
    };
}

struct ra_layout std430_layout(struct ra_renderpass_input *inp)
{
    size_t el_size = ra_vartype_size(inp->type);

    // std430 packing rules: like std140, except arrays/matrices are always
    // "tightly" packed, even arrays/matrices of vec3s
    size_t size = el_size * inp->dim_v;
    if (inp->dim_v == 3 && inp->dim_m == 1)
        size += el_size;

    return (struct ra_layout) {
        .align  = size,
        .stride = size,
        .size   = size * inp->dim_m,
    };
}

#define RA_TEX_ENTRY_MAX_AGE 10

struct ra_tex_entry {
    struct ra_tex_pool *pool;
    struct ra_tex_ref ref; // ref.priv points at this entry
    int age;  // for garbage collection
    int refs; // for refcounting
};

struct ra_tex_pool {
    struct ra *ra;
    const struct ra_format *fmt;
    // Textures currently in the pool
    struct ra_tex_entry **avail;
    int num_avail;
};

static void ra_tex_pool_uninit(struct ra_tex_pool *pool)
{
    for (int i = 0; i < pool->num_avail; i++) {
        ra_tex_free(pool->ra, &pool->avail[i]->ref.tex);
        talloc_free(pool->avail[i]);
    }

    talloc_free(pool);
}

void ra_tex_pool_free(struct ra_tex_pool **pool)
{
    if (*pool)
        ra_tex_pool_uninit(*pool);
    *pool = NULL;
}

void ra_tex_pool_gc_tick(struct ra_tex_pool *pool)
{
    for (int i = 0; i < pool->num_avail; i++) {
        struct ra_tex_entry *entry = pool->avail[i];
        if (++entry->age > RA_TEX_ENTRY_MAX_AGE) {
            MP_VERBOSE(pool->ra, "Freeing %dx%d texture due to old age.\n",
                       entry->ref.tex->params.w, entry->ref.tex->params.h);
            ra_tex_free(pool->ra, &entry->ref.tex);
            talloc_free(entry);
            // Remove from the array and repeat this loop iteration
            MP_TARRAY_REMOVE_AT(pool->avail, pool->num_avail, i--);
        }
    }
}

struct ra_tex_pool *ra_tex_pool_alloc(struct ra *ra, const struct ra_format *fmt)
{
    if (!fmt || !fmt->renderable || !fmt->linear_filter)
        return NULL;

    struct ra_tex_pool *pool = talloc_ptrtype(NULL, pool);
    *pool = (struct ra_tex_pool) {
        .ra = ra,
        .fmt = fmt,
    };

    return pool;
}

struct ra_tex_ref *ra_tex_pool_get(struct ra_tex_pool *pool, int w, int h)
{
    for (int i = 0; i < pool->num_avail; i++) {
        struct ra_tex_entry *entry = pool->avail[i];
        if (entry->ref.tex->params.w == w && entry->ref.tex->params.h == h) {
            MP_TARRAY_REMOVE_AT(pool->avail, pool->num_avail, i);
            entry->refs = 1;
            return &entry->ref;
        }
    }

    // No new image => allocate one
    struct ra_tex_params params = {
        .dimensions = 2,
        .w = w,
        .h = h,
        .d = 1,
        .format = pool->fmt,
        .src_linear = true,
        .render_src = true,
        .render_dst = true,
        .storage_dst = true,
        .blit_src = true,
    };

    MP_VERBOSE(pool->ra, "Creating new %dx%d texture.\n", w, h);
    struct ra_tex *tex = ra_tex_create(pool->ra, &params);
    if (!tex) {
        MP_FATAL(pool->ra, "Could not create texture!\n");
        abort(); // OOM
    }

    struct ra_tex_entry *entry = talloc_zero(NULL, struct ra_tex_entry);
    entry->pool = pool;
    entry->refs = 1;
    entry->ref.tex = tex;
    entry->ref.priv = entry;

    return &entry->ref;
}

struct ra_tex_ref *ra_tex_ref_dup(struct ra_tex_ref *ref)
{
    if (!ref)
        return NULL;

    struct ra_tex_entry *entry = ref->priv;
    assert(entry->refs > 0);
    entry->refs++;
    return ref;
}

void ra_tex_ref_free(struct ra_tex_ref **ref)
{
    if (!*ref)
        return;

    struct ra_tex_entry *entry = (*ref)->priv;
    struct ra_tex_pool *pool = entry->pool;

    assert(entry->refs > 0);
    if (--entry->refs == 0) {
        entry->age = 0;
        pool->ra->fns->tex_invalidate(pool->ra, entry->ref.tex);
        MP_TARRAY_APPEND(pool, pool->avail, pool->num_avail, entry);
    }

    *ref = NULL;
}

struct timer_pool {
    struct ra *ra;
    ra_timer *timer;
    bool running; // detect invalid usage

    uint64_t samples[VO_PERF_SAMPLE_COUNT];
    int sample_idx;
    int sample_count;

    uint64_t sum;
    uint64_t peak;
};

struct timer_pool *timer_pool_create(struct ra *ra)
{
    if (!ra->fns->timer_create)
        return NULL;

    ra_timer *timer = ra->fns->timer_create(ra);
    if (!timer)
        return NULL;

    struct timer_pool *pool = talloc(NULL, struct timer_pool);
    if (!pool) {
        ra->fns->timer_destroy(ra, timer);
        return NULL;
    }

    *pool = (struct timer_pool){ .ra = ra, .timer = timer };
    return pool;
}

void timer_pool_destroy(struct timer_pool *pool)
{
    if (!pool)
        return;

    pool->ra->fns->timer_destroy(pool->ra, pool->timer);
    talloc_free(pool);
}

void timer_pool_start(struct timer_pool *pool)
{
    if (!pool)
        return;

    assert(!pool->running);
    pool->ra->fns->timer_start(pool->ra, pool->timer);
    pool->running = true;
}

void timer_pool_stop(struct timer_pool *pool)
{
    if (!pool)
        return;

    assert(pool->running);
    uint64_t res = pool->ra->fns->timer_stop(pool->ra, pool->timer);
    pool->running = false;

    if (res) {
        // Input res into the buffer and grab the previous value
        uint64_t old = pool->samples[pool->sample_idx];
        pool->sample_count = MPMIN(pool->sample_count + 1, VO_PERF_SAMPLE_COUNT);
        pool->samples[pool->sample_idx++] = res;
        pool->sample_idx %= VO_PERF_SAMPLE_COUNT;
        pool->sum = pool->sum + res - old;

        // Update peak if necessary
        if (res >= pool->peak) {
            pool->peak = res;
        } else if (pool->peak == old) {
            // It's possible that the last peak was the value we just removed,
            // if so we need to scan for the new peak
            uint64_t peak = res;
            for (int i = 0; i < VO_PERF_SAMPLE_COUNT; i++)
                peak = MPMAX(peak, pool->samples[i]);
            pool->peak = peak;
        }
    }
}

struct mp_pass_perf timer_pool_measure(struct timer_pool *pool)
{
    if (!pool)
        return (struct mp_pass_perf){0};

    struct mp_pass_perf res = {
        .peak = pool->peak,
        .count = pool->sample_count,
    };

    int idx = pool->sample_idx - pool->sample_count + VO_PERF_SAMPLE_COUNT;
    for (int i = 0; i < res.count; i++) {
        idx %= VO_PERF_SAMPLE_COUNT;
        res.samples[i] = pool->samples[idx++];
    }

    if (res.count > 0) {
        res.last = res.samples[res.count - 1];
        res.avg = pool->sum / res.count;
    }

    return res;
}

void mp_log_source(struct mp_log *log, int lev, const char *src)
{
    int line = 1;
    if (!src)
        return;
    while (*src) {
        const char *end = strchr(src, '\n');
        const char *next = end + 1;
        if (!end)
            next = end = src + strlen(src);
        mp_msg(log, lev, "[%3d] %.*s\n", line, (int)(end - src), src);
        line++;
        src = next;
    }
}
