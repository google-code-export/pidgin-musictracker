#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

gboolean
get_hash_str(GHashTable *table, const char *key, GString *dest)
{
	GValue* value = (GValue*) g_hash_table_lookup(table, key);
	if (value != NULL && G_VALUE_HOLDS_STRING(value)) {
                g_string_assign(dest, g_value_get_string(value));
                trace("Got info for key '%s' is '%s'", key, dest->str);
                return TRUE;
	}
        return FALSE;
}

unsigned int get_hash_uint(GHashTable *table, const char *key)
{
	GValue* value = (GValue*) g_hash_table_lookup(table, key);
	if (value != NULL && G_VALUE_HOLDS_UINT(value)) {
		return g_value_get_uint(value);
	}
	return 0;
}

gboolean
get_rhythmbox_info(TrackInfo* ti)
{
	DBusGConnection *connection;
	DBusGProxy *player, *shell;
	GError *error = 0;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		trace("Failed to open connection to dbus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (!dbus_g_running(connection, "org.gnome.Rhythmbox")) {
		return FALSE;
	}

	shell = dbus_g_proxy_new_for_name(connection,
			"org.gnome.Rhythmbox",
			"/org/gnome/Rhythmbox/Shell",
			"org.gnome.Rhythmbox.Shell");
	player = dbus_g_proxy_new_for_name(connection,
			"org.gnome.Rhythmbox",
			"/org/gnome/Rhythmbox/Player",
			"org.gnome.Rhythmbox.Player");

	gboolean playing;
	if (!dbus_g_proxy_call_with_timeout(player, "getPlaying", DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &playing,
				G_TYPE_INVALID)) {
		trace("Failed to get playing state from rhythmbox dbus (%s). Assuming player is off", error->message);
		trackinfo_set_status(ti, STATUS_OFF);
		return TRUE;
	}
	
	char *uri;
	if (!dbus_g_proxy_call_with_timeout(player, "getPlayingUri", DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				G_TYPE_STRING, &uri,
				G_TYPE_INVALID)) {
		trace("Failed to get song uri from rhythmbox dbus (%s)", error->message);
		return FALSE;
	}

	GHashTable *table;
	if (!dbus_g_proxy_call_with_timeout(shell, "getSongProperties", DBUS_TIMEOUT, &error,
				G_TYPE_STRING, uri,
				G_TYPE_INVALID, 
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),	&table,
				G_TYPE_INVALID)) {
		if (!playing) {
			trackinfo_set_status(ti, STATUS_OFF);
			return TRUE;
		} else {
			trace("Failed to get song info from rhythmbox dbus (%s)", error->message);
			return FALSE;
		}
	}

	if (playing)
		trackinfo_set_status(ti, STATUS_NORMAL);
	else
		trackinfo_set_status(ti, STATUS_PAUSED);

        // iterate over hashtable keys, adding them as tags
        process_tag_hashtable(table, ti);

        // check if streamtitle is nonempty, if so use that as title
        if (g_hash_table_lookup(table, "rb:stream-song-title"))
          {
            get_hash_str(table, "rb:stream-song-title", trackinfo_get_gstring_track(ti));
          }
        else
          {
            // normalize tag name "title" as "track"
            g_string_assign(trackinfo_get_gstring_track(ti), trackinfo_get_gstring_tag(ti, "title")->str);
          }

	trackinfo_set_totalSecs(ti, get_hash_uint(table, "duration"));
	g_hash_table_destroy(table);

        int currentSecs;
	if (!dbus_g_proxy_call_with_timeout(player, "getElapsed", DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
                                G_TYPE_UINT, &currentSecs,
				G_TYPE_INVALID)) {
		trace("Failed to get elapsed time from rhythmbox dbus (%s)", error->message);
	}
        trackinfo_set_currentSecs(ti, currentSecs);

	return TRUE;
}
