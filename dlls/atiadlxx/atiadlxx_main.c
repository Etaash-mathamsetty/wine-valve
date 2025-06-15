/*
 * atiadlxx implementation
 *
 * Copyright 2023 Etaash Mathamsetty
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/debug.h"
#include "wine/heap.h"

#include "wine/vulkan.h"
#include "wine/asm.h"

#define COBJMACROS
#include "initguid.h"
#include "d3d11.h"
#include "d3d12.h"

#include "dxgi1_6.h"

#include "dxvk_interfaces.h"

#include "unixlib.h"

#include "amdheaders/adl_sdk.h"
#include <wingdi.h>

WINE_DEFAULT_DEBUG_CHANNEL(atiadlxx);

#define AMD_VENDOR_ID 0x1002

/* TODO split into multiple files */

/* TODO: Switch adapter_descs to this structure */
typedef struct _ADLAdapter {
    DXGI_ADAPTER_DESC1 desc;
    int monitor_index;
} ADLAdapter;

typedef struct _ADL_CONTEXT
{
    ADLThreadingModel model;
    ADL_MAIN_MALLOC_CALLBACK callback;
    IDXGIFactory1 *factory;
    IDXGIVkInteropFactory1 *dxgi_interop;
    DXGI_ADAPTER_DESC1 *adapter_descs;
    BOOL adl1;
    int monitor_count;
    int adapter_count;
    int enum_connected_adapters;
} ADL_CONTEXT;

static ADL_CONTEXT global_adl_context;

static const ADLVersionsInfo global_versions_info =
{
    "99.19.02-230831a-396538C-AMD-Software-Adrenalin-Edition",
    "99.10", /*BF4 reads this version*/
    "http://support.amd.com/drivers/xml/driver_09_us.xml",
};

static const ADLVersionsInfoX2 global_versions_infox2 =
{
    "99.19.02-230831a-396538C-AMD-Software-Adrenalin-Edition",
    "99.10", /*BF4 reads this version*/
    "99.10.2",
    "http://support.amd.com/drivers/xml/driver_09_us.xml",
};

static void create_dxgi_factory(IDXGIFactory1 **factory)
{
    static typeof(CreateDXGIFactory1) *pCreateDxgiFactory1 = NULL;

    if (!pCreateDxgiFactory1)
    {
        HMODULE dxgi_module = LoadLibraryW( L"dxgi.dll" );
        if (!dxgi_module)
        {
            ERR("Failed to load dxgi.dll\n");
            return;
        }

        pCreateDxgiFactory1 = (void *)GetProcAddress(dxgi_module, "CreateDXGIFactory1");
        if (!pCreateDxgiFactory1)
        {
            ERR("Failed to get CreateDXGIFactory1\n");
            return;
        }
    }

    if(FAILED(pCreateDxgiFactory1(&IID_IDXGIFactory1, (void **)factory)))
    {
        ERR("Failed to create IDXGIFactory1\n");
        return;
    }
}

static INIT_ONCE unix_init_once = INIT_ONCE_STATIC_INIT;
static BOOL unix_lib_initialized;

#define ATI_CALL(func, args) WINE_UNIX_CALL( unix_ ## func, args )

#ifdef __i386__
#define AMDAPI __cdecl
#else
#define AMDAPI __stdcall
#endif

static BOOL WINAPI init_unix_lib_once( INIT_ONCE *once, void *param, void **context )
{
    unix_lib_initialized = !__wine_init_unix_call() && !ATI_CALL( init, NULL );
    return TRUE;
}

static BOOL init_unix_lib(void)
{
    InitOnceExecuteOnce( &unix_init_once, init_unix_lib_once, NULL, NULL );
    return unix_lib_initialized;
}

static int WINAPI count_monitors(HMONITOR monitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    int *count = (int *)dwData;
    (*count)++;
    return TRUE;
}

static int init_context_descs(ADL_CONTEXT *context, int *num_adapters, DXGI_ADAPTER_DESC1 *descs)
{
    IDXGIAdapter1 *adapter = NULL;
    DXGI_ADAPTER_DESC1 desc;
    int i = 0;

    TRACE("(%p, %p, %p)\n", context, num_adapters, descs);

    if(!context)
        return ADL_ERR;

    for(; SUCCEEDED(IDXGIFactory1_EnumAdapters1(context->factory, i, &adapter)); i++)
    {
        IDXGIAdapter1_GetDesc1(adapter, &desc);

        if(desc.VendorId != AMD_VENDOR_ID)
            i--;
        else if(descs)
            descs[i] = desc;

        IDXGIAdapter1_Release(adapter);
    }

    *num_adapters = i;

    return ADL_OK;
}

static char* wchar_to_char(const WCHAR *src)
{
    int len = wcslen(src);
    char *dst = calloc(len + 1, sizeof(char));
    WideCharToMultiByte(CP_ACP, 0, src, -1, dst, len, NULL, NULL);

    return dst;
}

int AMDAPI ADL2_Main_ControlX3_Create(ADL_MAIN_MALLOC_CALLBACK callback, int enum_connected_adapters, ADL_CONTEXT_HANDLE* context, ADLThreadingModel model, int options)
{
    ADL_CONTEXT *adl_context = NULL;
    TRACE("(%p, %d, %p, %d, %d)\n", callback, enum_connected_adapters, context, model, options);

    if (!init_unix_lib())
    {
        ERR("Failed to initialize unixlib\n");
        return ADL_ERR;
    }

    adl_context = calloc(1, sizeof(ADL_CONTEXT));

    adl_context->model = model;
    adl_context->callback = callback;
    adl_context->enum_connected_adapters = enum_connected_adapters;
    adl_context->adl1 = false;

    create_dxgi_factory(&adl_context->factory);

    if(!adl_context->factory)
    {
        ERR("Failed to create IDXGIFactory1\n");
        return ADL_ERR;
    }

    if(FAILED(IDXGIFactory1_QueryInterface(adl_context->factory, &IID_IDXGIVkInteropFactory1, (void **)&adl_context->dxgi_interop)))
    {
        ERR("Failed to get IDXGIVkInteropFactory1\n");
        return ADL_ERR;
    }

    EnumDisplayMonitors(NULL, NULL, count_monitors, (LPARAM)&adl_context->monitor_count);

    *context = adl_context;

    init_context_descs(adl_context, &adl_context->adapter_count, NULL);

    adl_context->adapter_descs = calloc(adl_context->adapter_count, sizeof(DXGI_ADAPTER_DESC1));

    init_context_descs(adl_context, &adl_context->adapter_count, adl_context->adapter_descs);

    return ADL_OK;
}

