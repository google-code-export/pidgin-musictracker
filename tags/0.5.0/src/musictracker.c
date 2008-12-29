#ifndef WIN32
#include "config.h"
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "musictracker.h"
#include "actions.h"
#include "utils.h"
#include "filter.h"

#include "core.h"
#include "prefs.h"
#include "util.h"
#include "buddyicon.h"
#include "account.h"
#include "status.h"
#include "cmds.h"
#include "connection.h"
#include "signals.h"
#include "version.h"
#include "notify.h"
#include "plugin.h"
#include "version.h"
#include "gtkplugin.h"

static guint g_tid;
static PurplePlugin *g_plugin;
static gboolean g_run=1;
static TrackInfo *mostrecent_ti = 0;
static PurpleCmdId cmdid_nowplaying;
static PurpleCmdId cmdid_np;

//--------------------------------------------------------------------

#ifndef WIN32
gboolean get_amarok_info(TrackInfo* ti);
gboolean get_xmms_info(TrackInfo* ti);
gboolean get_audacious_legacy_info(TrackInfo* ti);
gboolean get_audacious_info(TrackInfo* ti);
gboolean get_rhythmbox_info(TrackInfo* ti);
gboolean get_exaile_info(TrackInfo* ti);
gboolean get_banshee_info(TrackInfo* ti);
gboolean get_quodlibet_info(TrackInfo* ti);
gboolean get_listen_info(TrackInfo* ti);
gboolean get_xmms2_info(TrackInfo* ti);
gboolean get_squeezecenter_info(TrackInfo* ti);
gboolean get_mpris_info(TrackInfo* ti);

void get_xmmsctrl_pref(GtkBox *box);
void get_xmms2_pref(GtkBox *box);
void get_squeezecenter_pref(GtkBox *box);

#else
gboolean get_foobar2000_info(TrackInfo* ti);
gboolean get_winamp_info(TrackInfo* ti);
gboolean get_wmp_info(TrackInfo* ti);
gboolean get_itunes_info(TrackInfo* ti);
gboolean get_msn_compat_info(TrackInfo *ti);
#endif

gboolean get_mpd_info(TrackInfo* ti);
gboolean get_lastfm_info(TrackInfo* ti);

void get_mpd_pref(GtkBox *box);
void get_lastfm_pref(GtkBox *box);

// Global array of players
struct PlayerInfo g_players[] = {
#ifndef WIN32
	{ "XMMS", get_xmms_info, get_xmmsctrl_pref },
	{ "Audacious < 1.4", get_audacious_legacy_info, get_xmmsctrl_pref },
	{ "Audacious >= 1.4", get_audacious_info, 0 },
	{ "Amarok", get_amarok_info, 0 },
	{ "Rhythmbox", get_rhythmbox_info, 0 },
	{ "Banshee", get_banshee_info, 0 },
	{ "QuodLibet", get_quodlibet_info, 0 },
	{ "Exaile", get_exaile_info, 0 },
	{ "Listen", get_listen_info, 0 },
	{ "SqueezeCenter", get_squeezecenter_info, get_squeezecenter_pref },
#ifdef HAVE_XMMSCLIENT
 	{ "XMMS2", get_xmms2_info, get_xmms2_pref },
#endif
 	{ "MPRIS", get_mpris_info, 0 },
#else
	{ "Winamp", get_winamp_info, 0 },
	{ "Windows Media Player", get_wmp_info, 0 },
	{ "iTunes", get_itunes_info, 0 },
	{ "Messenger compatible" , get_msn_compat_info, 0 },
	{ "Foobar2000", get_foobar2000_info, 0 },
#endif
	{ "MPD", get_mpd_info, get_mpd_pref },
 	{ "Last.fm", get_lastfm_info, get_lastfm_pref },
	{ "", 0, 0 } // dummy to end the array
};

//--------------------------------------------------------------------

static
gboolean
purple_status_is_away (PurpleStatus *status)
{
	PurpleStatusPrimitive	primitive	= PURPLE_STATUS_UNSET;

	if (!status)
		return FALSE;

	primitive	= purple_status_type_get_primitive (purple_status_get_type (status));

	return ((primitive == PURPLE_STATUS_AWAY) || (primitive == PURPLE_STATUS_EXTENDED_AWAY));
}

