#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

gboolean exaile_dbus_query(DBusGProxy *proxy, const char *method, GString* dest)
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
        g_string_assign(dest, str);
	g_free(str);

        trace("exaile_dbus_query: '%s' => '%s'", method, dest->str);

	return TRUE;
}

gboolean
get_exaile_info(TrackInfo* ti)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = 0;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		trace("Failed to open connection to dbus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (!dbus_g_running(connection, "org.exaile.DBusInterface")) {
		trackinfo_set_status(ti, STATUS_OFF);
		return TRUE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
			"org.exaile.DBusInterface",
			"/DBusInterfaceObject",
			"org.exaile.DBusInterface");

	// We should be using "status" instead of "query" here, but its broken in
	// the current (0.2.6) Exaile version
        GString *buf = g_string_new("");
	if (!exaile_dbus_query(proxy, "query", buf)) {
		trace("Failed to call Exaile dbus method. Assuming player is OFF");
		trackinfo_set_status(ti, STATUS_OFF);
                g_string_free(buf, TRUE);
		return TRUE;
	}

        char status[STRLEN];
	if (sscanf(buf->str, "status: %s", status) == 1) {
		if (!strcmp(status, "playing"))
			trackinfo_set_status(ti, STATUS_NORMAL);
		else
			trackinfo_set_status(ti, STATUS_PAUSED);
	} else {
		trackinfo_set_status(ti, STATUS_OFF);
	}
        g_string_free(buf, TRUE);

	if (trackinfo_get_status(ti) != STATUS_OFF) {
		int mins, secs;
		exaile_dbus_query(proxy, "get_artist", trackinfo_get_gstring_artist(ti));
		exaile_dbus_query(proxy, "get_album", trackinfo_get_gstring_album(ti));
		exaile_dbus_query(proxy, "get_title", trackinfo_get_gstring_track(ti));

                GString *buf = g_string_new("");
		exaile_dbus_query(proxy, "get_length", buf);
		if (sscanf(buf->str, "%d:%d", &mins, &secs)) {
			trackinfo_set_totalSecs(ti, mins*60 + secs);
		}
                g_string_free(buf, TRUE);

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
		trackinfo_set_currentSecs(ti, percentage*trackinfo_get_totalSecs(ti)/100);
	}
	return TRUE;
}