int AMDAPI ADL2_Main_ControlX2_Create(ADL_MAIN_MALLOC_CALLBACK callback, int enum_connected_adapters, ADL_CONTEXT_HANDLE* context, ADLThreadingModel model)
{
    return ADL2_Main_ControlX3_Create(callback, enum_connected_adapters, context, model, 0);
}

int AMDAPI ADL2_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK callback, int enum_connected_adapters, ADL_CONTEXT_HANDLE* context)
{
    return ADL2_Main_ControlX2_Create(callback, enum_connected_adapters, context, ADL_THREADING_UNLOCKED);
}

int AMDAPI ADL2_Main_Control_Destroy(ADL_CONTEXT *context)
{
    TRACE("(%p)\n", context);

    if(!context)
        return ADL_ERR;

    if(context->factory)
        IDXGIFactory1_Release(context->factory);

    if(context->dxgi_interop)
        IDXGIVkInteropFactory1_Release(context->dxgi_interop);

    free(context->adapter_descs);
    free(context);

    return ADL_OK;
}

int AMDAPI ADL2_Main_Control_Refresh(ADL_CONTEXT *context)
{
    TRACE("(%p)\n", context);

    if(!context)
        return ADL_ERR;

    return ADL_OK;
}

int AMDAPI ADL_Main_ControlX2_Create(ADL_MAIN_MALLOC_CALLBACK callback, int enum_connected_adapters, ADLThreadingModel threadingModel)
{
    TRACE("(%p, %d, %d)\n", callback, enum_connected_adapters, threadingModel);

    if (!init_unix_lib())
    {
        ERR("Failed to initialize unixlib\n");
        return ADL_ERR;
    }

    global_adl_context.model = threadingModel;
    global_adl_context.callback = callback;
    global_adl_context.enum_connected_adapters = enum_connected_adapters;
    global_adl_context.adl1 = true;

    EnumDisplayMonitors(NULL, NULL, count_monitors, (LPARAM)&global_adl_context.monitor_count);

    create_dxgi_factory(&global_adl_context.factory);

    if(!global_adl_context.factory)
    {
        ERR("Failed to create IDXGIFactory1\n");
        return ADL_ERR;
    }

    if(FAILED(IDXGIFactory1_QueryInterface(global_adl_context.factory, &IID_IDXGIVkInteropFactory1, (void **)&global_adl_context.dxgi_interop)))
    {
        ERR("Failed to get IDXGIVkInteropFactory1\n");
        return ADL_ERR;
    }

    init_context_descs(&global_adl_context, &global_adl_context.adapter_count, NULL);

    global_adl_context.adapter_descs = calloc(global_adl_context.adapter_count, sizeof(DXGI_ADAPTER_DESC1));

    init_context_descs(&global_adl_context, &global_adl_context.adapter_count, global_adl_context.adapter_descs);

    return ADL_OK;
}

int AMDAPI ADL_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK callback, int enum_connected_adapters)
{
    return ADL_Main_ControlX2_Create(callback, enum_connected_adapters, ADL_THREADING_UNLOCKED);
}

int AMDAPI ADL_Main_Control_Refresh(void)
{
    return ADL2_Main_Control_Refresh(&global_adl_context);
}

int AMDAPI ADL_Main_Control_Destroy(void)
{
    TRACE("()\n");

    if(global_adl_context.factory)
        IDXGIFactory1_Release(global_adl_context.factory);

    if(global_adl_context.dxgi_interop)
        IDXGIVkInteropFactory1_Release(global_adl_context.dxgi_interop);

    free(global_adl_context.adapter_descs);

    return ADL_OK;
}

int AMDAPI ADL2_Adapter_NumberOfAdapters_Get(ADL_CONTEXT *context, int *num_adapters)
{
    TRACE("(%p, %p)\n", context, num_adapters);

    if(!context || !num_adapters)
        return ADL_ERR;

    *num_adapters = context->adapter_count * context->monitor_count;

    return ADL_OK;
}

static int convert_to_gen(uint32_t asic_family)
{
    if(asic_family >= AsicFamily_RDNA)
        return ADL_GRAPHIC_CORE_GENERATION_RDNA;
    if(asic_family >= AsicFamily_GCN1)
        return ADL_GRAPHIC_CORE_GENERATION_GCN;
    if(asic_family == AsicFamily_PreGCN)
        return ADL_GRAPHIC_CORE_GENERATION_PRE_GCN;

    return ADL_GRAPHIC_CORE_GENERATION_UNDEFINED;
}

int AMDAPI ADL2_Adapter_Graphic_Core_Info_Get(ADL_CONTEXT *context, int index, ADLGraphicCoreInfo* info)
{
    struct get_device_info_params params = {0};
    TRACE("(%p, %d, %p)\n", context, index, info);

    if(!context || !info)
        return ADL_ERR;

    if(index >= context->adapter_count)
        return ADL_ERR;

    params.device_id = context->adapter_descs[index].DeviceId;

    if(!ATI_CALL( get_device_info, &params ))
    {
        info->iNumCUs = params.num_cu;
        info->iNumROPs = params.num_rops;
        info->iNumWGPs = params.num_wgp;
        info->iGCGen = convert_to_gen(params.asic_family);
        return ADL_OK;
    }

    return ADL_ERR;
}