//--------------------------------------------------------------------

static
gboolean
purple_status_supports_attr (PurpleStatus *status, const char *id)
{
	gboolean		b				= FALSE;
	PurpleStatusType	*status_type	= NULL;
	GList			*attrs			= NULL;
	PurpleStatusAttr	*attr			= NULL;

	if (!status || !id)
		return b;

	status_type	= purple_status_get_type(status);

	if (status_type != NULL)
	{
		attrs	= purple_status_type_get_attrs (status_type);

		while (attrs != NULL)
		{
			attr	= (PurpleStatusAttr *)attrs->data;

			if (attr != NULL)
			{
				if (strncasecmp(id, purple_status_attr_get_id (attr), strlen(id)) == 0)
					b	= TRUE;
			}

			attrs	= attrs->next;
		}
	}

	return b;
}

//--------------------------------------------------------------------

static gboolean
message_changed(const char *one, const char *two)
{
	if (one == NULL && two == NULL)
		return FALSE;

	if (one == NULL || two == NULL)
		return TRUE;

	return (g_utf8_collate(one, two) != 0);
}

//--------------------------------------------------------------------

static
char * put_tags(TrackInfo *ti, char *buf)
{
  int ovector[6];

  // match for %{.*}, ungreedily
  pcre *re = regex("(%\\{.*\\})", PCRE_UNGREEDY);

  int count;
  do 
    {
      count = pcre_exec(re, 0, buf, strlen(buf), 0, 0, ovector, 6);
      trace("pcre_exec returned %d", count);

      if (count > 0)
        {
          // extract tag from the match 
          int tag_length = ovector[3] - ovector[2];
          char *tag = malloc(tag_length+1);
          memcpy(tag, buf+ovector[2]+2, tag_length-3);
          tag[tag_length-3] = 0;

          trace("found tag '%s'", tag);
          
          // locate tag in tag hash
          // (this implcitly generates a new tag containing an empty string if not found)
          GString *string = trackinfo_get_gstring_tag(ti, tag);

          // replace the found match with value
          char *newbuf = malloc(strlen(buf) - tag_length + strlen(string->str) + 1);

          memcpy(newbuf, buf, ovector[2]);
          strcpy(newbuf + ovector[2], string->str);
          strcat(newbuf, buf+ovector[3]);
          free(buf);
          free(tag);
          buf = newbuf;
          trace("replaced to give %s", buf);
        }
    }
  while (count >0);

  pcre_free(re);

  return buf;
}

//--------------------------------------------------------------------

static
char* generate_status(const char *src, TrackInfo *ti, const char *savedstatus)
{
	trace("Status format: %s", src);

	char *status = malloc(strlen(src)+1);
	strcpy(status, src);
	status = put_field(status, 'p', trackinfo_get_artist(ti));
	status = put_field(status, 'a', trackinfo_get_album(ti));
	status = put_field(status, 't', trackinfo_get_track(ti));
	status = put_field(status, 'r', trackinfo_get_player(ti));

	// Duration
	char buf[20];
	sprintf(buf, "%d:%02d", trackinfo_get_totalSecs(ti)/60, trackinfo_get_totalSecs(ti)%60);
	status = put_field(status, 'd', buf);

	// Current time
	sprintf(buf, "%d:%02d", trackinfo_get_currentSecs(ti)/60, trackinfo_get_currentSecs(ti)%60);
	status = put_field(status, 'c', buf);

	// Progress bar
	int i, progress;
	if (trackinfo_get_totalSecs(ti) == 0)
		progress = 0;
	else 
		progress = (int)floor(trackinfo_get_currentSecs(ti) * 10.0 / trackinfo_get_totalSecs(ti));
	if (progress >= 10)
		progress = 9;
	for (i=0; i<10; ++i)
		buf[i] = '-';
	buf[progress] = '|';
	buf[10] = 0;
	status = put_field(status, 'b', buf);

        // Music symbol: U+266B 'beamed eighth notes'
	status = put_field(status, 'm', "\u266b");

        // The status message selected through the UI
	status = put_field(status, 's', savedstatus);

        // scan for %{tag} constructs and replace them with the tag information from the trackinfo, or '' if undefined
        status = put_tags(ti, status);

	trace("Formatted status: %s", status);

	if (purple_prefs_get_bool(PREF_FILTER_ENABLE)) {
		filter(status);
		trace("Filtered status: %s", status);
	}

        return status;
}

