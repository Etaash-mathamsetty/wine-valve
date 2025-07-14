/*
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

#include <stdarg.h>
#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winioctl.h"
#include "winbase.h"
#include "winsvc.h"
#include "winternl.h"
#include "ddk/ntifs.h"
#include "ddk/wdm.h"
#include "wine/list.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wdfldr);

typedef struct _WDF_BIND_INFO {
    BYTE unk[0x24];
} WDF_BIND_INFO, *PWDF_BIND_INFO;

typedef void *WDF_COMPONENT_GLOBALS, *PWDF_COMPONENT_GLOBALS;

static inline LPCSTR debugstr_us( const UNICODE_STRING *us )
{
    if (!us) return "<null>";
    return debugstr_wn( us->Buffer, us->Length / sizeof(WCHAR) );
}

NTSTATUS WINAPI WdfVersionBind(DRIVER_OBJECT *object, UNICODE_STRING *reg_path, WDF_BIND_INFO *info, WDF_COMPONENT_GLOBALS *globals)
{
    FIXME("%p %s %p %p stub!\n", object, debugstr_us(reg_path), info, globals);

    return STATUS_SUCCESS;
}

NTSTATUS WINAPI WdfVersionUnbind(UNICODE_STRING *reg_path, WDF_BIND_INFO *info, WDF_COMPONENT_GLOBALS *globals)
{
    FIXME("%s %p %p stub!\n", debugstr_us(reg_path), info, globals);

    return STATUS_SUCCESS;
}