int AMDAPI ADL2_Graphics_VersionsX2_Get(ADL_CONTEXT *context, ADLVersionsInfoX2 *versions_info)
{
    TRACE("(%p, %p)\n", context, versions_info);

    if(!context || !versions_info)
        return ADL_ERR;

    *versions_info = global_versions_infox2;

    return ADL_OK;
}

int AMDAPI ADL2_Graphics_VersionsX3_Get(ADL_CONTEXT *context, int adapter_idx, ADLVersionsInfoX2 *versions_nfo)
{
    FIXME("Ignoring adapter index %d\n", adapter_idx);

    return ADL2_Graphics_VersionsX2_Get(context, versions_nfo);
}

int AMDAPI ADL2_Graphics_Versions_Get(ADL_CONTEXT *context, ADLVersionsInfo *versions_info)
{
    TRACE("(%p, %p)\n", context, versions_info);

    if(!context || !versions_info)
        return ADL_ERR;

    *versions_info = global_versions_info;

    return ADL_OK;
}

int AMDAPI ADL_Graphics_Versions_Get(ADLVersionsInfo *versions_info)
{
    return ADL2_Graphics_Versions_Get(&global_adl_context, versions_info);
}

int AMDAPI ADL2_Graphics_Platform_Get(ADL_CONTEXT *context, int *platform)
{
    FIXME("(%p) stub\n", platform);

    if(!platform || !context)
        return ADL_ERR;

    *platform = GRAPHICS_PLATFORM_DESKTOP;

    return ADL_OK;
}

int AMDAPI ADL_Graphics_Platform_Get(int *platform)
{
    return ADL2_Graphics_Platform_Get(&global_adl_context, platform);
}

int AMDAPI ADL2_Graphics_IsGfx9AndAbove(ADL_CONTEXT *context)
{
    struct get_device_info_params params = {0};
    TRACE("(%p)\n", context);

    if(!context)
        return ADL_ERR;

    params.device_id = context->adapter_descs[0].DeviceId;

    if(!ATI_CALL( get_device_info, &params ))
    {
        return params.asic_family >= AsicFamily_Vega;
    }

    return FALSE;
}

int AMDAPI ADL2_Adapter_AdapterInfo_Get(ADL_CONTEXT *context, AdapterInfo *info, int size)
{
    TRACE("(%p, %p, %d)\n", context, info, size);

    size /= sizeof(AdapterInfo);

    if(!context || !info)
        return ADL_ERR;

    for(int l = 0; l < size; l++)
    {
        if(context->adapter_descs[l].VendorId == AMD_VENDOR_ID)
        {
            char strUDID[256];
            char *dst;
            info[l].iSize = sizeof(AdapterInfo);
            info[l].iAdapterIndex = l;
            info[l].iVendorID = AMD_VENDOR_ID;
            info[l].iBusNumber = context->adapter_descs[l].DeviceId;
            strcpy(info[l].strDisplayName, "\\\\.\\DISPLAY1");
            sprintf(strUDID, "PCI\\VEN_%04X&DEV_%04X&REV_%02X", context->adapter_descs[l].VendorId, context->adapter_descs[l].DeviceId, 0);
            strcpy(info[l].strUDID, strUDID);
            info[l].iPresent = 1;
            info[l].iExist = 1;
            info[l].iDeviceNumber = l;
            dst = wchar_to_char(context->adapter_descs[l].Description);
            strcpy(info[l].strAdapterName, dst);
            TRACE("added adapter %s\n", dst);
            free(dst);
        }
    }

    return ADL_OK;
}

int AMDAPI ADL2_Adapter_AdapterInfoX2_Get(ADL_CONTEXT *context, AdapterInfo **info)
{
    TRACE("(%p, %p)\n", context, info);

    if(!context || !info)
        return ADL_ERR;

    *info = (AdapterInfo*)context->callback(sizeof(AdapterInfo) * context->adapter_count);

    if(!*info)
        return ADL_ERR;

    memset(*info, 0, sizeof(AdapterInfo) * context->adapter_count);

    return ADL2_Adapter_AdapterInfo_Get(context, *info, sizeof(AdapterInfo) * context->adapter_count);
}

int AMDAPI ADL_Adapter_ObservedGameClockInfo_Get(ADL_CONTEXT *context, int index, int *base, int *game, int *boost, int *memory)
{
    struct get_device_info_params params = {0};
    FIXME("(%p, %d, %p, %p, %p, %p) semi-stub\n", context, index, base, game, boost, memory);

    if(!context || !base || !game || !boost || !memory)
        return ADL_ERR;

    params.device_id = context->adapter_descs[index].DeviceId;

    if(!ATI_CALL( get_device_info, &params ))
    {
        *base = params.min_core_clock;
        *game = params.core_clock;
        *boost = params.core_clock;
        *memory = params.memory_clock;
        return ADL_OK;
    }

    return ADL_ERR;
}

int AMDAPI ADL2_Adapter_MemoryInfo_Get(ADL_CONTEXT *context, int index, ADLMemoryInfo *info)
{
    struct get_device_info_params params = {0};
    TRACE("(%p, %d, %p)\n", context, index, info);

    if(!context || !info)
        return ADL_ERR;

    params.device_id = context->adapter_descs[index].DeviceId;

    if(!ATI_CALL( get_device_info, &params ))
    {
        info->iMemorySize = context->adapter_descs[index].DedicatedVideoMemory;
        info->iMemoryBandwidth = params.memory_bandwidth;
        strcpy(info->strMemoryType, "GDDR6");
        return ADL_OK;
    }

    return ADL_ERR;
}