//--------------------------------------------------------------------

// Updates 'user tune' status primitive with current track info
static
gboolean
set_status_tune (PurpleAccount *account, gboolean validStatus, TrackInfo *ti)
{
	PurpleStatus *status			= NULL;
	PurplePresence* presence = NULL;
	gboolean active = FALSE;

	if (validStatus)
	{
		if (ti == NULL)
			return FALSE;
		active = (trackinfo_get_status(ti) == STATUS_NORMAL) || (trackinfo_get_status(ti) == STATUS_PAUSED);
	}
	else
	{		
		active = FALSE;
	}

	presence = purple_account_get_presence (account);

	status = purple_presence_get_status(presence, purple_primitive_get_id_from_type(PURPLE_STATUS_TUNE));
	if (status == NULL)
	{
		trace("Can't get primitive status tune for account %s, protocol %s", 
                      purple_account_get_username(account), purple_account_get_protocol_name(account));
		return FALSE;
	}

        // don't need to do anything if track info hasn't changed
        if (!trackinfo_changed(ti, mostrecent_ti))
          {
            trace("trackinfo hasn't changed, not doing anything to tune status");
            return TRUE;
          }

	trace("For account %s protocol %s user tune active %s", 
              purple_account_get_username(account), purple_account_get_protocol_name(account),
              active?"true":"false");

	if (active)
	{
                GList *attrs = NULL;
                attrs = g_list_append(attrs, PURPLE_TUNE_ARTIST);
                attrs = g_list_append(attrs, trackinfo_get_artist(ti));
                attrs = g_list_append(attrs, PURPLE_TUNE_TITLE);
                attrs = g_list_append(attrs, trackinfo_get_track(ti));
                attrs = g_list_append(attrs, PURPLE_TUNE_ALBUM);
                attrs = g_list_append(attrs, trackinfo_get_album(ti));
                attrs = g_list_append(attrs, PURPLE_TUNE_TIME);
                attrs = g_list_append(attrs, (gpointer) (intptr_t) trackinfo_get_totalSecs(ti));
                purple_status_set_active_with_attrs_list(status, TRUE, attrs);
                g_list_free(attrs);
	}
	else
	{
                // purple_status_set_active_with_attrs_list() in current libpurple has the slight
                // misfeature that implicitly resetting attributes to their default values always
                // sets the changed flag, making this call non-idempotent (in the sense that it may
                // cause protocol to do all the work of sending the status again, even though it
                // hasn't actually changed).  Explicitly resetting the attributes works around that...
                GList *attrs = NULL;
                attrs = g_list_append(attrs, PURPLE_TUNE_ARTIST);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_TITLE);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_ALBUM);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_TIME);
                attrs = g_list_append(attrs, 0);
                purple_status_set_active_with_attrs_list(status, FALSE, attrs);
                g_list_free(attrs);
	}
	
	return TRUE;
}

//--------------------------------------------------------------------

