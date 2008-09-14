#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

/* More documentation about dbus:
http://dbus.freedesktop.org/doc/dbus-tutorial.html#objects
More documentation about the interface from audacious 1.4:
src/audacious/objects.xml from http://svn.atheme.org/audacious/trunk (can be found through www.google.com/codesearch)
*/

gboolean audacious_dbus_string(DBusGProxy *proxy, const char *method, int pos, const char *arg, GString* dest)
{
	GValue val = {0, };
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout(proxy, method, DBUS_TIMEOUT, &error,
				G_TYPE_UINT, pos,
				G_TYPE_STRING, arg,
				G_TYPE_INVALID,
				G_TYPE_VALUE, &val,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call %s(%d,'%s'): %s", method, pos, arg, error->message);
		return FALSE;
	}

	if (G_VALUE_TYPE(&val) == G_TYPE_STRING) {
                g_string_assign(dest, g_value_get_string(&val));
	}
        else {
          char *s = g_strdup_value_contents(&val);
          g_string_assign(dest, s);
          g_free(s);
        }

	g_value_unset(&val);
	return TRUE;
}

int audacious_dbus_uint(DBusGProxy *proxy, const char *method)
{
	guint ret;
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout(proxy, method, DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				G_TYPE_UINT, &ret,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call %s: %s", method, error->message);
		return 0;
	}

	return ret;
}

int audacious_dbus_int(DBusGProxy *proxy, const char *method, int pos)
{
	int ret;
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout(proxy, method, DBUS_TIMEOUT, &error,
				G_TYPE_UINT,pos,
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
get_audacious_info(TrackInfo* ti)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = 0;
	char *status = 0;
	int pos = 0;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		trace("Failed to open connection to dbus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (!dbus_g_running(connection, "org.atheme.audacious")) {
		trackinfo_set_status(ti, STATUS_OFF);
		return TRUE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
			"org.atheme.audacious",
			"/org/atheme/audacious",
			"org.atheme.audacious");

	if (!dbus_g_proxy_call_with_timeout(proxy, "Status", DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				G_TYPE_STRING, &status,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call: %s", error->message);
		return FALSE;
	}

        trackinfo_set_player(ti, "Audacious");
        
	if (strcmp(status, "stopped") == 0) {
		trackinfo_set_status(ti, STATUS_OFF);
		return TRUE;
	} else if (strcmp(status, "playing") == 0) {
		trackinfo_set_status(ti, STATUS_NORMAL);
	} else {
		trackinfo_set_status(ti, STATUS_PAUSED);
	}
	
	// Find the position in the playlist
	pos = audacious_dbus_uint(proxy, "Position");
	
	trackinfo_set_currentSecs(ti,audacious_dbus_uint(proxy, "Time")/1000);
	trackinfo_set_totalSecs(ti, audacious_dbus_int(proxy, "SongLength", pos));

	// in Audacious 1.4, the Tuple list for the track doesn't seem to be iterable, so we merely 
        // attempt to fetch a known list of possible tuples
        // later Audacious 1.5 provides GetTupleFields method to retrieve a list of standard tuple fields
        // which is presumably similar to this list
        const char *tupleFieldList[] = 
          {
            "artist"         , "title"          , "album"          , "comment"        , "genre"          ,
            "track"          , "track-number"   , "length"         , "year"           , "quality"        ,
            "codec"          , "file-name"      , "file-path"      , "file-ext"       , "song-artist"    ,
            "mtime"          , "formatter"      , "performer"      , "copyright"      , "date"           ,
            "subsong-id"     , "subsong-num"    , 0 
          };

        for (int i = 0; tupleFieldList[i] != 0; i++)
          {
            if (audacious_dbus_string(proxy, "SongTuple", pos, tupleFieldList[i], trackinfo_get_gstring_tag(ti, tupleFieldList[i])))
              {
                trace("tuple field '%s' returned '%s'", tupleFieldList[i], trackinfo_get_gstring_tag(ti, tupleFieldList[i])->str);
              }
          }

	return TRUE;
}