int AMDAPI ADL2_Display_DisplayInfo_Get(ADL_CONTEXT *context, int index, int *displays, ADLDisplayInfo **infos, int force_detect)
{
    DEVMODEA devmode = {0};
    DISPLAY_DEVICEA dispdev = {0};
    IDXGIAdapter1 *adapter = NULL;
    IDXGIOutput *output = NULL;
    HRESULT res;
    TRACE("(%p, %d, %p, %p, %d)\n", context, index, displays, infos, force_detect);

    if(!context || !displays || !infos)
        return ADL_ERR;

    if(index == -1)
        index = 0;

    res = IDXGIFactory1_EnumAdapters1(context->factory, index, &adapter);

    if(FAILED(res))
    {
        ERR("Failed to get IDXGIAdapter1\n");
        return ADL_ERR;
    }

    res = IDXGIAdapter1_EnumOutputs(adapter, 0, &output);

    if(FAILED(res))
    {
        ERR("Failed to get IDXGIOutput\n");
        return ADL_ERR;
    }

    IDXGIAdapter1_Release(adapter);
    IDXGIOutput_Release(output);

    *displays = context->monitor_count;
    *infos = context->callback(sizeof(ADLDisplayInfo) * context->monitor_count);

    if(!*infos)
        return ADL_ERR;

    devmode.dmSize = sizeof(DEVMODEA);

    for(int i = 0; i < context->monitor_count; i++)
    {
        ADLDisplayInfo* info = (*infos) + i;

        EnumDisplayDevicesA(NULL, i, &dispdev, 0);

        EnumDisplaySettingsA(dispdev.DeviceName, ENUM_CURRENT_SETTINGS, &devmode);

        info->displayID.iDisplayLogicalAdapterIndex = index;
        info->displayID.iDisplayLogicalIndex = i;
        info->displayID.iDisplayPhysicalAdapterIndex = index;
        info->displayID.iDisplayPhysicalIndex = i;

        strcpy(info->strDisplayName, dispdev.DeviceString);
        /* FIXME */
        strcpy(info->strDisplayManufacturerName, "Samsung");
        info->iDisplayConnector = ADL_DISPLAY_CONTYPE_DISPLAYPORT;
        info->iDisplayControllerIndex = 0;
        info->iDisplayInfoMask = ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED;
        info->iDisplayInfoValue = ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED;
        info->iDisplayOutputType = ADL_DT_LCD_PANEL;
        /* FIXME */
        info->iDisplayOutputType = 0;
    }

    return ADL_OK;
}

int AMDAPI ADL2_Display_Modes_Get(ADL_CONTEXT *context, int index, int display, int *num_modes, ADLMode **modes)
{
    IDXGIAdapter1 *adapter = NULL;
    IDXGIOutput *output = NULL;
    uint32_t num_modes_dxgi;
    HRESULT res;
    DEVMODEA devmode = {0};
    DISPLAY_DEVICEA dispdev = {0};
    DXGI_MODE_DESC *modes_dxgi = NULL;
    TRACE("(%p, %d, %d, %p, %p)\n", context, index, display, num_modes, modes);

    if(!context || !num_modes || !modes)
        return ADL_ERR;

    if(index == -1)
        index = 0;
    if(display == -1)
        display = 0;

    res = IDXGIFactory1_EnumAdapters1(context->factory, index, &adapter);

    if(FAILED(res))
    {
        ERR("Failed to get IDXGIAdapter1\n");
        return ADL_ERR;
    }

    res = IDXGIAdapter1_EnumOutputs(adapter, display, &output);

    if(FAILED(res))
    {
        ERR("Failed to get IDXGIOutput\n");
        return ADL_ERR;
    }

    res = IDXGIOutput_GetDisplayModeList(output, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &num_modes_dxgi, NULL);

    if(FAILED(res))
    {
        ERR("Failed to get display mode list\n");
        return ADL_ERR;
    }

    modes_dxgi = calloc(num_modes_dxgi, sizeof(DXGI_MODE_DESC));

    res = IDXGIOutput_GetDisplayModeList(output, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &num_modes_dxgi, modes_dxgi);

    if(FAILED(res))
    {
        ERR("Failed to get display mode list\n");
        return ADL_ERR;
    }

    if(!modes_dxgi)
        return ADL_ERR;

    *num_modes = num_modes_dxgi;
    *modes = (ADLMode*)context->callback(sizeof(ADLMode) * num_modes_dxgi);

    if(!*modes)
    {
        free(modes_dxgi);
        return ADL_ERR;
    }

    EnumDisplayDevicesA(NULL, display, &dispdev, 0);

    EnumDisplaySettingsA(dispdev.DeviceName, ENUM_CURRENT_SETTINGS, &devmode);

    for(uint32_t i = 0; i < num_modes_dxgi; i++)
    {
        ADLMode *mode = (*modes) + (num_modes_dxgi - i - 1);
        mode->fRefreshRate = (float)modes_dxgi[i].RefreshRate.Numerator / (float)modes_dxgi[i].RefreshRate.Denominator;
        mode->iColourDepth = devmode.dmBitsPerPel;
        mode->iAdapterIndex = index;
        mode->iXPos = devmode.dmPosition.x;
        mode->iYPos = devmode.dmPosition.y;
        mode->iXRes = modes_dxgi[i].Width;
        mode->iYRes = modes_dxgi[i].Height;
        mode->iOrientation = devmode.dmOrientation * 90;
        mode->iModeFlag = modes_dxgi[i].ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE ?
                                        ADL_DISPLAY_MODE_PROGRESSIVE_FLAG : ADL_DISPLAY_MODE_INTERLACED_FLAG;
        /* FIXME */
        mode->iModeMask = ADL_DISPLAY_MODE_COLOURFORMAT_8888 | ADL_DISPLAY_MODE_ORIENTATION_SUPPORTED_000 | ADL_DISPLAY_MODE_REFRESHRATE_ROUNDED;
        mode->iModeValue = ADL_DISPLAY_MODE_COLOURFORMAT_8888 | ADL_DISPLAY_MODE_ORIENTATION_SUPPORTED_000 | ADL_DISPLAY_MODE_REFRESHRATE_ROUNDED;

        mode->displayID.iDisplayLogicalAdapterIndex = index;
        mode->displayID.iDisplayLogicalIndex = i;
        mode->displayID.iDisplayPhysicalAdapterIndex = index;
        mode->displayID.iDisplayPhysicalIndex = i;
    }

    free(modes_dxgi);
    IDXGIAdapter1_Release(adapter);
    IDXGIOutput_Release(output);

    return ADL_OK;
}

