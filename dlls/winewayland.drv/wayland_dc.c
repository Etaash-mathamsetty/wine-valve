/*
 * Wayland DC funcs implementation
 *
 * Copyright 2025 Etaash Mathamsetty
 * Copyright 2022 Alexandros Frantzis for Collabora Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include "waylanddrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

/**********************************************************************
 *           WAYLAND_PutImage
 *
 * This is a fallback implementation for when the dibdrv cannot perform
 * this task, typically because the destination belongs to a different
 * process. In such a case the implementation utilizes the remote surface
 * infrastructure to commit content to the remote HWND.
 *
 * The implementation is very limited, supporting only simple full copies,
 * but that's enough for some typical cross-process cases, notably software
 * rendered content in Chrome/CEF.
 */
DWORD WAYLAND_PutImage(PHYSDEV dev, HRGN clip, BITMAPINFO *info,
                             const struct gdi_image_bits *bits, struct bitblt_coords *src,
                             struct bitblt_coords *dst, DWORD rop)
{
    HWND hwnd;
    DWORD ret = ERROR_SUCCESS;

    hwnd = NtUserWindowFromDC(dev->hdc);

    TRACE("hwnd=%p rop=%#x biBitCount=%d compr=%u size=%dx%d "
          "src=log=%d,%d+%dx%d:dev=%d,%d+%dx%d:vis=%s "
          "dst=log=%d,%d+%dx%d:dev=%d,%d+%dx%d:vis=%s "
          "clip=%p\n",
          hwnd, (UINT)rop, info->bmiHeader.biBitCount,
          (UINT)info->bmiHeader.biCompression,
          (int)info->bmiHeader.biWidth, (int)info->bmiHeader.biHeight,
          src->log_x, src->log_y, src->log_width, src->log_height,
          src->x, src->y, src->width, src->height,
          wine_dbgstr_rect(&src->visrect),
          dst->log_x, dst->log_y, dst->log_width, dst->log_height,
          dst->x, dst->y, dst->width, dst->height,
          wine_dbgstr_rect(&dst->visrect), clip);

    if (info->bmiHeader.biPlanes != 1)
    {
        TRACE("Multiplanar buffers not supported\n");
        goto update_format;
    }

    if (info->bmiHeader.biBitCount != 32)
    {
        TRACE("Non 32-bit buffers not supported\n");
        goto update_format;
    }

    if (info->bmiHeader.biCompression != BI_RGB)
    {
        TRACE("Non RGB not supported\n");
        goto update_format;
    }

    if (info->bmiHeader.biHeight > 0)
    {
        TRACE("Bottom-up buffers not supported\n");
        goto update_format;
    }

    if (!bits) return ERROR_SUCCESS;  /* just querying the format */

    if (!hwnd)
    {
        TRACE("Invalid hwnd=%p\n", hwnd);
        return ERROR_TRANSFORM_NOT_SUPPORTED;
    }

    if (clip)
    {
        TRACE("Clipping not supported\n");
        return ERROR_CLIPPING_NOT_SUPPORTED;
    }

    if ((src->width != dst->width) || (src->height != dst->height))
    {
        TRACE("Image stretching is not supported\n");
        return ERROR_TRANSFORM_NOT_SUPPORTED;
    }

    if ((src->width != info->bmiHeader.biWidth) ||
        (src->height != -info->bmiHeader.biHeight))
    {
        TRACE("Partial blits are not supported\n");
        return ERROR_TRANSFORM_NOT_SUPPORTED;
    }

    if (rop != SRCCOPY)
    {
        TRACE("Raster operations other than SRCCOPY are not supported\n");
        return ERROR_INVALID_OPERATION;
    }

    return ret;

update_format:
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    if (info->bmiHeader.biHeight > 0) info->bmiHeader.biHeight = -info->bmiHeader.biHeight;
    return ERROR_BAD_FORMAT;
}
