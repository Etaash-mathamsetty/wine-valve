/*
 * WAYLANDDRV initialization code
 *
 * Copyright 2020 Alexandre Frantzis for Collabora Ltd
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

#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "waylanddrv.h"

#define IS_OPTION_TRUE(ch) \
    ((ch) == 'y' || (ch) == 'Y' || (ch) == 't' || (ch) == 'T' || (ch) == '1')

char *process_name = NULL;
BOOL option_use_system_cursors = TRUE;

static const struct user_driver_funcs waylanddrv_funcs =
{
    .pBeep = WAYLAND_Beep,
    .pClipboardWindowProc = WAYLAND_ClipboardWindowProc,
    .pClipCursor = WAYLAND_ClipCursor,
    .pDesktopWindowProc = WAYLAND_DesktopWindowProc,
    .pDestroyWindow = WAYLAND_DestroyWindow,
    .pSetIMECompositionRect = WAYLAND_SetIMECompositionRect,
    .pKbdLayerDescriptor = WAYLAND_KbdLayerDescriptor,
    .pReleaseKbdTables = WAYLAND_ReleaseKbdTables,
    .pSetCursor = WAYLAND_SetCursor,
    .pSetCursorPos = WAYLAND_SetCursorPos,
    .pSetWindowIcon = WAYLAND_SetWindowIcon,
    .pSetWindowText = WAYLAND_SetWindowText,
    .pSysCommand = WAYLAND_SysCommand,
    .pUpdateDisplayDevices = WAYLAND_UpdateDisplayDevices,
    .pWindowMessage = WAYLAND_WindowMessage,
    .pWindowPosChanged = WAYLAND_WindowPosChanged,
    .pWindowPosChanging = WAYLAND_WindowPosChanging,
    .pCreateWindowSurface = WAYLAND_CreateWindowSurface,
    .pHasWindowManager = WAYLAND_HasWindowManager,
    .pSystrayDockInit = WAYLAND_SystrayDockInit,
    .pSystrayDockInsert = WAYLAND_SystrayDockInsert,
    .pSystrayDockClear = WAYLAND_SystrayDockClear,
    .pSystrayDockRemove = WAYLAND_SystrayDockRemove,
    .pVulkanInit = WAYLAND_VulkanInit,
    .pwine_get_wgl_driver = WAYLAND_wine_get_wgl_driver,
};

static inline void ascii_to_unicode(WCHAR *dst, const char *src, size_t len)
{
    while (len--) *dst++ = (unsigned char)*src++;
}

static inline UINT asciiz_to_unicode(WCHAR *dst, const char *src)
{
    WCHAR *p = dst;
    while ((*p++ = *src++));
    return (p - dst) * sizeof(WCHAR);
}

static HKEY reg_open_key(HKEY root, const WCHAR *name, ULONG name_len)
{
    UNICODE_STRING nameW = {name_len, name_len, (WCHAR *)name};
    OBJECT_ATTRIBUTES attr;
    HANDLE ret;

    attr.Length = sizeof(attr);
    attr.RootDirectory = root;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    return NtOpenKeyEx(&ret, MAXIMUM_ALLOWED, &attr, 0) ? 0 : ret;
}

static HKEY open_hkcu_key(const char *name)
{
    WCHAR bufferW[256];
    static HKEY hkcu;

    if (!hkcu)
    {
        char buffer[256];
        DWORD_PTR sid_data[(sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE) / sizeof(DWORD_PTR)];
        DWORD i, len = sizeof(sid_data);
        SID *sid;

        if (NtQueryInformationToken(GetCurrentThreadEffectiveToken(), TokenUser, sid_data, len, &len))
            return 0;

        sid = ((TOKEN_USER *)sid_data)->User.Sid;
        len = sprintf(buffer, "\\Registry\\User\\S-%u-%u", sid->Revision,
                      MAKELONG(MAKEWORD(sid->IdentifierAuthority.Value[5],
                                        sid->IdentifierAuthority.Value[4]),
                               MAKEWORD(sid->IdentifierAuthority.Value[3],
                                        sid->IdentifierAuthority.Value[2])));
        for (i = 0; i < sid->SubAuthorityCount; i++)
            len += sprintf(buffer + len, "-%u", sid->SubAuthority[i]);

        ascii_to_unicode(bufferW, buffer, len);
        hkcu = reg_open_key(NULL, bufferW, len * sizeof(WCHAR));
    }

    return reg_open_key(hkcu, bufferW, asciiz_to_unicode(bufferW, name) - sizeof(WCHAR));
}

static ULONG query_reg_value(HKEY hkey, const WCHAR *name,
                             KEY_VALUE_PARTIAL_INFORMATION *info, ULONG size)
{
    unsigned int name_size = name ? lstrlenW(name) * sizeof(WCHAR) : 0;
    UNICODE_STRING nameW = {name_size, name_size, (WCHAR *)name};

    if (NtQueryValueKey(hkey, &nameW, KeyValuePartialInformation,
                        info, size, &size))
        return 0;

    return size - FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data);
}

static inline DWORD get_config_key(HKEY defkey, HKEY appkey, const char *name,
                                   WCHAR *buffer, DWORD size)
{
    WCHAR nameW[128];
    char buf[2048];
    KEY_VALUE_PARTIAL_INFORMATION *info = (void *)buf;

    asciiz_to_unicode(nameW, name);

    if (appkey && query_reg_value(appkey, nameW, info, sizeof(buf)))
    {
        size = min(info->DataLength, size - sizeof(WCHAR));
        memcpy(buffer, info->Data, size);
        buffer[size / sizeof(WCHAR)] = 0;
        return 0;
    }

    if (defkey && query_reg_value(defkey, nameW, info, sizeof(buf)))
    {
        size = min(info->DataLength, size - sizeof(WCHAR));
        memcpy(buffer, info->Data, size);
        buffer[size / sizeof(WCHAR)] = 0;
        return 0;
    }

    return ERROR_FILE_NOT_FOUND;
}

static void wayland_init_options(void)
{
    static const WCHAR waylanddriverW[] = {'\\','W','a','y','l','a','n','d',' ','D','r','i','v','e','r',0};
    WCHAR buffer[MAX_PATH+16], *p, *appname;
    HKEY hkey, appkey = 0;
    DWORD len;

    /* @@ Wine registry key: HKCU\Software\Wine\Wayland Driver */
    hkey = open_hkcu_key("Software\\Wine\\Wayland Driver");

    /* open the app-specific key */
    appname = NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer;
    if ((p = wcsrchr(appname, '/'))) appname = p + 1;
    if ((p = wcsrchr(appname, '\\'))) appname = p + 1;
    len = lstrlenW(appname);

    if (len && len < MAX_PATH)
    {
        HKEY tmpkey;
        int i;
        for (i = 0; appname[i]; i++) buffer[i] = RtlDowncaseUnicodeChar(appname[i]);
        buffer[i] = 0;
        appname = buffer;
        if ((process_name = malloc(len * 3 + 1)))
            ntdll_wcstoumbs(appname, len + 1, process_name, len * 3 + 1, FALSE);
        memcpy(appname + i, waylanddriverW, sizeof(waylanddriverW));
        /* @@ Wine registry key: HKCU\Software\Wine\AppDefaults\app.exe\Wayland Driver */
        if ((tmpkey = open_hkcu_key("Software\\Wine\\AppDefaults")))
        {
            appkey = reg_open_key(tmpkey, appname, lstrlenW(appname) * sizeof(WCHAR));
            NtClose(tmpkey);
        }
    }

    if (!get_config_key(hkey, appkey, "UseSystemCursors", buffer, sizeof(buffer)))
        option_use_system_cursors = IS_OPTION_TRUE(buffer[0]);

    NtClose(appkey);
    NtClose(hkey);
}