int AMDAPI ADL_Display_Modes_Get(int index, int display, int *num_modes, ADLMode **modes)
{
    return ADL2_Display_Modes_Get(&global_adl_context, index, display, num_modes, modes);
}

static int convert_colorspace(DXGI_COLOR_SPACE_TYPE type)
{
    switch(type)
    {
        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
            return ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB888;
        default: /* FIXME */
            FIXME("unknown colorspace %#x\n", type);
            return ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB888;
    }
}

int AMDAPI ADL2_Display_FreeSync_Cap(ADL_CONTEXT *context, int index, int display, ADLFreeSyncCap *caps)
{
    DEVMODEA devmode = {0};
    DISPLAY_DEVICEA dispdev = {0};
    FIXME("(%p, %d, %d, %p) stub! faking freesync support\n", context, index, display, caps);

    if(!context || !caps)
        return ADL_ERR;

    if(index == -1)
        index = 0;

    if(display == -1)
        display = 0;

    EnumDisplayDevicesA(NULL, display, &dispdev, 0);

    EnumDisplaySettingsA(dispdev.DeviceName, ENUM_CURRENT_SETTINGS, &devmode);

    caps->iCaps = ADL_FREESYNC_CAP_SUPPORTED | ADL_FREESYNC_CAP_CURRENTMODESUPPORTED | ADL_FREESYNC_CAP_GPUSUPPORTED;
    caps->iMaxRefreshRateInMicroHz = devmode.dmDisplayFrequency * 1000000;
    caps->iMinRefreshRateInMicroHz = devmode.dmDisplayFrequency * 1000000;
    caps->ucLabelIndex = ADL_FREESYNC_LABEL_FREESYNC_PREMIUM_PRO;

    return ADL_OK;
}

int AMDAPI ADL2_Display_DDCInfo2_Get(ADL_CONTEXT *context, int index, int display, ADLDDCInfo2 *info)
{
    DEVMODEA devmode = {0};
    DISPLAY_DEVICEA dispdev = {0};
    IDXGIOutput* output = NULL;
    IDXGIOutput6 *output6 = NULL;
    IDXGIAdapter1 *adapter = NULL;
    ADLFreeSyncCap caps;
    DXGI_OUTPUT_DESC1 desc;
    HRESULT res;
    TRACE("(%p, %d, %d, %p)\n", context, index, display, info);

    if(!context || !info)
        return ADL_ERR;

    if(index == -1)
        index = 0;

    if(display == -1)
        display = 0;

    EnumDisplayDevicesA(NULL, display, &dispdev, 0);

    EnumDisplaySettingsA(dispdev.DeviceName, ENUM_CURRENT_SETTINGS, &devmode);

    res = IDXGIFactory1_EnumAdapters1(context->factory, index, &adapter);

    if(FAILED(res))
    {
        ERR("Failed to get IDXGIAdapter1\n");
        return ADL_ERR;
    }

    res = IDXGIAdapter1_EnumOutputs(adapter, display, &output);

    if(FAILED(res))
    {
        ERR("Failed to get IDXGIOutput\n");
        return ADL_ERR;
    }

    res = IDXGIOutput_QueryInterface(output, &IID_IDXGIOutput6, (void **)&output6);

    if(FAILED(res))
    {
        ERR("Failed to get IDXGIOutput6\n");
        return ADL_ERR;
    }

    res = IDXGIOutput6_GetDesc1(output6, &desc);

    if(FAILED(res))
    {
        ERR("Failed to get output description\n");
        return ADL_ERR;
    }

    memset(info, 0, sizeof(ADLDDCInfo2));

    info->iNativeDisplayChromaticityBlueX = desc.BluePrimary[0];
    info->iNativeDisplayChromaticityBlueY = desc.BluePrimary[1];
    info->iNativeDisplayChromaticityGreenX = desc.GreenPrimary[0];
    info->iNativeDisplayChromaticityGreenY = desc.GreenPrimary[1];
    info->iNativeDisplayChromaticityRedX = desc.RedPrimary[0];
    info->iNativeDisplayChromaticityRedY = desc.RedPrimary[1];
    info->iNativeDisplayChromaticityWhitePointX = desc.WhitePoint[0];
    info->iNativeDisplayChromaticityWhitePointY = desc.WhitePoint[1];

    ADL2_Display_FreeSync_Cap(context, index, display, &caps);

    TRACE("output_desc.ColorSpace %#x.\n", desc.ColorSpace);
    if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
    {
        TRACE("Reporting monitor %s as HDR10 supported.\n", debugstr_a((char*)devmode.dmDeviceName));
        info->iSupportedHDR = ADL_HDR_CEA861_3;

        if(caps.iCaps & ADL_FREESYNC_CAP_SUPPORTED)
            info->iSupportedHDR |= ADL_HDR_FREESYNC_HDR;
    }

    /* FIXME */
    info->iFreesyncFlags = caps.iCaps;

    info->ulMaxRefresh = devmode.dmDisplayFrequency;
    info->ulPTMRefreshRate = devmode.dmDisplayFrequency;
    info->ulPTMCx = devmode.dmPelsWidth;
    info->ulPTMCy = devmode.dmPelsHeight;
    info->ulMaxVResolution = devmode.dmPelsHeight;
    info->ulMaxHResolution = devmode.dmPelsWidth;
    strcpy(info->cDisplayName, (char*)devmode.dmDeviceName);

    info->iPanelPixelFormat = convert_colorspace(desc.ColorSpace);

    info->ulSupportsDDC = 1;
    info->ulSize = sizeof(ADLDDCInfo2);
    info->ulMinLuminanceData = desc.MinLuminance;
    info->ulMaxLuminanceData = desc.MaxLuminance;
    info->ulAvgLuminanceData = desc.MaxFullFrameLuminance;

    IDXGIAdapter1_Release(adapter);
    IDXGIOutput_Release(output);

    /* TODO add the rest of the data */

    return ADL_OK;
}

