
#include <config.h>
#ifdef HAVE_XMMSCLIENT

#include <dlfcn.h>
#include <string.h>
#include <xmmsclient/xmmsclient.h>

#include "musictracker.h"
#include "utils.h"

struct xmmsclientlib
{
  void *handle;
  xmmsc_connection_t *(*xmmsc_init)(const char *clientname);
  int (*xmmsc_connect)(xmmsc_connection_t *, const char *);
  void (*xmmsc_unref)(xmmsc_connection_t *c);
  char *(*xmmsc_get_last_error)(xmmsc_connection_t *c);
  xmmsc_result_t *(*xmmsc_playback_status)(xmmsc_connection_t *c);
  xmmsc_result_t *(*xmmsc_playback_current_id)(xmmsc_connection_t *c);
  xmmsc_result_t *(*xmmsc_playback_playtime)(xmmsc_connection_t *c);
  xmmsc_result_t *(*xmmsc_medialib_get_info)(xmmsc_connection_t *, uint32_t);
  int (*xmmsc_result_get_dict_entry_string)(xmmsc_result_t *res, const char *key, const char **r);
  int (*xmmsc_result_get_dict_entry_int)(xmmsc_result_t *res, const char *key, int32_t *r);
  int (*xmmsc_result_propdict_foreach)(xmmsc_result_t *res, xmmsc_propdict_foreach_func func, void *user_data);
  void (*xmmsc_result_wait)(xmmsc_result_t *res);
  int (*xmmsc_result_iserror)(xmmsc_result_t *res);
  int (*xmmsc_result_get_uint)(xmmsc_result_t *res, uint32_t *r);
  const char *(*xmmsc_result_get_error)(xmmsc_result_t *res);
  void (*xmmsc_result_unref)(xmmsc_result_t *res);
};

static struct xmmsclientlib dl;

#define get_func(name) dl.name = dlsym(handle, #name) ; \
                       if (dl.name == NULL) {trace("(XMMS2) could not resolve symbol %s in libxmmsclient.so", #name); dlclose(handle); return FALSE; } ; 

static
void *xmms2_dlsym_init(void)
{
  if (!dl.handle)
    {
      void *handle = dlopen("libxmmsclient.so", RTLD_NOW);
      if (handle)
        {
          get_func(xmmsc_init);
          get_func(xmmsc_connect);
          get_func(xmmsc_unref);
          get_func(xmmsc_get_last_error);
          get_func(xmmsc_playback_status);
          get_func(xmmsc_playback_current_id);
          get_func(xmmsc_playback_playtime);
          get_func(xmmsc_medialib_get_info);
          get_func(xmmsc_result_get_dict_entry_string);
          get_func(xmmsc_result_get_dict_entry_int);
          get_func(xmmsc_result_propdict_foreach);
          get_func(xmmsc_result_wait);                
          get_func(xmmsc_result_iserror);                
          get_func(xmmsc_result_get_uint);
          get_func(xmmsc_result_get_error);
          get_func(xmmsc_result_unref);
          dl.handle = handle;
        }
      else
        {
          trace("(XMMS2) Failed to load library libxmmsclient.so");
        }
    }
  return dl.handle;
}

/**
 * Gets the playback status.
 */
static
gboolean get_xmms2_status(xmmsc_connection_t *conn, TrackInfo *ti)
{
	guint status;
	xmmsc_result_t *result = NULL;

	if (!conn || !ti) {
		return FALSE;
	}

	result = (*dl.xmmsc_playback_status)(conn);
	(*dl.xmmsc_result_wait)(result);

	if ((*dl.xmmsc_result_iserror)(result) ||
	    !(*dl.xmmsc_result_get_uint)(result, &status)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get playback status,"
		                   " %s.\n", (*dl.xmmsc_result_get_error)(result));
		(*dl.xmmsc_result_unref)(result);
		return FALSE;
	}

	switch (status) {
	case XMMS_PLAYBACK_STATUS_STOP:
		trackinfo_set_status(ti, STATUS_OFF);
		break;
	case XMMS_PLAYBACK_STATUS_PLAY:
		trackinfo_set_status(ti, STATUS_NORMAL);
		break;
	case XMMS_PLAYBACK_STATUS_PAUSE:
		trackinfo_set_status(ti, STATUS_PAUSED);
		break;
	}

	(*dl.xmmsc_result_unref)(result);

	return TRUE;
}

/**
 * Helper function applied to each key in the media information result
 */
static
void mediainfo_helper(const void *key, xmmsc_result_value_type_t type, const void *value, const char *source, void *user_data)
{
  TrackInfo *ti = (TrackInfo *)user_data;
  
  switch (type)
    {
    case XMMSC_RESULT_VALUE_TYPE_STRING:
      g_string_assign(trackinfo_get_gstring_tag(ti, key), (char *)value);
      trace("key '%s' value '%s' source '%s'", key, value, source);
      break;
    case XMMSC_RESULT_VALUE_TYPE_INT32:
    case XMMSC_RESULT_VALUE_TYPE_UINT32:
      {
        g_string_printf(trackinfo_get_gstring_tag(ti, key), "%d", value);
        trace("key '%s' value %d source '%s'", key, value, source);
      }
      break;
    default:
      trace("unknown type %d", type);
    }
}


/**
 * Gets the media information of the current track.
 */
static
gboolean get_xmms2_mediainfo(xmmsc_connection_t *conn, TrackInfo *ti)
{
	guint id;
	gint duration;
	const char *val = NULL;
	xmmsc_result_t *result = NULL;

	if (!conn || !ti) {
		return FALSE;
	}

	result = (*dl.xmmsc_playback_current_id)(conn);
	(*dl.xmmsc_result_wait)(result);

	if ((*dl.xmmsc_result_iserror)(result) ||
	    !(*dl.xmmsc_result_get_uint)(result, &id)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get current ID, %s.\n",
		                   (*dl.xmmsc_result_get_error)(result));
		(*dl.xmmsc_result_unref)(result);
		return FALSE;
	}

	(*dl.xmmsc_result_unref)(result);

	/* Nothing is being played if the ID is 0. */
	if (id == 0) {
		purple_debug_info(PLUGIN_ID, "(XMMS2) Stopped.\n");
		return FALSE;
	}

	result = (*dl.xmmsc_medialib_get_info)(conn, id);
	(*dl.xmmsc_result_wait)(result);

	if ((*dl.xmmsc_result_iserror)(result)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get media info, %s.\n",
		                   (*dl.xmmsc_result_get_error)(result));
		(*dl.xmmsc_result_unref)(result);
		return FALSE;
	}

        if ((*dl.xmmsc_result_propdict_foreach)(result, mediainfo_helper, ti) == 0)
          {
            trace("xmmsc_result_propdict_foreach failed");
          }

        // normalize tag "date" as "year"
        g_string_assign(trackinfo_get_gstring_tag(ti, "year"), trackinfo_get_gstring_tag(ti, "date")->str);

	if ((*dl.xmmsc_result_get_dict_entry_int)(result, "duration", &duration)) {
		trackinfo_set_totalSecs(ti, duration / 1000);
	}

	(*dl.xmmsc_result_unref)(result);

	return TRUE;
}

