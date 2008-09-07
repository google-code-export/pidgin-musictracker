#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

void banshee_hash_str(GHashTable *table, const char *key, GString *dest)
{
	GValue* value = (GValue*) g_hash_table_lookup(table, key);
	if (value != NULL && G_VALUE_HOLDS_STRING(value)) {
                g_string_assign(dest, g_value_get_string(value));
	}
}

gboolean banshee_dbus_string(DBusGProxy *proxy, const char *method, GString* dest)
{
	char *str = 0;
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout (proxy, method, DBUS_TIMEOUT, &error,
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
	return TRUE;
}

int banshee_dbus_int(DBusGProxy *proxy, const char *method)
{
	int ret;
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout (proxy, method, DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				G_TYPE_INT, &ret,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call %s: %s", method, error->message);
		return 0;
	}

	return ret;
}

gboolean
get_banshee_info(TrackInfo* ti)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = 0;
	int status;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		trace("Failed to open connection to dbus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (dbus_g_running(connection,"org.gnome.Banshee")) {
		proxy = dbus_g_proxy_new_for_name (connection,
				"org.gnome.Banshee",
				"/org/gnome/Banshee/Player",
				"org.gnome.Banshee.Core");

		if (!dbus_g_proxy_call_with_timeout (proxy, "GetPlayingStatus", DBUS_TIMEOUT, &error,
					G_TYPE_INVALID,
					G_TYPE_INT, &status,
					G_TYPE_INVALID))
		{
			trace("Failed to make dbus call: %s", error->message);
			return FALSE;
		}

		if (status == -1) {
			trackinfo_set_status(ti, STATUS_OFF);
			return TRUE;
		} else if (status == 1)
			trackinfo_set_status(ti, STATUS_NORMAL);
		else
			trackinfo_set_status(ti, STATUS_PAUSED);

		banshee_dbus_string(proxy, "GetPlayingArtist", trackinfo_get_gstring_artist(ti));
		banshee_dbus_string(proxy, "GetPlayingAlbum", trackinfo_get_gstring_album(ti));
		banshee_dbus_string(proxy, "GetPlayingTitle", trackinfo_get_gstring_track(ti));

		trackinfo_set_totalSecs(ti, banshee_dbus_int(proxy, "GetPlayingDuration"));
		trackinfo_set_currentSecs(ti, banshee_dbus_int(proxy, "GetPlayingPosition"));
		return TRUE;
	} else if (dbus_g_running(connection, "org.bansheeproject.Banshee")) { // provide for new interface
		proxy = dbus_g_proxy_new_for_name (connection,
				"org.bansheeproject.Banshee",
				"/org/bansheeproject/Banshee/PlayerEngine",
				"org.bansheeproject.Banshee.PlayerEngine");

                GString *szStatus = g_string_new("");
		banshee_dbus_string(proxy, "GetCurrentState", szStatus);
		if (strcmp(szStatus->str, "idle") == 0) {
			trackinfo_set_status(ti, STATUS_OFF);
		} else if (strcmp(szStatus->str, "playing") == 0)
			trackinfo_set_status(ti, STATUS_NORMAL);
		else
			trackinfo_set_status(ti, STATUS_PAUSED);
                g_string_free(szStatus, TRUE);

                if (trackinfo_get_status(ti) == STATUS_OFF)
                  {
                    return TRUE;
                  } 
		
		GHashTable* table;
		if (!dbus_g_proxy_call_with_timeout (proxy, "GetCurrentTrack", DBUS_TIMEOUT, &error,
					G_TYPE_INVALID,
					dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &table,
					G_TYPE_INVALID))
		{
			trace("Failed to make dbus call: %s", error->message);
			return FALSE;
		}
		
		banshee_hash_str(table, "album", trackinfo_get_gstring_album(ti));
		banshee_hash_str(table, "artist", trackinfo_get_gstring_artist(ti));
		banshee_hash_str(table, "name", trackinfo_get_gstring_track(ti));
		
		g_hash_table_destroy(table);
		
		trackinfo_set_totalSecs(ti, banshee_dbus_int(proxy, "GetLength") / 1000);
		trackinfo_set_currentSecs(ti, banshee_dbus_int(proxy, "GetPosition") / 1000);
		return TRUE;
	}

        return FALSE;
}
