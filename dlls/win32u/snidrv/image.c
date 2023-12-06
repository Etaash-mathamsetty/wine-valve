/*
 * DBus tray support
 *
 * Copyright 2023 Sergei Chernyadyev
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
#ifdef SONAME_LIBDBUS_1

#include "snidrv.h"
#include "stdlib.h"
#include "ntuser.h"
#include "ntgdi.h"

/***********************************************************************
 *           get_mono_icon_argb
 *
 * Return a monochrome icon/cursor bitmap bits in ARGB format.
 */
static unsigned int *get_mono_icon_argb( HDC hdc, HBITMAP bmp, unsigned int *width, unsigned int *height )
{
    BITMAP bm;
    char *mask;
    unsigned int i, j, stride, mask_size, bits_size, *bits = NULL, *ptr;

    if (!NtGdiExtGetObjectW( bmp, sizeof(bm), &bm )) return NULL;
    stride = ((bm.bmWidth + 15) >> 3) & ~1;
    mask_size = stride * bm.bmHeight;
    if (!(mask = malloc( mask_size ))) return NULL;
    if (!NtGdiGetBitmapBits( bmp, mask_size, mask )) goto done;

    bm.bmHeight /= 2;
    bits_size = bm.bmWidth * bm.bmHeight * sizeof(*bits);
    if (!(bits = malloc( bits_size ))) goto done;

    ptr = bits;
    for (i = 0; i < bm.bmHeight; i++)
        for (j = 0; j < bm.bmWidth; j++, ptr++)
        {
            int and = ((mask[i * stride + j / 8] << (j % 8)) & 0x80);
            int xor = ((mask[(i + bm.bmHeight) * stride + j / 8] << (j % 8)) & 0x80);
            if (!xor && and)
                *ptr = 0;
            else if (xor && !and)
                *ptr = 0xffffffff;
            else
                /* we can't draw "invert" pixels, so render them as black instead */
#ifdef WORDS_BIGENDIAN
                *ptr = 0xff000000;
#else
                *ptr = 0x000000ff;
#endif
        }

    *width = bm.bmWidth;
    *height = bm.bmHeight;

done:
    free( mask );
    return bits;
}

/***********************************************************************
 *           get_bitmap_argb
 *
 * Return the bitmap bits in ARGB format. Helper for setting icons and cursors.
 */
static unsigned int *get_bitmap_argb( HDC hdc, HBITMAP color, HBITMAP mask, unsigned int *width,
                                      unsigned int *height )
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    BITMAP bm;
    unsigned int *ptr, *bits = NULL;
    unsigned char *mask_bits = NULL;
    int i, j;
    BOOL has_alpha = FALSE;

    if (!color) return get_mono_icon_argb( hdc, mask, width, height );

    if (!NtGdiExtGetObjectW( color, sizeof(bm), &bm )) return NULL;
    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = bm.bmWidth;
    info->bmiHeader.biHeight = -bm.bmHeight;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = bm.bmWidth * bm.bmHeight * 4;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;
    if (!(bits = malloc( bm.bmWidth * bm.bmHeight * sizeof(unsigned int) )))
        goto failed;
    if (!NtGdiGetDIBitsInternal( hdc, color, 0, bm.bmHeight, bits, info, DIB_RGB_COLORS, 0, 0 ))
        goto failed;

    *width = bm.bmWidth;
    *height = bm.bmHeight;

    for (i = 0; i < bm.bmWidth * bm.bmHeight; i++)
        if ((has_alpha = (bits[i] & 0xff000000) != 0)) break;

    if (!has_alpha)
    {
        unsigned int width_bytes = (bm.bmWidth + 31) / 32 * 4;
        /* generate alpha channel from the mask */
        info->bmiHeader.biBitCount = 1;
        info->bmiHeader.biSizeImage = width_bytes * bm.bmHeight;
        if (!(mask_bits = malloc( info->bmiHeader.biSizeImage ))) goto failed;
        if (!NtGdiGetDIBitsInternal( hdc, mask, 0, bm.bmHeight, mask_bits, info, DIB_RGB_COLORS, 0, 0 ))
            goto failed;
        ptr = bits;
        for (i = 0; i < bm.bmHeight; i++)
            for (j = 0; j < bm.bmWidth; j++, ptr++)
                if (!((mask_bits[i * width_bytes + j / 8] << (j % 8)) & 0x80)) *ptr |= 0xff000000;
        free( mask_bits );
    }
#ifndef WORDS_BIGENDIAN
    for (unsigned i = 0; i < bm.bmWidth * bm.bmHeight; i++)
    {
        bits[i] = ((bits[i] & 0xFF) << 24) | ((bits[i] & 0xFF00) << 8) | ((bits[i] & 0xFF0000) >> 8) | ((bits[i] & 0xFF000000) >> 24);
    }
#endif

    return bits;

failed:
    free( bits );
    free( mask_bits );
    *width = *height = 0;
    return NULL;
}


BOOL create_bitmap_from_icon(HANDLE icon, unsigned *p_width, unsigned *p_height, void** p_bits)
{
    ICONINFO info;
    HDC hdc;

    if (!NtUserGetIconInfo(icon, &info, NULL, NULL, NULL, 0))
        return FALSE;

    hdc = NtGdiCreateCompatibleDC( 0 );
    *p_bits = get_bitmap_argb( hdc, info.hbmColor, info.hbmMask, p_width, p_height );
    NtGdiDeleteObjectApp( info.hbmMask );
    NtGdiDeleteObjectApp( hdc );
    return *p_bits != NULL;
}

#endif