gboolean
set_status (PurpleAccount *account, TrackInfo *ti)
{
	char buf[STRLEN];

        // check for status changing disabled for this account
	build_pref(buf, PREF_CUSTOM_DISABLED, 
			purple_account_get_username(account),
			purple_account_get_protocol_name(account));
	if (purple_prefs_get_bool(buf)) {
		trace("Status changing disabled for %s account", purple_account_get_username(account));
	}

        // set 'now playing' status
        if (set_status_tune(account, ti != 0, ti) && purple_prefs_get_bool(PREF_NOW_LISTENING_ONLY))
          {
            return TRUE;
          }

        // have we requested 'away' status to take priority?
	PurpleStatus *status = purple_account_get_active_status (account);
	if (status != NULL)
	{
          if (purple_prefs_get_bool(PREF_DISABLE_WHEN_AWAY) && purple_status_is_away(status))
            {
              trace("Status is away and we are disabled when away");
              return TRUE;
            }
	}

        // discover the pidgin saved status in use for this account
        const char *savedmessage = "";
        {
            PurpleSavedStatus *savedstatus = purple_savedstatus_get_current();
            if (savedstatus)
              {
                PurpleSavedStatusSub *savedsubstatus = purple_savedstatus_get_substatus(savedstatus, account);
                if (savedsubstatus)
                  {
                    // use account-specific saved status
                    savedmessage = purple_savedstatus_substatus_get_message(savedsubstatus);
                  }
                else
                  {
                    // don't have an account-specific saved status, use the general one
                    savedmessage = purple_savedstatus_get_message(savedstatus);
                  }
              }
        }


        char *text = 0;
        if (ti)
          {
            switch (trackinfo_get_status(ti))
              {
              case STATUS_OFF:
                text = generate_status(purple_prefs_get_string(PREF_OFF), ti, savedmessage);
                break;
              case STATUS_PAUSED:
                text = generate_status(purple_prefs_get_string(PREF_PAUSED), ti, savedmessage);
                break;
              case STATUS_NORMAL:
                {
                  // check for protocol status format override for this account
                  build_pref(buf, PREF_CUSTOM_FORMAT, 
                             purple_account_get_username(account),
                             purple_account_get_protocol_name(account));
                  const char *override = purple_prefs_get_string(buf);
                  
                  if (override && (*override != 0))
                    {
                      // if so, use account specific status format
                      text = generate_status(override, ti, savedmessage);
                    }
                  else
                    {
                      // otherwise, use the general status format
                      text = generate_status(purple_prefs_get_string(PREF_FORMAT), ti, savedmessage);
                    }
                }
                
                break;
              default:
                trace("unknown player status %d", trackinfo_get_status(ti));
              }
          }

        if (text == 0)
          text = strdup("");

        // if the status is empty, use the current status selected through the UI (if there is one)
        if (strlen(text) == 0)
          {
            if (savedmessage != 0)
              {
                trace("empty player status, using current saved status....");
                free(text);
                text = strdup(savedmessage);
              }
          }

        // set the status message
        if (purple_status_supports_attr (status, "message"))
          {
            if ((text != NULL) && message_changed(text, purple_status_get_attr_string(status, "message")))
              {
                trace("Setting %s status to: %s", purple_account_get_username (account), text);
                GList *attrs = NULL;
                attrs = g_list_append(attrs, "message");
                attrs = g_list_append(attrs, (gpointer)text);
                purple_status_set_active_with_attrs_list(status, TRUE, attrs);
                g_list_free(attrs);  
              }
          }

        free(text);
	return TRUE;
}

//--------------------------------------------------------------------

void
set_userstatus_for_active_accounts (TrackInfo *ti)
{
        GList                   *accounts               = NULL,
                                        *head                   = NULL;
        PurpleAccount             *account                = NULL;

	gboolean b = purple_prefs_get_bool(PREF_DISABLED);
	if (b) {
		trace("Disabled flag on!");
	}
        else
          {
            head = accounts = purple_accounts_get_all_active ();
            
            while (accounts != NULL)
              {
                account = (PurpleAccount *)accounts->data;
                
                if (account != NULL)
                        set_status (account, ti);
                
                accounts = accounts->next;
              }
            
            
            trace("Status set for all accounts");
          }

        // keep a copy of the most recent trackinfo in case we need it elsewhere....
        if (mostrecent_ti)
            trackinfo_destroy(mostrecent_ti);

        if (ti)
          {
            mostrecent_ti = trackinfo_new();
            trackinfo_assign(mostrecent_ti, ti);
          }         
        else        
          mostrecent_ti = 0;

        if (head != NULL)
                g_list_free (head);
}

//--------------------------------------------------------------------

