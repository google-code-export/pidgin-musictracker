#include "musictracker.h"
#include "utils.h"
#include <stdint.h>
#include <string.h>

static int g_state = PLAYER_STATUS_STOPPED;

void quodlibet_hash_str(GHashTable *table, const char *key, char *dest)
{
	const char *value = (const char*) g_hash_table_lookup(table, key);
	if (value != NULL) {
		strncpy(dest, value, STRLEN-1);
		dest[STRLEN-1] = 0;
	} else
		dest[0] = 0;
}

void cb_quodlibet_paused(DBusGProxy *proxy, gpointer data)
{
	g_state = (intptr_t) data;
	trace("quodlibet paused: %d", g_state);
}

void
get_quodlibet_info(struct TrackInfo* ti)
{
	static DBusGProxy *player = 0;
	GError *error = 0;
	char buf[STRLEN];
	static gboolean connected = FALSE;

        ti->status = PLAYER_STATUS_CLOSED;

	if (!dbus_g_running("net.sacredchao.QuodLibet")) {
		return;
	}

        if (!player)
          {
            player = dbus_g_proxy_new_for_name(connection,
                                               "net.sacredchao.QuodLibet",
                                               "/net/sacredchao/QuodLibet",
                                               "net.sacredchao.QuodLibet");
          }

	if (!connected) {
		dbus_g_proxy_add_signal(player, "Paused", G_TYPE_INVALID);
		dbus_g_proxy_connect_signal(player, "Paused", G_CALLBACK(cb_quodlibet_paused),
				(gpointer) PLAYER_STATUS_PAUSED, 0);
		dbus_g_proxy_add_signal(player, "Unpaused", G_TYPE_INVALID);
		dbus_g_proxy_connect_signal(player, "Unpaused", G_CALLBACK(cb_quodlibet_paused),
				(gpointer) PLAYER_STATUS_PLAYING, 0);
		connected = TRUE;
	}

	GHashTable *table;
	if (!dbus_g_proxy_call_with_timeout(player, "CurrentSong", DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_STRING), &table,
				G_TYPE_INVALID)) {
		ti->status = PLAYER_STATUS_STOPPED;
		return;
	}
	ti->status = g_state;

	quodlibet_hash_str(table, "artist", ti->artist);
	quodlibet_hash_str(table, "album", ti->album);
	quodlibet_hash_str(table, "title", ti->track);
	quodlibet_hash_str(table, "~#length", buf);
	sscanf(buf, "%d", &ti->totalSecs);
	g_hash_table_destroy(table);
}
