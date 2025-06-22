/*
 * ContainerID helper functions
 *
 * Copyright 2025 Harald Sitter <sitter@kde.org>
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h> /* Definition of AT_* constants */
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/containerid.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(containerid);

// Find the directory with a 'removeble' file. Mutates sysfs_path in place.
static BOOL find_removable_file_dir(char *sysfs_path)
{
    DIR *device_dir = NULL;
    struct stat st;
    for (;;) {
        if (strcmp("/sys/devices", sysfs_path) == 0) {
            TRACE("Device is not removable (could not find removable file)\n");
            return FALSE;
        }
        device_dir = opendir(sysfs_path);
        if (fstatat(dirfd(device_dir), "removable", &st, 0) == 0) {
            closedir(device_dir);
            return TRUE;
        }
        closedir(device_dir);
        dirname(sysfs_path); // mutates in place
    }
    return FALSE;
}

// Checks if the device at sysfs_path is removable by checking the contents of the 'removable' file.
static BOOL is_device_removable(char *sysfs_path)
{
    char is_removable_str[MAX_PATH];
    char removable[] = "removable";
    DIR *device_dir = opendir(sysfs_path);
    int fd = openat(dirfd(device_dir), "removable", O_RDONLY | O_CLOEXEC);
    int err = errno;

    closedir(device_dir);

    if (fd != -1) {
        read(fd, is_removable_str, sizeof(is_removable_str));
        close(fd);
        if (strncmp(is_removable_str, removable, strlen(removable)) == 0) {
            // Perfect, it's removable, so let's expose the sysfs path and by extension generate a container id.
            return TRUE;
        } else {
            return FALSE;
            TRACE("Device is not removable, not exposing sysfs path\n");
        }
    }

    WARN("Failed to open %s/removable: %s\n", sysfs_path, strerror(err));
    return FALSE;
}

static BOOL get_device_sysfs_path_from_sys_path(char const *sysfs_path, char device_path[MAX_PATH])
{
    char resolved_sysfs_path[MAX_PATH];
    // Resolve all parts.
    if (realpath(sysfs_path, resolved_sysfs_path) == NULL) {
        WARN("realpath failed: %s\n", strerror(errno));
        return FALSE;
    }
    // Then walk up until we find a removable file marker.
    if (find_removable_file_dir(resolved_sysfs_path)) {
        // resolved_sysfs_path is now pointing at the device directory containing a removable file.
        // Next let's figure out if this device is actually removable.
        if (is_device_removable(resolved_sysfs_path)) {
            strcpy(device_path, resolved_sysfs_path);
            return TRUE;
        }
    }
    return FALSE;
}

static void container_id_from_inputs(char const **inputs, unsigned inputs_count, GUID *container_id)
{
    UINT8 hash[sizeof(GUID)] = {0};
    UINT8 hash_index = 0;

    // Trivialistic hash function. XOR all the bytes of all the inputs together.
    for (int i = 0; i < inputs_count; i++) {
        for (int j = 0; j < strlen(inputs[i]); j++) {
            hash[hash_index] ^= inputs[i][j];
            hash_index = (hash_index + 1) % ARRAY_SIZE(hash);
        }
    }

    memcpy(container_id, hash, sizeof(GUID));
}

static  NTSTATUS fill_container_id(char const device_path[MAX_PATH], char const id_product[7], char const id_vendor[7], GUID *container_id)
{
    char const *inputs[] = {device_path, id_product, id_vendor};

    // When sysfs_path is empty it means something has gone horribly wrong.
    if (device_path[0] == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    container_id_from_inputs(inputs, ARRAY_SIZE(inputs), container_id);
    TRACE("Generated container id: %s\n", wine_dbgstr_guid(container_id));

    return STATUS_SUCCESS;
}

static BOOL read_id_file(char const *sysfs_path, char const *file, char *buffer, size_t buffer_size)
{
    DIR *device_dir = opendir(sysfs_path);
    int fd = openat(dirfd(device_dir), file, O_RDONLY | O_CLOEXEC);
    int err = errno;
    off_t offset = 0;

    closedir(device_dir);

    if (fd == -1) {
        WARN("Failed to open %s/%s: %s\n", sysfs_path, file, strerror(err));
        return FALSE;
    }

    for (;;) {
        ssize_t len = read(fd, buffer + offset, buffer_size - offset);
        if (len == 0)
            break;
        if (len == -1) {
            if (errno == EINTR)
                continue;
            WARN("Failed to read %s/%s: %s\n", sysfs_path, file, strerror(errno));
            close(fd);
            return FALSE;
        }
    }
    close(fd);
    return TRUE;

}

BOOL container_id_for_sysfs(char const *sysfs_path, GUID *container_id)
{
    char device_path[MAX_PATH] = {0};
    char id_product[7] = {0}; // 7 = strlen(0x0b05)+1
    char id_vendor[7] = {0};

    if (!get_device_sysfs_path_from_sys_path(sysfs_path, device_path)) {
        return FALSE;
    }

    if (!read_id_file(device_path, "idProduct", id_product, sizeof(id_product))) {
        return FALSE;
    }

    if (!read_id_file(device_path, "idVendor", id_vendor, sizeof(id_vendor))) {
        return FALSE;
    }

    fill_container_id(device_path, id_product, id_vendor, container_id);
    return TRUE;
}