static void utf8_validate(char* text)
{
  if (!g_utf8_validate(text,-1,NULL))
    {
      // we expect the player-specific track info retrieval code to get the track info strings in utf-8
      // if that failed to happen, let's assume it's in the locale encoding and convert from that
      char *converted_text = g_locale_to_utf8(text,-1,NULL,NULL,NULL);
      if (converted_text)
        {
          strcpy(text, converted_text);
          g_free(converted_text);
          trace("Converted from locale to valid utf-8 '%s'", text);
        }
      else
        {
          // conversion from locale encoding failed
          // replace invalid sequences with '?' so we end up with a valid utf-8 string
          // (as required by other glib routines used by purple core)
          const gchar *end;
          while (!g_utf8_validate(text,-1,&end))
              {
                *(gchar *)end = '?';
              }
          trace("After removal of invalid utf-8 '%s'", text);
        }
    }
}

//--------------------------------------------------------------------

static gboolean
cb_timeout(gpointer data) {
	if (g_run == 0)
		return FALSE;

        gboolean b = TRUE;

        TrackInfo *ti = trackinfo_new();
	trackinfo_set_status(ti, STATUS_OFF);

	int player = purple_prefs_get_int(PREF_PLAYER);
	if (player != -1) {
                trackinfo_set_player(ti, g_players[player].name);
		b = (*g_players[player].track_func)(ti);
	} else {
		int i = 0;
		while (strlen(g_players[i].name) && (!b || trackinfo_get_status(ti) == STATUS_OFF)) {
                        trackinfo_set_player(ti, g_players[i].name);
			b = (*g_players[i].track_func)(ti);
			++i;
		}
	}

	if (!b) {
		trace("Getting info failed. Setting empty status.");
		set_userstatus_for_active_accounts(0);
	}
        else {
	trim(trackinfo_get_album(ti));
	trim(trackinfo_get_track(ti));
	trim(trackinfo_get_artist(ti));
	trace("%s,%s,%s,%s,%d", trackinfo_get_player(ti), trackinfo_get_artist(ti), trackinfo_get_album(ti), trackinfo_get_track(ti), trackinfo_get_status(ti));

        // ensure track information is valid utf-8
        utf8_validate(trackinfo_get_album(ti));
        utf8_validate(trackinfo_get_track(ti));
        utf8_validate(trackinfo_get_artist(ti));

        set_userstatus_for_active_accounts(ti);

	trace("Status set for all accounts");
        }
        
        trackinfo_destroy(ti);

	return TRUE;
}

//--------------------------------------------------------------------

PurpleCmdRet musictracker_cmd_nowplaying(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
  if (mostrecent_ti && trackinfo_get_status(mostrecent_ti) == STATUS_NORMAL)
    {
      char *status = generate_status(purple_prefs_get_string(PREF_FORMAT), mostrecent_ti, "");
      PurpleConversationType type = purple_conversation_get_type (conv);
  
      if (type == PURPLE_CONV_TYPE_CHAT)
        {
          PurpleConvChat *chat = purple_conversation_get_chat_data(conv);
          if ((chat != NULL) && (status != NULL))
            purple_conv_chat_send (chat, status);
        }
      else if (type == PURPLE_CONV_TYPE_IM)
        {
          PurpleConvIm *im = purple_conversation_get_im_data(conv);
          if ((im != NULL) && (status != NULL))
            purple_conv_im_send (im, status);
        }
      
      if (status != NULL)
        g_free(status);
    }
  // do nothing if nothing is playing?

  return PURPLE_CMD_RET_OK;
}

//--------------------------------------------------------------------

static gboolean
plugin_load(PurplePlugin *plugin) {
	trace("Plugin loaded.");
	g_tid = purple_timeout_add(INTERVAL, &cb_timeout, 0);
	g_plugin = plugin;

	// custom status format for each account
	GList *accounts = purple_accounts_get_all();
	while (accounts) {
		PurpleAccount *account = (PurpleAccount*) accounts->data;
		char buf[100];
		build_pref(buf, PREF_CUSTOM_FORMAT, 
					purple_account_get_username(account),
					purple_account_get_protocol_name(account));
		if (!purple_prefs_exists(buf)) {
			purple_prefs_add_string(buf, "");
		}
		build_pref(buf, PREF_CUSTOM_DISABLED, 
					purple_account_get_username(account),
					purple_account_get_protocol_name(account));
		if (!purple_prefs_exists(buf)) {
			purple_prefs_add_bool(buf, FALSE);
		}
		accounts = accounts->next;
	}

        // register the 'nowplaying' commmand
        cmdid_nowplaying = purple_cmd_register("nowplaying",
                            "", 
                            PURPLE_CMD_P_DEFAULT,
                            PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                            NULL, 
                            musictracker_cmd_nowplaying,
                            "nowplaying:  Display now playing",
                            NULL);

        cmdid_np = purple_cmd_register("np",
                            "", 
                            PURPLE_CMD_P_DEFAULT,
                            PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                            NULL, 
                            musictracker_cmd_nowplaying,
                            "np:  Display now playing",
                            NULL);

	g_run = 1;
    return TRUE;
}

