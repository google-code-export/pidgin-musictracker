#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

gboolean
get_listen_info(TrackInfo* ti)
{
    DBusGConnection *connection;
    DBusGProxy *proxy;
    GError *error = 0;
    char *buf = 0;
    pcre *re;

    connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (connection == NULL) {
        trace("Failed to open connection to dbus: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    if (!dbus_g_running(connection, "org.gnome.Listen")) {
        trace("org.gnome.Listen not running");
        return FALSE;
    }

    proxy = dbus_g_proxy_new_for_name(connection,
            "org.gnome.Listen",
            "/org/gnome/listen",
            "org.gnome.Listen");

    if (!dbus_g_proxy_call_with_timeout(proxy, "current_playing", DBUS_TIMEOUT, &error,
                           G_TYPE_INVALID,
                           G_TYPE_STRING, &buf,
                           G_TYPE_INVALID))
    {
        trace("Failed to make dbus call: %s", error->message);
        return FALSE;
    }

    if (strcmp(buf, "") == 0) {
        trackinfo_set_status(ti, STATUS_PAUSED);
        return TRUE;
    }

    trackinfo_set_status(ti, STATUS_NORMAL);
    re = regex("^(.*) - \\((.*) - (.*)\\)$", 0);
    capture_gstring(re, buf, strlen(buf), trackinfo_get_gstring_track(ti), trackinfo_get_gstring_album(ti), trackinfo_get_gstring_artist(ti));
    pcre_free(re);

    return TRUE;
}