int AMDAPI ADL2_Display_DisplayMapConfig_Get(ADL_CONTEXT *context, int index, int *num_display_maps, ADLDisplayMap **display_maps,
                                                        int *num_display_target, ADLDisplayTarget **display_target, int options)
{
    DEVMODEA devmode = {0};
    DISPLAY_DEVICEA dispdev = {0};
    FIXME("(%p, %d, %p, %p, %p, %p, %d) semi-stub\n", context, index, num_display_maps, display_maps, num_display_target, display_target, options);

    if(!context || !num_display_maps || !display_maps || !num_display_target || !display_target)
        return ADL_ERR;

    if(index == -1)
        index = 0;

    if(options > 1)
        FIXME("options %#x not supported\n", options);


    *num_display_maps = context->monitor_count;
    *num_display_target = context->monitor_count;

    *display_maps = context->callback(sizeof(ADLDisplayMap) * context->monitor_count);
    *display_target = context->callback(sizeof(ADLDisplayTarget) * context->monitor_count);

    for(int i = 0; i < context->monitor_count; i++)
    {
        ADLDisplayMap *map = (*display_maps) + i;
        ADLDisplayTarget *target = (*display_target) + i;
        ADLMode mode;

        EnumDisplayDevicesA(NULL, i, &dispdev, 0);
        EnumDisplaySettingsA(dispdev.DeviceName, ENUM_CURRENT_SETTINGS, &devmode);

        map->iDisplayMapIndex = i;
        map->iNumDisplayTarget = 1;
        map->iFirstDisplayTargetArrayIndex = i;

        mode.displayID.iDisplayLogicalAdapterIndex = index;
        mode.displayID.iDisplayLogicalIndex = i;
        mode.displayID.iDisplayPhysicalAdapterIndex = index;
        mode.displayID.iDisplayPhysicalIndex = i;

        mode.fRefreshRate = (float)devmode.dmDisplayFrequency;
        mode.iColourDepth = devmode.dmBitsPerPel;
        mode.iAdapterIndex = index;
        mode.iXPos = devmode.dmPosition.x;
        mode.iYPos = devmode.dmPosition.y;
        mode.iXRes = devmode.dmPelsWidth;
        mode.iYRes = devmode.dmPelsHeight;
        mode.iOrientation = devmode.dmOrientation * 90;
        mode.iModeFlag = ADL_DISPLAY_MODE_PROGRESSIVE_FLAG;
        /* FIXME */
        mode.iModeMask = ADL_DISPLAY_MODE_COLOURFORMAT_8888 | ADL_DISPLAY_MODE_ORIENTATION_SUPPORTED_000 | ADL_DISPLAY_MODE_REFRESHRATE_ROUNDED;
        mode.iModeValue = ADL_DISPLAY_MODE_COLOURFORMAT_8888 | ADL_DISPLAY_MODE_ORIENTATION_SUPPORTED_000 | ADL_DISPLAY_MODE_REFRESHRATE_ROUNDED;

        map->displayMode = mode;
        map->iDisplayMapMask = ADL_DISPLAY_DISPLAYMAP_MANNER_SINGLE;
        map->iDisplayMapValue = ADL_DISPLAY_DISPLAYMAP_MANNER_SINGLE;

        target->displayID.iDisplayLogicalAdapterIndex = index;
        target->displayID.iDisplayLogicalIndex = i;
        target->displayID.iDisplayPhysicalAdapterIndex = index;
        target->displayID.iDisplayPhysicalIndex = i;

        target->iDisplayMapIndex = i;
        target->iDisplayTargetMask = ADL_DISPLAY_DISPLAYTARGET_PREFERRED;
        target->iDisplayTargetValue = i == 0 ? ADL_DISPLAY_DISPLAYTARGET_PREFERRED : 0;
    }

    return ADL_OK;
}

int AMDAPI ADL_Display_DisplayMapConfig_Get(int index, int *num_display_maps, ADLDisplayMap **display_maps,
                                                        int *num_display_target, ADLDisplayTarget **display_target, int options)
{
    return ADL2_Display_DisplayMapConfig_Get(&global_adl_context, index, num_display_maps, display_maps, num_display_target, display_target, options);
}

int AMDAPI ADL_Display_DisplayInfo_Get(int index, int *num_displays, ADLDisplayInfo **infos, int force_detect)
{
    return ADL2_Display_DisplayInfo_Get(&global_adl_context, index, num_displays, infos, force_detect);
}

int AMDAPI ADL_Adapter_AdapterInfo_Get(AdapterInfo *info, int size)
{
    return ADL2_Adapter_AdapterInfo_Get(&global_adl_context, info, size);
}

int AMDAPI ADL_Adapter_AdapterInfoX2_Get(AdapterInfo **info)
{
    return ADL2_Adapter_AdapterInfoX2_Get(&global_adl_context, info);
}

int AMDAPI ADL_Adapter_MemoryInfo_Get(int index, ADLMemoryInfo *info)
{
    return ADL2_Adapter_MemoryInfo_Get(&global_adl_context, index, info);
}

int AMDAPI ADL_Adapter_NumberOfAdapters_Get(int *num_adapters)
{
    return ADL2_Adapter_NumberOfAdapters_Get(&global_adl_context, num_adapters);
}

int AMDAPI ADL2_Adapter_ASICFamilyType_Get(ADL_CONTEXT *context, int index, int *asic_types, int *valids)
{
    struct get_device_info_params params = {0};
    FIXME("(%p, %d, %p, %p) semi-stub\n", context, index, asic_types, valids);

    if(!context || !asic_types || !valids)
        return ADL_ERR;

    if(context->adapter_descs[index].VendorId != AMD_VENDOR_ID)
        return ADL_ERR;

    params.device_id = context->adapter_descs[index].DeviceId;

    if(ATI_CALL( get_device_info, &params ))
        return ADL_ERR;

    *asic_types = params.is_apu ? ADL_ASIC_INTEGRATED : ADL_ASIC_DISCRETE;
    *valids = 0xAF; /* ADL_ASIC_MASK */

    return ADL_OK;
}

