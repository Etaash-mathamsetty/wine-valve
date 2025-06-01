/*
 * Wayland system tray
 *
 * Copyright 2025 Etaash Mathamsetty
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
#include <string.h>

#include "waylanddrv.h"
#include "wine/debug.h"

#ifdef SONAME_LIBDBUS_1
#include <dbus/dbus.h>
#endif

WINE_DEFAULT_DEBUG_CHANNEL(systray);


#ifdef SONAME_LIBDBUS_1


void WAYLAND_SystrayDockInit(HWND hwnd)
{
    TRACE("hwnd %p\n", hwnd);
}

void WAYLAND_SystrayDockClear(HWND hwnd)
{
    TRACE("hwnd %p\n", hwnd);
}

BOOL WAYLAND_SystrayDockInsert(HWND hwnd, UINT cx, UINT cy, void *icon)
{
    TRACE("hwnd %p (cx, cy)=(%u, %u) icon %p\n", hwnd, cx, cy, icon);

    return FALSE;
}

BOOL WAYLAND_SystrayDockRemove(HWND hwnd)
{
    TRACE("hwnd %p\n", hwnd);

    return FALSE;
}

#else

static int once;

void WAYLAND_SystrayDockInit(HWND hwnd)
{
    if (!once++)
        ERR("dbus support was not compiled in!\n");
}

void WAYLAND_SystrayDockClear(HWND hwnd)
{
    if (!once++)
        ERR("dbus support was not compiled in!\n");
}

BOOL WAYLAND_SystrayDockRemove(HWND hwnd)
{
    if (!once++)
        ERR("dbus support was not compiled in!\n");

    return FALSE;
}

BOOL WAYLAND_SystrayDockInsert(HWND hwnd, UINT cx, UINT cy, void *icon)
{
    if (!once++)
        ERR("dbus support was not compiled in!\n");

    return FALSE;
}

#endif

