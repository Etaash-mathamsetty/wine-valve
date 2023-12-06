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

#include <pthread.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <dlfcn.h>
#include <poll.h>

#include <dbus/dbus.h>

#include "wine/list.h"
#include "wine/unixlib.h"
#include "wine/gdi_driver.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winesni);

#define DBUS_FUNCS               \
    DO_FUNC(dbus_bus_get_private); \
    DO_FUNC(dbus_connection_dispatch); \
    DO_FUNC(dbus_connection_get_dispatch_status); \
    DO_FUNC(dbus_connection_flush); \
    DO_FUNC(dbus_connection_close);  \
    DO_FUNC(dbus_connection_ref);   \
    DO_FUNC(dbus_connection_unref);   \
    DO_FUNC(dbus_connection_set_watch_functions);   \
    DO_FUNC(dbus_watch_get_unix_fd); \
    DO_FUNC(dbus_watch_handle); \
    DO_FUNC(dbus_watch_get_flags); \
    DO_FUNC(dbus_watch_get_enabled); \
    DO_FUNC(dbus_error_free); \
    DO_FUNC(dbus_error_init); \
    DO_FUNC(dbus_error_is_set); \
    DO_FUNC(dbus_threads_init_default)

#define DO_FUNC(f) static typeof(f) * p_##f
DBUS_FUNCS;
#undef DO_FUNC

static pthread_once_t init_control = PTHREAD_ONCE_INIT;

static void* dbus_module = NULL;

static DBusConnection *global_connection;
static DBusWatch *global_connection_watch;
static int global_connection_watch_fd;
static UINT global_connection_watch_flags;
static BOOL sni_initialized = FALSE;

static BOOL load_dbus_functions(void)
{
    if (!(dbus_module = dlopen( SONAME_LIBDBUS_1, RTLD_NOW )))
        goto failed;

#define DO_FUNC(f) if (!(p_##f = dlsym( dbus_module, #f ))) goto failed
    DBUS_FUNCS;
#undef DO_FUNC
    return TRUE;

failed:
    WARN( "failed to load DBUS support: %s\n", dlerror() );
    return FALSE;
}

static void dbus_finalize(void)
{
    if (global_connection != NULL)
    {
        p_dbus_connection_flush(global_connection);
        p_dbus_connection_close(global_connection);
        p_dbus_connection_unref(global_connection);
    }
    if (dbus_module != NULL)
    {
        dlclose(dbus_module);
    }
}

static dbus_bool_t add_watch(DBusWatch *w, void *data);
static void remove_watch(DBusWatch *w, void *data);
static void toggle_watch(DBusWatch *w, void *data);

static BOOL dbus_initialize(void)
{
    DBusError error;
    p_dbus_error_init( &error );
    if (!p_dbus_threads_init_default()) return FALSE;
    if (!(global_connection = p_dbus_bus_get_private( DBUS_BUS_SESSION, &error )))
    {
        WARN("failed to get system dbus connection: %s\n", error.message );
        p_dbus_error_free( &error );
        return FALSE;
    }

    if (!p_dbus_connection_set_watch_functions(global_connection, add_watch, remove_watch,
                                               toggle_watch, NULL, NULL))
    {
        WARN("dbus_set_watch_functions() failed\n");
        return FALSE;
    }
    return TRUE;
}

static void snidrv_once_initialize(void)
{
    if (!load_dbus_functions()) goto err;
    if (!dbus_initialize()) goto err;
    /* TODO: replace this with Interlocked if there will be a getter function for this variable */
    sni_initialized = TRUE;
err:
    if (!sni_initialized)
    {
        dbus_finalize();
    }
}

BOOL snidrv_init(void)
{
    pthread_once(&init_control, snidrv_once_initialize);
    return sni_initialized;
}

static dbus_bool_t add_watch(DBusWatch *w, void *data)
{
    int fd;
    unsigned int flags, poll_flags;
    if (!p_dbus_watch_get_enabled(w))
        return TRUE;

    fd = p_dbus_watch_get_unix_fd(w);
    flags = p_dbus_watch_get_flags(w);
    poll_flags = 0;

    if (flags & DBUS_WATCH_READABLE)
        poll_flags |= POLLIN;
    if (flags & DBUS_WATCH_WRITABLE)
        poll_flags |= POLLOUT;

    /* global connection */
    global_connection_watch_fd = fd;
    global_connection_watch_flags = poll_flags;
    global_connection_watch = w;

    return TRUE;
}

static void remove_watch(DBusWatch *w, void *data)
{
    /* global connection */
    global_connection_watch_fd = 0;
    global_connection_watch_flags = 0;
    global_connection_watch = NULL;
}


static void toggle_watch(DBusWatch *w, void *data)
{
    if (p_dbus_watch_get_enabled(w))
        add_watch(w, data);
    else
        remove_watch(w, data);
}

BOOL snidrv_run_loop()
{
    while (true)
    {
        int poll_ret;
        struct pollfd fd_info;
        DBusConnection* conn;
        /* TODO: add condvar */
        if (!global_connection_watch_fd) continue;

        conn = p_dbus_connection_ref(global_connection);
        fd_info = (struct pollfd) {
            .fd = global_connection_watch_fd,
            .events = global_connection_watch_flags,
            .revents = 0,
        };

        poll_ret = poll(&fd_info, 1, 100);
        if (poll_ret == 0)
            goto cleanup;
        if (poll_ret == -1)
        {
            ERR("fd poll error\n");
            goto cleanup;
        }

        if (fd_info.revents & (POLLERR | POLLHUP | POLLNVAL)) continue;
        if (fd_info.revents & POLLIN)
        {
            p_dbus_watch_handle(global_connection_watch, DBUS_WATCH_READABLE);
            while ( p_dbus_connection_get_dispatch_status ( conn ) == DBUS_DISPATCH_DATA_REMAINS )
            {
                p_dbus_connection_dispatch ( conn ) ;
            }
        }

        if (fd_info.revents & POLLOUT)
            p_dbus_watch_handle(global_connection_watch, DBUS_WATCH_WRITABLE);
    cleanup:
        p_dbus_connection_unref(conn);
    }

    return 0;
}

#endif