int AMDAPI ADL_Adapter_ASICFamilyType_Get(int index, int *asic_types, int *valids)
{
    return ADL2_Adapter_ASICFamilyType_Get(&global_adl_context, index, asic_types, valids);
}

int AMDAPI ADL2_Overdrive_Caps(ADL_CONTEXT *context, int index, int *supported, int *enabled, int *version)
{
    FIXME("(%p, %d, %p, %p, %p) stub\n", context, index, supported, enabled, version);

    if(!context || !supported || !enabled || !version)
        return ADL_ERR;

    *supported = ADL_TRUE;
    *enabled = ADL_FALSE;
    *version = 6;

    return ADL_OK;
}

int AMDAPI ADL2_Overdrive6_Capabilities_Get(ADL_CONTEXT *context, int index, ADLOD6Capabilities *caps)
{
    int base, game, boost, memory;
    FIXME("(%p, %d, %p) semi-stub\n", context, index, caps);

    if(!context || !caps)
        return ADL_ERR;


    caps->iCapabilities = ADL_OD6_CAPABILITY_POWER_CONTROL | ADL_OD6_CAPABILITY_MCLK_CUSTOMIZATION |
                             ADL_OD6_CAPABILITY_GPU_ACTIVITY_MONITOR | ADL_OD6_CAPABILITY_SCLK_CUSTOMIZATION;
    caps->iNumberOfPerformanceLevels = 2;
    caps->iExtMask = 0;
    caps->iExtValue = 0;
    ADL_Adapter_ObservedGameClockInfo_Get(context, index, &base, &game, &boost, &memory);
    caps->iSupportedStates = ADL_OD6_SUPPORTEDSTATE_PERFORMANCE;
    caps->sEngineClockRange.iMax = game;
    caps->sEngineClockRange.iMin = base;
    caps->sMemoryClockRange.iMax = memory;
    caps->sMemoryClockRange.iMin = 100; /* assume 100 is base */

    return ADL_OK;
}

int AMDAPI ADL2_Overdrive6_StateInfo_Get(ADL_CONTEXT *context, int index, int type, ADLOD6StateInfo *info)
{
    int base, game, boost, memory;
    FIXME("(%p, %d, %d, %p) semi-stub\n", context, index, type, info);

    if(!context || !info)
        return ADL_ERR;

    info->iExtMask = 0;
    info->iExtValue = 0;
    info->iNumberOfPerformanceLevels = 2;
    ADL_Adapter_ObservedGameClockInfo_Get(context, index, &base, &game, &boost, &memory);
    /* assume caller allocated the struct in the correct way */
    info->aLevels[0].iEngineClock = base;
    info->aLevels[0].iMemoryClock = 100; /* assume 100 is base */
    info->aLevels[1].iEngineClock = game;
    info->aLevels[1].iMemoryClock = memory;

    return ADL_OK;
}

int AMDAPI ADL2_Adapter_AdapterInfoX4_Get(ADL_CONTEXT *context, int index, int *num_adapters, AdapterInfoX2 **info)
{
    AdapterInfoX2 *info2;
    char *dst;
    TRACE("(%p, %d, %p, %p)\n", context, index, num_adapters, info);


    if(index == -1)
    {
        *num_adapters = context->adapter_count;
        *info = context->callback(sizeof(AdapterInfoX2) * context->adapter_count);

        if(!*info)
            return ADL_ERR;

        info2 = *info;

        for(int i = 0; i < context->adapter_count; i++)
        {
            info2[i].iSize = sizeof(AdapterInfoX2);
            info2[i].iAdapterIndex = i;
            info2[i].iPresent = 1;
            info2[i].iExist = 1;
            dst = wchar_to_char(context->adapter_descs[index].Description);
            strcpy(info2->strAdapterName, dst);
            free(dst);
            info2[i].iVendorID = AMD_VENDOR_ID;
            info2[i].iBusNumber = context->adapter_descs[i].DeviceId;
            info2[i].iDeviceNumber = i;
        }

        return ADL_OK;
    }

    *info = context->callback(sizeof(AdapterInfoX2));

    if(!*info)
        return ADL_ERR;

    info2 = *info;

    memset(info2, 0, sizeof(AdapterInfoX2));

    info2->iSize = sizeof(AdapterInfoX2);
    info2->iAdapterIndex = index;
    info2->iPresent = 1;
    info2->iExist = 1;
    info2->iDeviceNumber = index;
    dst = wchar_to_char(context->adapter_descs[index].Description);
    strcpy(info2->strAdapterName, dst);
    free(dst);
    info2->iVendorID = AMD_VENDOR_ID;
    info2->iBusNumber = context->adapter_descs[index].DeviceId;


    return ADL_OK;
}

int AMDAPI ADL2_Adapter_MemoryInfo2_Get(ADL_CONTEXT *context, int index, ADLMemoryInfo2 *info)
{
    struct get_device_info_params params = {0};
    TRACE("(%p, %d, %p)\n", context, index, info);

    if(!context || !info)
        return ADL_ERR;

    params.device_id = context->adapter_descs[index].DeviceId;

    if(!ATI_CALL( get_device_info, &params ))
    {
        info->iMemorySize = context->adapter_descs[index].DedicatedVideoMemory;
        info->iMemoryBandwidth = params.memory_bandwidth;
        strcpy(info->strMemoryType, "GDDR6");
        info->iHyperMemorySize = 0;
        info->iVisibleMemorySize = context->adapter_descs[index].SharedSystemMemory;
        info->iInvisibleMemorySize = 0;
        return ADL_OK;
    }

    return ADL_ERR;
}

int AMDAPI ADL2_Display_SLSMapIndex_Get(ADL_CONTEXT *context, int index, int num_displays, ADLDisplayTarget *targets, int *ret)
{
    FIXME("(%p, %d, %d, %p, %p) stub\n", context, index, num_displays, targets, ret);

    if(!context || !targets || !ret)
        return ADL_ERR;

    *ret = 0;
    return ADL_OK;
}

