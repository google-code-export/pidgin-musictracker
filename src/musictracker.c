#ifndef WIN32
#include "config.h"
#else
#include <config-win32.h>
#include <win32dep.h>
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
#include "gettext.h"

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

#define _(String) dgettext (PACKAGE, String)

static guint g_tid;
static gboolean g_run = 0;
static struct TrackInfo mostrecent_ti;
static PurpleCmdId cmdid_nowplaying;
static PurpleCmdId cmdid_np;

//--------------------------------------------------------------------

// Global array of players
struct PlayerInfo g_players[] = {
#ifndef WIN32
	{ "XMMS", get_xmms_info, get_xmmsctrl_pref },
	{ "Audacious < 1.4", get_audacious_legacy_info, get_xmmsctrl_pref },
	{ "Audacious >= 1.4", get_audacious_info, 0 },
	{ "Amarok 1.x.x", get_amarok_info, 0 },
	{ "Rhythmbox", get_rhythmbox_info, 0 },
	{ "Banshee", get_banshee_info, 0 },
	{ "Vagalume", get_vagalume_info, 0 },
	{ "QuodLibet", get_quodlibet_info, 0 },
	{ "Exaile < 0.3", get_exaile_info, 0 },
	{ "MOC", get_moc_info, 0 },
	{ "Listen", get_listen_info, 0 },
	{ "SqueezeCenter", get_squeezecenter_info, get_squeezecenter_pref },
#ifdef HAVE_XMMSCLIENT
 	{ "XMMS2", get_xmms2_info, get_xmms2_pref },
#endif
 	{ "MPRIS", get_mpris_info, 0 },
 	{ "Songbird", get_dbusbird_info, 0 },
#else
	{ "Winamp", get_winamp_info, 0 },
	{ "Windows Media Player", get_wmp_info, 0 },
	{ "iTunes", get_itunes_info, 0 },
	{ "Messenger compatible interface" , get_msn_compat_info, get_msn_compat_pref },
	{ "Foobar2000", get_foobar2000_info, 0 },
#endif
	{ "MPD", get_mpd_info, get_mpd_pref },
 	{ "Last.fm (API 2.0)", get_lastfm_ws_info, get_lastfm_ws_pref },
 	{ "Last.fm (API 1.0)", get_lastfm_info, get_lastfm_pref },
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
purple_status_is_offline_or_invisible(PurpleStatus *status)
{
  PurpleStatusPrimitive primitive = PURPLE_STATUS_UNSET;

  if (!status)
    return FALSE;

  primitive = purple_status_type_get_primitive(purple_status_get_type(status));

  return ((primitive == PURPLE_STATUS_INVISIBLE) || (primitive == PURPLE_STATUS_OFFLINE));
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

static gboolean
trackinfo_changed(const struct TrackInfo* one, const struct TrackInfo* two)
{
  if ((one == NULL) && (two == NULL))
    return FALSE;

  // a null trackinfo is stored in mostrecent_ti as a PLAYER_STATUS_CLOSED one...
  if ((one == NULL) && (two != NULL) && (two->status <= PLAYER_STATUS_CLOSED))
    return FALSE;

  if ((one == NULL) || (two == NULL))
    return TRUE;

  if (one->status != two->status)
    return TRUE;

  if (strcmp(one->track, two->track) != 0)
    return TRUE;
  
  if (strcmp(one->artist, two->artist) != 0)
    return TRUE;
  
  if (strcmp(one->album, two->album) != 0)
    return TRUE;

  // comparing totalSecs is a bit daft since it should be invariant for a given track
  // really we want to compare those members which correspond to attributes accepted by the tune status...
  
  return FALSE;
}

//--------------------------------------------------------------------

static
char* generate_status(const char *src, struct TrackInfo *ti, const char *savedstatus)
{
	trace("Status format: %s", src);

	char *status = malloc(strlen(src)+1);
	strcpy(status, src);
	status = put_field(status, 'p', ti->artist);
	status = put_field(status, 'a', ti->album);
	status = put_field(status, 't', ti->track);
	status = put_field(status, 'r', ti->player);

	// Duration
	char buf[20];
	sprintf(buf, "%d:%02d", ti->totalSecs/60, ti->totalSecs%60);
	status = put_field(status, 'd', buf);

	// Current time
	sprintf(buf, "%d:%02d", ti->currentSecs/60, ti->currentSecs%60);
	status = put_field(status, 'c', buf);

	// Progress bar
	int i, progress;
	if (ti->totalSecs == 0)
		progress = 0;
	else 
		progress = (int)floor(ti->currentSecs * 10.0 / ti->totalSecs);
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

	trace("Formatted status: %s", status);

        return status;
}

//--------------------------------------------------------------------

// Updates 'user tune' status primitive with current track info
static
gboolean
set_status_tune (PurpleAccount *account, gboolean validStatus, struct TrackInfo *ti)
{
	PurpleStatus *status			= NULL;
	PurplePresence* presence = NULL;
	gboolean active = FALSE;

	if (validStatus)
	{
		if (ti == NULL)
			return FALSE;
		active = (ti->status == PLAYER_STATUS_PLAYING);
	}
	else
	{		
		active = FALSE;
	}

	presence = purple_account_get_presence (account);

	status = purple_presence_get_status(presence, purple_primitive_get_id_from_type(PURPLE_STATUS_TUNE));
	if (status == NULL)
	{
		trace("No tune status for account %s, protocol %s, falling back to setting status message", 
                      purple_account_get_username(account), purple_account_get_protocol_name(account));
		return FALSE;
	}

        // libpurple always reports that XMPP supports the PURPLE_STATUS_TUNE status
        // and attempts to set the PURPLE_STATUS_TUNE always succeed, irrespective of
        // if the particular server does or doesn't actually support it
        //
        // this is particularly a problem for googletalk, which really expects tune info
        // to replace the status message, and lots of peopple use...
        //
	char *buf = build_pref(PREF_BROKEN_NOWLISTENING,
                               purple_account_get_username(account),
                               purple_account_get_protocol_name(account));
        gboolean dont_use_tune_status = purple_prefs_get_bool(buf);
        g_free(buf);

	if (dont_use_tune_status)
          {
            trace("Won't try to use status tune on account '%s' protocol '%s', I've been told not to", purple_account_get_username(account), purple_account_get_protocol_name(account));
            return FALSE;
          }

        // don't need to do anything if track info hasn't changed
        if (!trackinfo_changed(ti, &mostrecent_ti))
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
                attrs = g_list_append(attrs, ti->artist);
                attrs = g_list_append(attrs, PURPLE_TUNE_TITLE);
                attrs = g_list_append(attrs, ti->track);
                attrs = g_list_append(attrs, PURPLE_TUNE_ALBUM);
                attrs = g_list_append(attrs, ti->album);
                attrs = g_list_append(attrs, PURPLE_TUNE_TIME);
                attrs = g_list_append(attrs, (gpointer) (intptr_t) ti->totalSecs);
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
                // (See http://developer.pidgin.im/ticket/7081)
                GList *attrs = NULL;
                attrs = g_list_append(attrs, PURPLE_TUNE_ARTIST);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_TITLE);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_ALBUM);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_TIME);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_GENRE);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_COMMENT);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_TRACK);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_YEAR);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_URL);
                attrs = g_list_append(attrs, 0);
                attrs = g_list_append(attrs, PURPLE_TUNE_FULL);
                attrs = g_list_append(attrs, 0);
                purple_status_set_active_with_attrs_list(status, FALSE, attrs);
                g_list_free(attrs);
	}
	
	return TRUE;
}

