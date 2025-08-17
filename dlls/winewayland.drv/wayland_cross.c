/*
 * Wayland Cross Process helpers
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
#include <unistd.h>

#include "waylanddrv.h"
#include "wine/debug.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);


int wayland_remote_call(HWND target, DWORD type, void *data, SIZE_T size)
{
    HANDLE process = NULL, section = NULL;
    LARGE_INTEGER section_size;
    SIZE_T view_size = 0;
    DWORD tid, pid;
    void *local, *other;
    int ret = 0;
    CLIENT_ID client_id = {0};

    TRACE("hwnd %p type %x data %p size %lu\n", target, type, data, size);

    tid = NtUserGetWindowThread(target, &pid);

    if (!tid) return -1;

    client_id.UniqueProcess = UlongToPtr(pid);

    if (NtOpenProcess(&process, PROCESS_ALL_ACCESS, NULL, &client_id))
    {
        ERR("Failed to open process %x\n", pid);
        return -1;
    }

    section_size.QuadPart = size;

    if (NtCreateSection(&section, GENERIC_READ | SECTION_MAP_READ | SECTION_MAP_WRITE,
                        NULL, &section_size, PAGE_READWRITE, SEC_COMMIT, 0))
    {
        ERR("Failed to create section\n");
        ret = -1;
        goto done;
    }

    if (NtMapViewOfSection(section, GetCurrentProcess(), (void *)&local, 0, 0, NULL,
                           &view_size, ViewUnmap, 0, PAGE_READWRITE))
    {
        ERR("Failed to map view of section in current process\n");
        ret = -1;
        goto done;
    }

    memcpy(local, data, size);
    NtUnmapViewOfSection(GetCurrentProcess(), local);

    if (NtMapViewOfSection(section, process, (void *)&other, 0, 0, NULL,
                           &view_size, ViewUnmap, 0, PAGE_READWRITE))
    {
        ERR("Failed to map view of section for other process\n");
        ret = -1;
        goto done;
    }

    NtUserPostMessage(target, WM_WAYLAND_REMOTE, type, (ULONG_PTR)other);

done:
    if (section) NtClose(section);
    if (process) NtClose(process);

    return ret;
}
