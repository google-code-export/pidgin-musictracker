#include "musictracker.h"
#include "utils.h"
#include <string.h>

void
get_listen_info(struct TrackInfo* ti)
{
    static DBusGProxy *proxy = 0;
    GError *error = 0;
    char *buf = 0;
    pcre *re;

    ti->status = PLAYER_STATUS_CLOSED;

    if (!dbus_g_running("org.gnome.Listen")) {
        return;
    }

    if (!proxy)
      {
        proxy = dbus_g_proxy_new_for_name(connection,
                                          "org.gnome.Listen",
                                          "/org/gnome/listen",
                                          "org.gnome.Listen");
      }

    if (!dbus_g_proxy_call_with_timeout(proxy, "current_playing", DBUS_TIMEOUT, &error,
                           G_TYPE_INVALID,
                           G_TYPE_STRING, &buf,
                           G_TYPE_INVALID))
    {
        trace("Failed to make dbus call: %s", error->message);
        return;
    }

    if (strcmp(buf, "") == 0) {
        ti->status = PLAYER_STATUS_PAUSED;
        return;
    }

    ti->status = PLAYER_STATUS_PLAYING;
    re = regex("^(.*) - \\((.*) - (.*)\\)$", 0);
    capture(re, buf, strlen(buf), ti->track, ti->album, ti->artist);
    pcre_free(re);
    g_free(buf);
}
