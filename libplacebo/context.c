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

#include "context.h"

#include <stdio.h>

struct pl_context *pl_context_create(int api_ver)
{
    if (api_ver != PL_API_VER) {
        fprintf(stderr,
               "*************************************************************\n"
               "libplacebo: ABI mismatch detected!\n\n"
               "This is usually indicative of a linking mismatch, and will\n"
               "result in serious issues including stack corruption, random\n"
               "crashes and arbitrary code exection. Aborting as a safety\n"
               "precaution!\n");
        abort();
    }

    struct pl_context *ctx = talloc_zero(NULL, struct pl_context);
    return ctx;
}

void pl_context_destroy(struct pl_context **ctx)
{
    TA_FREEP(ctx);
}

void pl_context_set_log_cb(struct pl_context *ctx, void *priv,
                           void (*fun)(void *priv, enum pl_log_level level,
                                       const char *msg))
{
    ctx->logfun = fun;
    ctx->logpriv = priv;
}

void pl_context_set_log_level(struct pl_context *ctx, enum pl_log_level level)
{
    ctx->loglevel = level;
}

void pl_msg(struct pl_context *ctx, enum pl_log_level lev, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    pl_msg_va(ctx, lev, fmt, va);
    va_end(va);
}

void pl_msg_va(struct pl_context *ctx, enum pl_log_level lev, const char *fmt,
               va_list va)
{
    if (!pl_msg_test(ctx, lev))
        return;

    ctx->logbuffer.len = 0;
    bstr_xappend_vasprintf(ctx, &ctx->logbuffer, fmt, va);
    ctx->logfun(ctx->logpriv, lev, ctx->logbuffer.start);
}
