#include "musictracker.h"
#include "utils.h"
#include <string.h>

gboolean exaile_dbus_query(DBusGProxy *proxy, const char *method, char* dest)
{
	char *str = 0;
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout(proxy, method, DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				G_TYPE_STRING, &str,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call %s: %s", method, error->message);
		return FALSE;
	}

	assert(str);
	strncpy(dest, str, STRLEN);
	dest[STRLEN-1] = 0;
	g_free(str);

        trace("exaile_dbus_query: '%s' => '%s'", method, dest);

	return TRUE;
}

void
get_exaile_info(struct TrackInfo* ti)
{
	static DBusGProxy *proxy = 0;
	GError *error = 0;
	char buf[STRLEN], status[STRLEN];

        ti->status = PLAYER_STATUS_CLOSED;

	if (!dbus_g_running("org.exaile.DBusInterface")) {
		return;
	}

        if (!proxy)
          {
            proxy = dbus_g_proxy_new_for_name (connection,
                                               "org.exaile.DBusInterface",
                                               "/DBusInterfaceObject",
                                               "org.exaile.DBusInterface");
          }

	// We should be using "status" instead of "query" here, but its broken in
	// the current (0.2.6) Exaile version
	if (!exaile_dbus_query(proxy, "query", buf)) {
		trace("Failed to call Exaile dbus method. Assuming player is OFF");
		return;
	}

        ti->player = "Exaile";

	if (sscanf(buf, "status: %s", status) == 1) {
		if (!strcmp(status, "playing"))
			ti->status = PLAYER_STATUS_PLAYING;
		else
			ti->status = PLAYER_STATUS_PAUSED;
	} else {
		ti->status = PLAYER_STATUS_STOPPED;
	}

	if (ti->status != PLAYER_STATUS_STOPPED) {
		int mins, secs;
		exaile_dbus_query(proxy, "get_artist", ti->artist);
		exaile_dbus_query(proxy, "get_album", ti->album);
		exaile_dbus_query(proxy, "get_title", ti->track);

		exaile_dbus_query(proxy, "get_length", buf);
		if (sscanf(buf, "%d:%d", &mins, &secs) == 2) {
			ti->totalSecs = mins*60 + secs;
		}

		error = 0;
		unsigned char percentage;
		if (!dbus_g_proxy_call_with_timeout(proxy, "current_position", DBUS_TIMEOUT, &error,
					G_TYPE_INVALID,
					G_TYPE_UCHAR, &percentage,
					G_TYPE_INVALID))
		{
			trace("Failed to make dbus call: %s", error->message);
		}
                trace("exaile_dbus_query: 'current_position' => %d", percentage);
		ti->currentSecs = percentage*ti->totalSecs/100;
	}
}