//--------------------------------------------------------------------

static gboolean
plugin_unload(PurplePlugin *plugin) {
	trace("Plugin unloaded.");
        set_userstatus_for_active_accounts(0);
	g_run = 0;
        purple_cmd_unregister(cmdid_nowplaying);
        purple_cmd_unregister(cmdid_np);
	purple_timeout_remove(g_tid);
	return TRUE;
}

//--------------------------------------------------------------------

static PidginPluginUiInfo ui_info = {
	pref_frame,
	0,
        0,0,0,0
};

static PurplePluginInfo info = {
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_STANDARD,
    PIDGIN_PLUGIN_TYPE,
    0,
    NULL,
    PURPLE_PRIORITY_DEFAULT,

    PLUGIN_ID,
    "MusicTracker",
    VERSION,

    "MusicTracker Plugin for Pidgin",
    "The MusicTracker Plugin allows you to customize your status message with information about currently playing song from your music player. Portions initially adopted from pidgin-currenttrack project."
#ifdef WIN32
    "WMP support via WMPuICE by Christian Mueller from http://www.mediatexx.com."
#endif
    ,
    "Jon TURNEY <jon.turney@dronecode.org.uk>",
    "http://code.google.com/p/pidgin-musictracker",

    plugin_load,
    plugin_unload,
    NULL,

    &ui_info,
    NULL,
    NULL,
    actions_list,
    NULL,
    NULL,
    NULL,
    NULL
};

//--------------------------------------------------------------------

static void
init_plugin(PurplePlugin *plugin) {
	purple_prefs_add_none("/plugins/core/musictracker");
	purple_prefs_add_string(PREF_FORMAT, "%r: %t by %p on %a (%d)");
	purple_prefs_add_string(PREF_OFF, "");
	purple_prefs_add_string(PREF_PAUSED, "%r: Paused");
	purple_prefs_add_int(PREF_PAUSED, 0);
	purple_prefs_add_int(PREF_PLAYER, -1);
	purple_prefs_add_bool(PREF_DISABLED, FALSE);
	purple_prefs_add_bool(PREF_LOG, FALSE);
	purple_prefs_add_bool(PREF_FILTER_ENABLE, FALSE);
	purple_prefs_add_string(PREF_FILTER,
			filter_get_default());
	purple_prefs_add_string(PREF_MASK, "*");
	purple_prefs_add_bool(PREF_DISABLE_WHEN_AWAY, FALSE);
	purple_prefs_add_bool(PREF_NOW_LISTENING_ONLY, FALSE);

	// Player specific defaults
	purple_prefs_add_string(PREF_XMMS_SEP, "|");
	purple_prefs_add_string(PREF_MPD_HOSTNAME, "localhost");
	purple_prefs_add_string(PREF_MPD_PASSWORD, "");
	purple_prefs_add_string(PREF_MPD_PORT, "6600");
	purple_prefs_add_string(PREF_LASTFM, "");
	purple_prefs_add_int(PREF_LASTFM_INTERVAL, 120);
	purple_prefs_add_int(PREF_LASTFM_QUIET, 600);
 	purple_prefs_add_string(PREF_XMMS2_PATH, "");
	purple_prefs_add_string(PREF_SQUEEZECENTER_SERVER, "localhost:9090");
	purple_prefs_add_string(PREF_SQUEEZECENTER_USER, "");
	purple_prefs_add_string(PREF_SQUEEZECENTER_PASSWORD, "");
	purple_prefs_add_string(PREF_SQUEEZECENTER_PLAYERS, "");
}

//--------------------------------------------------------------------

PURPLE_INIT_PLUGIN(musictracker, init_plugin, info);