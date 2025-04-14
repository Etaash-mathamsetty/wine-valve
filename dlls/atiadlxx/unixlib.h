/*
 * Unix library interface
 *
 * Copyright 2023 Paul Gofman for CodeWeavers
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

#include <stdbool.h>
#include "wine/unixlib.h"

enum amd_ags_funcs
{
    unix_init,
    unix_get_device_info,
};

typedef enum AsicFamily
{
    AsicFamily_Unknown,                                         ///< Unknown architecture, potentially from another IHV. Check \ref AGSDeviceInfo::vendorId
    AsicFamily_PreGCN,                                          ///< Pre GCN architecture.
    AsicFamily_GCN1,                                            ///< AMD GCN 1 architecture: Oland, Cape Verde, Pitcairn & Tahiti.
    AsicFamily_GCN2,                                            ///< AMD GCN 2 architecture: Hawaii & Bonaire.  This also includes APUs Kaveri and Carrizo.
    AsicFamily_GCN3,                                            ///< AMD GCN 3 architecture: Tonga & Fiji.
    AsicFamily_GCN4,                                            ///< AMD GCN 4 architecture: Polaris.
    AsicFamily_Vega,                                            ///< AMD Vega architecture, including Raven Ridge (ie AMD Ryzen CPU + AMD Vega GPU).
    AsicFamily_RDNA,                                            ///< AMD RDNA architecture
    AsicFamily_RDNA2,                                           ///< AMD RDNA2 architecture
    AsicFamily_RDNA3,                                           ///< AMD RDNA3 architecture
} AsicFamily;

struct get_device_info_params
{
    uint32_t device_id;
    uint32_t _pad;
    /* Output parameters. */
    uint32_t asic_family;
    uint32_t num_cu;
    uint32_t num_wgp;
    uint32_t num_rops;
    uint32_t core_clock;
    uint32_t memory_clock;
    uint32_t memory_bandwidth;
    uint32_t min_core_clock;
    uint32_t min_memory_clock;
    bool is_apu;
    float teraflops;
};
