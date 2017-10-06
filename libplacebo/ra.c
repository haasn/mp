/*
 * This file is part of libplacebo.
 *
 * libplacebo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libplacebo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libplacebo. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include "public/ra.h"

bool ra_format_is_ordered(const struct ra_format *fmt)
{
    bool ret = true;
    for (int i = 0; i < fmt->num_components; i++)
        ret &= fmt->component_index[i] == i;
    return ret;
}

bool ra_format_is_regular(const struct ra_format *fmt)
{
    int bits = 0;
    for (int i = 0; i < fmt->num_components; i++) {
        if (fmt->component_index[i] != i || fmt->component_pad[i])
            return false;
        bits += fmt->component_depth[i];
    }

    return bits == fmt->texel_size * 8;
}

size_t ra_var_type_size(enum ra_var_type type)
{
    switch (type) {
    case RA_VAR_SINT:  return sizeof(int);
    case RA_VAR_UINT:  return sizeof(unsigned int);
    case RA_VAR_FLOAT: return sizeof(float);
    default: abort();
    }
}

struct ra_var_layout ra_var_host_layout(const struct ra_var var)
{
    size_t row_size = ra_var_type_size(var.type) * var.dim_v;
    return (struct ra_var_layout) {
        .align  = 1,
        .stride = row_size,
        .size   = row_size * var.dim_m,
    };
}

/* XXX: name conflict
const struct ra_tex *ra_tex_create(const struct ra *ra,
                                   const struct ra_tex_params *params)
{
    return ra->fns->tex_create(ra, params);
}

void ra_tex_destroy(const struct ra *ra, const struct ra_tex **tex)
{
    ra->fns->tex_destroy(ra, *tex);
    *tex = NULL;
}

const struct ra_buf *ra_buf_create(const struct ra *ra,
                                   const struct ra_buf_params *params)
{
    return ra->fns->buf_create(ra, params);
}

void ra_buf_destroy(const struct ra *ra, const struct ra_buf **buf)
{
    ra->fns->buf_destroy(ra, *buf);
    *buf = NULL;
}
*/

const struct ra_format *ra_find_texture_format(const struct ra *ra,
                                               enum ra_fmt_type type,
                                               int num_components,
                                               int bits_per_component,
                                               bool regular)
{
    for (int n = 0; n < ra->num_formats; n++) {
        const struct ra_format *fmt = ra->formats[n];
        if (fmt->type != type || fmt->num_components != num_components)
            continue;
        if (regular && !ra_format_is_regular(fmt))
            continue;

        for (int i = 0; i < fmt->num_components; i++) {
            if (fmt->component_depth[i] != bits_per_component)
                goto next_fmt;
        }

        return fmt;

next_fmt: ; // equivalent to `continue`
    }

    // ran out of formats
    return NULL;
}

/* XXX: name conflict
const struct ra_format *ra_find_named_format(const struct ra *ra,
                                             const char *name)
{
    if (!name)
        return NULL;

    for (int i = 0; i < ra->num_formats; i++) {
        const struct ra_format *fmt = ra->formats[i];
        if (strcmp(name, fmt->name) == 0)
            return fmt;
    }

    // ran out of formats
    return NULL;
}
*/