int AMDAPI ADL_Display_SLSMapIndex_Get(int index, int num_displays, ADLDisplayTarget *targets, int *ret)
{
    return ADL2_Display_SLSMapIndex_Get(&global_adl_context, index, num_displays, targets, ret);
}

int AMDAPI ADL2_Display_SLSMapConfig_Get( 	ADL_CONTEXT *  	context,
		int  	iAdapterIndex,
		int  	iSLSMapIndex,
		ADLSLSMap *  	lpSLSMap,
		int *  	lpNumSLSTarget,
		ADLSLSTarget **  	lppSLSTarget,
		int *  	lpNumNativeMode,
		ADLSLSMode **  	lppNativeMode,
		int *  	lpNumBezelMode,
		ADLBezelTransientMode **  	lppBezelMode,
		int *  	lpNumTransientMode,
		ADLBezelTransientMode **  	lppTransientMode,
		int *  	lpNumSLSOffset,
		ADLSLSOffset **  	lppSLSOffset,
		int  	iOption )
{
    FIXME("(%p, %d, %d, %p, %p, %p, %p, %p, %p, %p, %p, %p, %p, %p, %d) stub\n", context, iAdapterIndex,
            iSLSMapIndex, lpSLSMap, lpNumSLSTarget, lppSLSTarget, lpNumNativeMode, lppNativeMode,
            lpNumBezelMode, lppBezelMode, lpNumTransientMode, lppTransientMode, lpNumSLSOffset, lppSLSOffset, iOption);

    if(!context || !lpSLSMap || !lpNumSLSTarget || !lppSLSTarget || !lpNumNativeMode ||
    !lppNativeMode || !lpNumBezelMode || !lppBezelMode || !lpNumTransientMode ||
    !lppTransientMode || !lpNumSLSOffset || !lppSLSOffset)
        return ADL_ERR;

    *lpNumSLSTarget = 0;
    *lpNumNativeMode = 0;
    *lpNumBezelMode = 0;
    *lpNumTransientMode = 0;
    *lpNumSLSOffset = 0;

    return ADL_OK;
}

int AMDAPI ADL_Display_SLSMapConfig_Get(
        int  	iAdapterIndex,
		int  	iSLSMapIndex,
		ADLSLSMap *  	lpSLSMap,
		int *  	lpNumSLSTarget,
		ADLSLSTarget **  	lppSLSTarget,
		int *  	lpNumNativeMode,
		ADLSLSMode **  	lppNativeMode,
		int *  	lpNumBezelMode,
		ADLBezelTransientMode **  	lppBezelMode,
		int *  	lpNumTransientMode,
		ADLBezelTransientMode **  	lppTransientMode,
		int *  	lpNumSLSOffset,
		ADLSLSOffset **  	lppSLSOffset,
		int  	iOption )
{
    return ADL2_Display_SLSMapConfig_Get(&global_adl_context, iAdapterIndex, iSLSMapIndex, lpSLSMap, lpNumSLSTarget,
        lppSLSTarget, lpNumNativeMode, lppNativeMode, lpNumBezelMode, lppBezelMode, lpNumTransientMode,
        lppTransientMode, lpNumSLSOffset, lppSLSOffset, iOption);
}

int AMDAPI ADL2_Display_EdidData_Get(ADL_CONTEXT *context, int adapter_index, int display_index, ADLDisplayEDIDData *data)
{
    FIXME("(%p, %d, %d, %p) stub\n", context, adapter_index, display_index, data);

    if (!data) return ADL_ERR;

    memset(data, 0, sizeof(ADLDisplayEDIDData));

    return ADL_OK;
}

int AMDAPI ADL_Display_EdidData_Get(int adapter_index, int display_index, ADLDisplayEDIDData *data)
{
    return ADL2_Display_EdidData_Get(&global_adl_context, adapter_index, display_index, data);
}

int AMDAPI ADL2_Adapter_Active_Get(ADL_CONTEXT *context, int index, int *status)
{
    FIXME("(%p %d %p) stub!\n", context, index, status);

    if(!context || !status)
        return ADL_ERR;

    *status = ADL_TRUE;

    return ADL_OK;
}

typedef struct
{
    /* TODO: what are the elements? */
    int xyz;
} ADLDisplayContentAttribute;

int AMDAPI ADL2_Display_SourceContentAttribute_Set(ADL_CONTEXT *context, int adapter_index, int display_index, ADLDisplayContentAttribute *attribute)
{
    FIXME("(%p %d %d %p) stub!\n", context, adapter_index, display_index, attribute);

    if(!context || !attribute)
        return ADL_ERR;

    return ADL_OK;
}

int AMDAPI ADL2_Adapter_Crossfire_Caps(ADL_CONTEXT *context, int adapter_index, int *preffer, int *numComb, ADLCrossfireComb **combs) {

    if(!context || !preffer || !numComb)
        return ADL_ERR;

    FIXME("(%p %d %p %p %p) stub!\n", context, adapter_index, preffer, numComb, combs);

    *preffer = 0;
    *numComb = 0;

    return ADL_OK;
}

int AMDAPI ADL2_OverdriveN_Temperature_Get(ADL_CONTEXT *context, int adapter_index, int temptype, int *temp)
{

    if(!context || !temp)
        return ADL_ERR;

    FIXME("(%p %d %d %p) stub!\n", context, adapter_index, temptype, temp);

    *temp = 0;

    return ADL_OK;
}

int AMDAPI ADL2_OverdriveN_PerformanceStatus_Get(ADL_CONTEXT *context, int adapter_index, ADLODNPerformanceStatus *status)
{
    int temp;
    if(!context || !status)
        return ADL_ERR;

    FIXME("(%p %d %p) semi-stub!\n", context, adapter_index, status);

    return ADL_Adapter_ObservedGameClockInfo_Get(context, adapter_index, &status->iCoreClock, &temp, &temp, &status->iMemoryClock);
}