static void wayland_init_process_name(void)
{
    WCHAR *p, *appname;
    WCHAR appname_lower[MAX_PATH];
    DWORD appname_len;
    DWORD appnamez_size;
    DWORD utf8_size;
    int i;

    appname = NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer;
    if ((p = wcsrchr(appname, '/'))) appname = p + 1;
    if ((p = wcsrchr(appname, '\\'))) appname = p + 1;
    appname_len = lstrlenW(appname);

    if (appname_len == 0 || appname_len >= MAX_PATH) return;

    for (i = 0; appname[i]; i++) appname_lower[i] = RtlDowncaseUnicodeChar(appname[i]);
    appname_lower[i] = 0;

    appnamez_size = (appname_len + 1) * sizeof(WCHAR);

    if (!RtlUnicodeToUTF8N(NULL, 0, &utf8_size, appname_lower, appnamez_size) &&
        (process_name = malloc(utf8_size)))
    {
        RtlUnicodeToUTF8N(process_name, utf8_size, &utf8_size, appname_lower, appnamez_size);
    }
}

static NTSTATUS waylanddrv_unix_init(void *arg)
{
    /* Set the user driver functions now so that they are available during
     * our initialization. We clear them on error. */
    __wine_set_user_driver(&waylanddrv_funcs, WINE_GDI_DRIVER_VERSION);

    wayland_init_process_name();
    wayland_init_options();

    if (!wayland_process_init()) goto err;

    return 0;

err:
    __wine_set_user_driver(NULL, WINE_GDI_DRIVER_VERSION);
    return STATUS_UNSUCCESSFUL;
}

static NTSTATUS waylanddrv_unix_read_events(void *arg)
{
    while (wl_display_dispatch_queue(process_wayland.wl_display,
                                     process_wayland.wl_event_queue) != -1)
        continue;
    /* This function only returns on a fatal error, e.g., if our connection
     * to the Wayland server is lost. */
    return STATUS_UNSUCCESSFUL;
}

static NTSTATUS waylanddrv_unix_init_clipboard(void *arg)
{
    /* If the compositor supports zwlr_data_control_manager_v1, we don't need
     * per-process clipboard window and handling, we can use the default clipboard
     * window from the desktop process. */
    if (process_wayland.zwlr_data_control_manager_v1) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    waylanddrv_unix_init,
    waylanddrv_unix_read_events,
    waylanddrv_unix_init_clipboard,
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_funcs) == waylanddrv_unix_func_count);

#ifdef _WIN64

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    waylanddrv_unix_init,
    waylanddrv_unix_read_events,
    waylanddrv_unix_init_clipboard,
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_wow64_funcs) == waylanddrv_unix_func_count);

#endif /* _WIN64 */