/**
 * Gets the playback time.
 */
static
gboolean get_xmms2_playtime(xmmsc_connection_t *conn, TrackInfo *ti)
{
	guint playtime;
	xmmsc_result_t *result = NULL;

	if (!conn || !ti) {
		return FALSE;
	}

	result = (*dl.xmmsc_playback_playtime)(conn);
	(*dl.xmmsc_result_wait)(result);

	if ((*dl.xmmsc_result_iserror)(result) ||
	    !(*dl.xmmsc_result_get_uint)(result, &playtime)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get playback time, %s.\n",
		                   (*dl.xmmsc_result_get_error)(result));
		(*dl.xmmsc_result_unref)(result);
		return FALSE;
	}

	trackinfo_set_currentSecs(ti, playtime / 1000);

	(*dl.xmmsc_result_unref)(result);

	return TRUE;
}

gboolean get_xmms2_info(TrackInfo *ti)
{
	xmmsc_connection_t *connection = NULL;
	const gchar *path = NULL;
	gint ret;

        // have we successfully loaded the xmmsclient library and resolved symbols
        if (!xmms2_dlsym_init())
          {
            return FALSE;
          }

	connection = (*dl.xmmsc_init)("musictracker");

	if (!connection) {
		purple_debug_error(PLUGIN_ID, "(XMMS2)"
		                   " Connection initialization failed.\n");
		return FALSE;
	}

	path = getenv("XMMS_PATH");
	if (!path) {
		path = purple_prefs_get_string(PREF_XMMS2_PATH);

		if (strcmp(path, "") == 0) {
			path = NULL;
		}
	}

	ret = (*dl.xmmsc_connect)(connection, path);
	if (!ret) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Connection to %s failed, %s.\n",
		                   path, (*dl.xmmsc_get_last_error)(connection));
		(*dl.xmmsc_unref)(connection);
		return FALSE;
	}

	if (!get_xmms2_status(connection, ti) ||
	    !get_xmms2_mediainfo(connection, ti) ||
	    !get_xmms2_playtime(connection, ti)) {
		(*dl.xmmsc_unref)(connection);
		return FALSE;
	}

	(*dl.xmmsc_unref)(connection);

	return TRUE;
}

static
void cb_xmms2_pref_changed(GtkWidget *entry, gpointer data)
{
	const gchar *path = gtk_entry_get_text(GTK_ENTRY(entry));
	const char *pref = (const char*) data;

	purple_prefs_set_string(pref, path);
}

void get_xmms2_pref(GtkBox *vbox)
{
	GtkWidget *entry, *hbox;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Path:"),
	                   FALSE, FALSE, 0);

	entry = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(entry),
	                   purple_prefs_get_string(PREF_XMMS2_PATH));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "changed",
	                 G_CALLBACK(cb_xmms2_pref_changed),
	                 (gpointer) PREF_XMMS2_PATH);
}

#endif