//--------------------------------------------------------------------

gboolean
set_status (PurpleAccount *account, struct TrackInfo *ti)
{
        // check for status changing disabled for this account
        char *buf = build_pref(PREF_CUSTOM_DISABLED,
			purple_account_get_username(account),
			purple_account_get_protocol_name(account));
        gboolean status_changing_disabled = purple_prefs_get_bool(buf);
        g_free(buf);

	if (status_changing_disabled)
          {
            trace("Status changing disabled for %s account", purple_account_get_username(account));
            return TRUE;
          }

        // 'invisible' or 'offline' statuses always take priority
        // (don't even set the tune status as this might make us to go online...)
	PurpleStatus *status = purple_account_get_active_status(account);
	if (status != NULL)
	{
          if (purple_status_is_offline_or_invisible(status))
            {
              trace("Status is invisible or offline");
              return TRUE;
            }
	}

        // set 'now playing' status
        if (set_status_tune(account, ti != 0, ti) && purple_prefs_get_bool(PREF_NOW_LISTENING_ONLY))
          {
            return TRUE;
          }

        // have we requested 'away' status to take priority?
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
            switch (ti->status)
              {
              case PLAYER_STATUS_CLOSED:
                text = generate_status("", ti, savedmessage);
                break;

              case PLAYER_STATUS_STOPPED:
                text = generate_status(purple_prefs_get_string(PREF_OFF), ti, savedmessage);
                break;

              case PLAYER_STATUS_PAUSED:
                text = generate_status(purple_prefs_get_string(PREF_PAUSED), ti, savedmessage);
                break;

              case PLAYER_STATUS_PLAYING:
                {
                  // check for protocol status format override for this account
                  buf = build_pref(PREF_CUSTOM_FORMAT,
                                   purple_account_get_username(account),
                                   purple_account_get_protocol_name(account));
                  const char *override = purple_prefs_get_string(buf);
                  g_free(buf);

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
                trace("unknown player status %d", ti->status);
              }
          }

        // ensure we have something!
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

static
void
set_userstatus_for_active_accounts (struct TrackInfo *ti)
{
        GList                   *accounts               = NULL,
                                        *head                   = NULL;
        PurpleAccount             *account                = NULL;

	gboolean b = purple_prefs_get_bool(PREF_DISABLED);
	if (b) {
		trace("status changing has been disabled");
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
            
            if (head != NULL)
              g_list_free (head);
            
            trace("Status set for all accounts");
          }
        
        // stash trackinfo in case we need it elsewhere....
        if (ti)
          mostrecent_ti = *ti;
        else
          mostrecent_ti.status = PLAYER_STATUS_CLOSED;
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

static
void
set_track_information(struct TrackInfo *ti)
{
  set_userstatus_for_active_accounts(ti);
}

//--------------------------------------------------------------------

void
restore_track_information(void)
{
  set_track_information(&mostrecent_ti);
}

//--------------------------------------------------------------------

void
clear_track_information(void)
{
  set_userstatus_for_active_accounts(0);
}

//--------------------------------------------------------------------

static gboolean
cb_timeout(gpointer data) {
	if (g_run == 0)
		return FALSE;

	struct TrackInfo ti;

	int player = purple_prefs_get_int(PREF_PLAYER);

	if (player != -1)
          {
            // a specific player is configured
            trace("trying '%s'", g_players[player].name);
            memset(&ti, 0, sizeof(ti));
            ti.status = PLAYER_STATUS_CLOSED;
            ti.player = g_players[player].name;
            (*g_players[player].track_func)(&ti);
          }
        else
          {
            // try to autodetect an active player
            int i = 0;
            while (strlen(g_players[i].name))
              {
                trace("trying '%s'", g_players[i].name);
                memset(&ti, 0, sizeof(ti));
                ti.status = PLAYER_STATUS_CLOSED;
                ti.player = g_players[i].name;
                (*g_players[i].track_func)(&ti);

                // XXX: to maintain historical behaviour we stop looking at the first player which is paused or playing
                // we could try harder and continue to look for a player which is playing if we found one which was paused...
                if (ti.status > PLAYER_STATUS_STOPPED)
                  {
                    break;
                  }

                ++i;
            }
          }

	if (ti.status == PLAYER_STATUS_CLOSED)
          {
		trace("Getting info failed. Setting empty status.");
		set_userstatus_for_active_accounts(0);
		return TRUE;
          }

	trim(ti.album);
	trim(ti.track);
	trim(ti.artist);
	trace("%s,%s,%s,%s,%d", ti.player, ti.artist, ti.album, ti.track, ti.status);

        // ensure track information is valid utf-8
        utf8_validate(ti.album);
        utf8_validate(ti.track);
        utf8_validate(ti.artist);

        // ensure track information is only printable chars
        // (XMPP has strict rules about allowable characters,
        //  printable is a subset of those allowed)
        filter_printable(ti.track);
        filter_printable(ti.artist);
        filter_printable(ti.album);

        // if profanity filter is on, sanitize track info
	if (purple_prefs_get_bool(PREF_FILTER_ENABLE))
          {
            filter_profanity(ti.track);
            filter_profanity(ti.artist);
            filter_profanity(ti.album);
	}

        set_track_information(&ti);

	return TRUE;
}

//--------------------------------------------------------------------

static
PurpleCmdRet musictracker_cmd_nowplaying(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
  if (mostrecent_ti.status == PLAYER_STATUS_PLAYING)
    {
      char *status = generate_status(purple_prefs_get_string(PREF_FORMAT), &mostrecent_ti, "");
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
	trace("Plugin loading.");
	g_tid = purple_timeout_add(INTERVAL, &cb_timeout, 0);

        // make the mostrecent track information something invalid so it will always get updated the first time...
        mostrecent_ti.status = PLAYER_STATUS_INVALID;
	mostrecent_ti.player = "";

	// custom status format for each account
	GList *accounts = purple_accounts_get_all();
	while (accounts) {
		PurpleAccount *account = (PurpleAccount*) accounts->data;
                trace("Checking if we need to set default preferences for %s on protocol %s", purple_account_get_username(account),
                      purple_account_get_protocol_name(account));

		char *buf = build_pref(PREF_CUSTOM_FORMAT,
                                       purple_account_get_username(account),
                                       purple_account_get_protocol_name(account));
		if (!purple_prefs_exists(buf)) {
			purple_prefs_add_string(buf, "");
		}
                g_free(buf);

		buf = build_pref(PREF_CUSTOM_DISABLED,
                                 purple_account_get_username(account),
                                 purple_account_get_protocol_name(account));
		if (!purple_prefs_exists(buf)) {
			purple_prefs_add_bool(buf, FALSE);
		}
                g_free(buf);

		buf = build_pref(PREF_BROKEN_NOWLISTENING,
                                 purple_account_get_username(account),
                                 purple_account_get_protocol_name(account));
		if (!purple_prefs_exists(buf)) {
                        // use a crude guess for the initial state of the broken now listening flag...
                        // there's no good way to do this automatically, hence the reason for offloading
                        // this decision on to the user :-)
                        if (strcmp(purple_account_get_protocol_name(account),"XMPP") == 0)
                          {
                            purple_prefs_add_bool(buf, TRUE);
                          }
                        else
                          {
                            purple_prefs_add_bool(buf, FALSE);
                          }
		}
                g_free(buf);

		accounts = accounts->next;
	}

        // register the 'nowplaying' commmand
	trace("Registering nowplaying command.");
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
	trace("Plugin loaded.");
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

    "gtk-jturney-musictracker",
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
	purple_prefs_add_bool(PREF_MSN_SWAP_ARTIST_TITLE, FALSE);

#ifdef ENABLE_NLS
        // bind translation domain for musictracker to file
        bindtextdomain(PACKAGE, LOCALEDIR);
        // always output in UTF-8 codeset as that is used internally by GTK+
	bind_textdomain_codeset(PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */

        // initialize translated plugin details
        /* 
           TRANSLATORS: this string controls the name musictracker appears as under the Tools menu
           It's probably a good idea to treat 'MusicTracker' as a proper name, transliterate but don't translate
         */
        info.name        = _("MusicTracker");
        info.summary     = _("MusicTracker Plugin for Pidgin");

        /*
           TRANSLATORS: %s will be replaced with the URL of the translation site, currently
           http://translations.launchpad.net/pidgin-musictracker/trunk/+pots/musictracker
         */
        char *translation_bugs = g_strdup_printf(_("Fix translation bugs at %s"),
                                                 "http://translations.launchpad.net/pidgin-musictracker/trunk/+pots/musictracker");

        info.description = g_strdup_printf("%s\n%s\n%s",
                                           _("The MusicTracker Plugin allows you to customize your status message with information about currently playing song from your music player. Portions initially adopted from pidgin-currenttrack project."),
#ifdef WIN32
                                           _("WMP support via WMPuICE by Christian Mueller from http://www.mediatexx.com."),
#else
                                           "",
#endif
                                           translation_bugs);

        g_free(translation_bugs);
}

//--------------------------------------------------------------------

PURPLE_INIT_PLUGIN(musictracker, init_plugin, info);
