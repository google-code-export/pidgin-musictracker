
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
  int (*xmmsv_dict_entry_get_string)(xmmsv_t *val, const char *key, const char **r);
  int (*xmmsv_dict_entry_get_int)(xmmsv_t *val, const char *key, int32_t *r);
  void (*xmmsc_result_wait)(xmmsc_result_t *res);
  xmmsv_t *(*xmmsc_result_get_value)(xmmsc_result_t *res);
  int (*xmmsv_get_uint)(xmmsv_t *val, uint32_t *r);
  int (*xmmsv_get_string)(const xmmsv_t *val, const char **r);
  int (*xmmsv_get_error)(xmmsv_t *val, const char **errbuf);
  void (*xmmsc_result_unref)(xmmsc_result_t *res);
  xmmsv_t *(*xmmsv_propdict_to_dict)(xmmsv_t *propdict, const char **src_prefs);
  void (*xmmsv_unref)(xmmsv_t *val);
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
          get_func(xmmsv_dict_entry_get_string);
          get_func(xmmsv_dict_entry_get_int);
          get_func(xmmsc_result_wait);
          get_func(xmmsc_result_get_value);
          get_func(xmmsv_get_uint);
          get_func(xmmsv_get_error);
          get_func(xmmsc_result_unref);
          get_func(xmmsv_get_string);
          get_func(xmmsv_propdict_to_dict);
          get_func(xmmsv_unref);
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
gboolean get_xmms2_status(xmmsc_connection_t *conn, struct TrackInfo *ti)
{
	guint status;
	const char *errbuf = NULL;
	xmmsc_result_t *result = NULL;
	xmmsv_t *value = NULL;

        ti->status = PLAYER_STATUS_CLOSED;

	if (!conn || !ti) {
		return FALSE;
	}

	result = (*dl.xmmsc_playback_status)(conn);
	(*dl.xmmsc_result_wait)(result);
	value = (*dl.xmmsc_result_get_value)(result);

	if ((*dl.xmmsv_get_error)(value, &errbuf) ||
	    !(*dl.xmmsv_get_uint)(value, &status)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get playback status,"
		                   " %s.\n", errbuf);
		(*dl.xmmsc_result_unref)(result);
		return FALSE;
	}

	switch (status) {
	case XMMS_PLAYBACK_STATUS_STOP:
		ti->status = PLAYER_STATUS_STOPPED;
		break;
	case XMMS_PLAYBACK_STATUS_PLAY:
		ti->status = PLAYER_STATUS_PLAYING;
		break;
	case XMMS_PLAYBACK_STATUS_PAUSE:
		ti->status = PLAYER_STATUS_PAUSED;
		break;
	}

	(*dl.xmmsc_result_unref)(result);

	return TRUE;
}

/**
 * Gets the media information of the current track.
 */
static
gboolean get_xmms2_mediainfo(xmmsc_connection_t *conn, struct TrackInfo *ti)
{
	guint id;
	gint duration;
	const char *val = NULL;
	const char *errbuf = NULL;
	xmmsc_result_t *result = NULL;
	xmmsv_t *value = NULL;

	if (!conn || !ti) {
		return FALSE;
	}

	result = (*dl.xmmsc_playback_current_id)(conn);
	(*dl.xmmsc_result_wait)(result);
	value = (*dl.xmmsc_result_get_value)(result);

	if ((*dl.xmmsv_get_error)(value, &errbuf) ||
	    !(*dl.xmmsv_get_uint)(value, &id)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get current ID, %s.\n",
		                   errbuf);
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
	value = (*dl.xmmsc_result_get_value)(result);

	if ((*dl.xmmsv_get_error)(value, &errbuf)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get media info, %s.\n",
		                   errbuf);
		(*dl.xmmsc_result_unref)(result);
		return FALSE;
	}

        /* transform propdict dict-of-dicts into a regular dict */
        xmmsv_t *dict = (*dl.xmmsv_propdict_to_dict)(value, NULL);

	if ((*dl.xmmsv_dict_entry_get_string)(dict, "title", &val)) {
		strcpy(ti->track, val);
	}
	if ((*dl.xmmsv_dict_entry_get_string)(dict, "artist", &val)) {
		strcpy(ti->artist, val);
	}
	if ((*dl.xmmsv_dict_entry_get_string)(dict, "album", &val)) {
		strcpy(ti->album, val);
	}
	if ((*dl.xmmsv_dict_entry_get_int)(dict, "duration", &duration)) {
		ti->totalSecs = duration / 1000;
	}

	(*dl.xmmsv_unref)(dict);
	(*dl.xmmsc_result_unref)(result);

	return TRUE;
}

/**
 * Gets the playback time.
 */
static
gboolean get_xmms2_playtime(xmmsc_connection_t *conn, struct TrackInfo *ti)
{
	guint playtime;
	const char *errbuf = NULL;
	xmmsc_result_t *result = NULL;
	xmmsv_t *value = NULL;

	if (!conn || !ti) {
		return FALSE;
	}

	result = (*dl.xmmsc_playback_playtime)(conn);
	(*dl.xmmsc_result_wait)(result);
	value = (*dl.xmmsc_result_get_value)(result);

	if ((*dl.xmmsv_get_error)(value, &errbuf) ||
	    !(*dl.xmmsv_get_uint)(value, &playtime)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get playback time, %s.\n",
		                   errbuf);
		(*dl.xmmsc_result_unref)(result);
		return FALSE;
	}

	ti->currentSecs = playtime / 1000;

	(*dl.xmmsc_result_unref)(result);

	return TRUE;
}

void
get_xmms2_info(struct TrackInfo *ti)
{
	xmmsc_connection_t *connection = NULL;
	const gchar *path = NULL;
	gint ret;

        // have we successfully loaded the xmmsclient library and resolved symbols
        if (!xmms2_dlsym_init())
          {
            return;
          }

	connection = (*dl.xmmsc_init)("musictracker");

	if (!connection) {
		purple_debug_error(PLUGIN_ID, "(XMMS2)"
		                   " Connection initialization failed.\n");
		return;
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
		                   "(XMMS2) Connection to path '%s' failed, %s.\n",
		                   path ? path : "" , (*dl.xmmsc_get_last_error)(connection));
		(*dl.xmmsc_unref)(connection);
		return;
	}

	if (!get_xmms2_status(connection, ti) ||
	    !get_xmms2_mediainfo(connection, ti) ||
	    !get_xmms2_playtime(connection, ti)) {
		(*dl.xmmsc_unref)(connection);
		return;
	}

	(*dl.xmmsc_unref)(connection);
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
