/* WinRT Windows.Gaming.Input implementation
 *
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#include "private.h"

#include "initguid.h"
#include "dinput.h"
#include "provider.h"

#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(input);

DEFINE_GUID( device_path_guid, 0x00000000, 0x0000, 0x0000, 0x8d, 0x4a, 0x23, 0x90, 0x3f, 0xb6, 0xbd, 0xf8 );

static CRITICAL_SECTION provider_cs;
static CRITICAL_SECTION_DEBUG provider_cs_debug =
{
    0, 0, &provider_cs,
    { &provider_cs_debug.ProcessLocksList, &provider_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": provider_cs") }
};
static CRITICAL_SECTION provider_cs = { &provider_cs_debug, -1, 0, 0, 0, 0 };

static struct list provider_list = LIST_INIT( provider_list );

struct provider
{
    IWineGameControllerProvider IWineGameControllerProvider_iface;
    IGameControllerProvider IGameControllerProvider_iface;
    LONG ref;

    IDirectInputDevice8W *dinput_device;
    WCHAR device_path[MAX_PATH];
    struct list entry;
};

static inline struct provider *impl_from_IWineGameControllerProvider( IWineGameControllerProvider *iface )
{
    return CONTAINING_RECORD( iface, struct provider, IWineGameControllerProvider_iface );
}

static HRESULT WINAPI wine_provider_QueryInterface( IWineGameControllerProvider *iface, REFIID iid, void **out )
{
    struct provider *impl = impl_from_IWineGameControllerProvider( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IWineGameControllerProvider ))
    {
        IInspectable_AddRef( (*out = &impl->IWineGameControllerProvider_iface) );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IGameControllerProvider ))
    {
        IInspectable_AddRef( (*out = &impl->IGameControllerProvider_iface) );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI wine_provider_AddRef( IWineGameControllerProvider *iface )
{
    struct provider *impl = impl_from_IWineGameControllerProvider( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI wine_provider_Release( IWineGameControllerProvider *iface )
{
    struct provider *impl = impl_from_IWineGameControllerProvider( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );

    if (!ref)
    {
        IDirectInputDevice8_Release( impl->dinput_device );
        free( impl );
    }

    return ref;
}

static HRESULT WINAPI wine_provider_GetIids( IWineGameControllerProvider *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI wine_provider_GetRuntimeClassName( IWineGameControllerProvider *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI wine_provider_GetTrustLevel( IWineGameControllerProvider *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static const struct IWineGameControllerProviderVtbl wine_provider_vtbl =
{
    wine_provider_QueryInterface,
    wine_provider_AddRef,
    wine_provider_Release,
    /* IInspectable methods */
    wine_provider_GetIids,
    wine_provider_GetRuntimeClassName,
    wine_provider_GetTrustLevel,
};

DEFINE_IINSPECTABLE( game_provider, IGameControllerProvider, struct provider, IWineGameControllerProvider_iface )

static HRESULT WINAPI game_provider_get_FirmwareVersionInfo( IGameControllerProvider *iface, GameControllerVersionInfo *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_provider_get_HardwareProductId( IGameControllerProvider *iface, UINT16 *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_provider_get_HardwareVendorId( IGameControllerProvider *iface, UINT16 *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_provider_get_HardwareVersionInfo( IGameControllerProvider *iface, GameControllerVersionInfo *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_provider_get_IsConnected( IGameControllerProvider *iface, boolean *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static const struct IGameControllerProviderVtbl game_provider_vtbl =
{
    game_provider_QueryInterface,
    game_provider_AddRef,
    game_provider_Release,
    /* IInspectable methods */
    game_provider_GetIids,
    game_provider_GetRuntimeClassName,
    game_provider_GetTrustLevel,
    /* IGameControllerProvider methods */
    game_provider_get_FirmwareVersionInfo,
    game_provider_get_HardwareProductId,
    game_provider_get_HardwareVendorId,
    game_provider_get_HardwareVersionInfo,
    game_provider_get_IsConnected,
};

void provider_create( const WCHAR *device_path )
{
    IDirectInputDevice8W *dinput_device;
    IGameControllerProvider *provider;
    struct provider *impl, *entry;
    GUID guid = device_path_guid;
    IDirectInput8W *dinput;
    BOOL found = FALSE;
    const WCHAR *tmp;
    HRESULT hr;

    if (wcsnicmp( device_path, L"\\\\?\\HID#", 8 )) return;
    if ((tmp = wcschr( device_path + 8, '#' )) && !wcsnicmp( tmp - 6, L"&IG_", 4 )) return;

    TRACE( "device_path %s\n", debugstr_w( device_path ) );

    *(const WCHAR **)&guid = device_path;
    if (FAILED(DirectInput8Create( windows_gaming_input, DIRECTINPUT_VERSION, &IID_IDirectInput8W,
                                   (void **)&dinput, NULL ))) return;
    hr = IDirectInput8_CreateDevice( dinput, &guid, &dinput_device, NULL );
    IDirectInput8_Release( dinput );
    if (FAILED(hr)) return;

    if (!(impl = calloc( 1, sizeof(*impl) ))) goto done;
    impl->IWineGameControllerProvider_iface.lpVtbl = &wine_provider_vtbl;
    impl->IGameControllerProvider_iface.lpVtbl = &game_provider_vtbl;
    IDirectInputDevice_AddRef( dinput_device );
    impl->dinput_device = dinput_device;
    impl->ref = 1;

    wcscpy( impl->device_path, device_path );
    list_init( &impl->entry );
    provider = &impl->IGameControllerProvider_iface;
    TRACE( "created WineGameControllerProvider %p\n", provider );

    EnterCriticalSection( &provider_cs );
    LIST_FOR_EACH_ENTRY( entry, &provider_list, struct provider, entry )
        if ((found = !wcscmp( entry->device_path, device_path ))) break;
    if (!found) list_add_tail( &provider_list, &impl->entry );
    LeaveCriticalSection( &provider_cs );

    if (found) IGameControllerProvider_Release( provider );
    else manager_on_provider_created( provider );
done:
    IDirectInputDevice_Release( dinput_device );
}

void provider_remove( const WCHAR *device_path )
{
    IGameControllerProvider *provider;
    struct provider *entry;
    BOOL found = FALSE;

    TRACE( "device_path %s\n", debugstr_w( device_path ) );

    EnterCriticalSection( &provider_cs );
    LIST_FOR_EACH_ENTRY( entry, &provider_list, struct provider, entry )
        if ((found = !wcscmp( entry->device_path, device_path ))) break;
    if (found) list_remove( &entry->entry );
    LeaveCriticalSection( &provider_cs );

    if (found)
    {
        provider = &entry->IGameControllerProvider_iface;
        manager_on_provider_removed( provider );
        IGameControllerProvider_Release( provider );
    }
}
