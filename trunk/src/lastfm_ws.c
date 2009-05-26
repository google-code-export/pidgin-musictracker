/*
 * musictracker
 * lastfm_ws.c
 * retrieve track info from Last.fm webservices API
 *
 * Copyright (C) 2008, Jon TURNEY <jon.turney@dronecode.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 *
 */

#include "musictracker.h"
#include "utils.h"

#ifndef WIN32
#include <config.h>
#else
#include <config-win32.h>
#endif

#include "gettext.h"
#define _(String) dgettext (PACKAGE, String)

#define INTERVAL_SECONDS (INTERVAL/1000)

#define LASTFM_WS_API_KEY "acf5c54b792ded24e895d98300a0d67a"
#define LASTFM_WS_URL "http://ws.audioscrobbler.com/2.0/?method=user.getrecenttracks&user=%s&api_key=%s&limit=1"
#define USER_AGENT "pidgin-musictracker/" VERSION

//
// See http://www.last.fm/api/intro for documentation on the last.fm API webservices
//

static struct TrackInfo lastfm_ws_ti =
  { "", "", "", "", PLAYER_STATUS_STOPPED, 0 , 0 };

static
void data_from_node(xmlnode *track, const char *name, char *result)
{
  result[0] = 0;
  xmlnode *node = xmlnode_get_child(track, name);
  if (node)
    {
      char *value = xmlnode_get_data(node);
      strncpy(result, value, STRLEN);
      result[STRLEN-1] = 0;
      g_free(value);
    }
}

static
void
lastfm_ws_fetch(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
  trace("Fetched %d bytes of data %s", len, error_message ? error_message : "");
  if (url_text)
    {
      trace(url_text);
      xmlnode *response = xmlnode_from_str(url_text, -1);
      if (response)
        {
          xmlnode *recenttracks = xmlnode_get_child(response, "recenttracks");
          if (recenttracks)
            {
              xmlnode *track = xmlnode_get_child(recenttracks, "track");
              if (track)
                {
                  const char *nowplaying = xmlnode_get_attrib(track, "nowplaying");

                  data_from_node(track, "name", lastfm_ws_ti.track);
                  data_from_node(track, "album", lastfm_ws_ti.album);
                  data_from_node(track, "artist", lastfm_ws_ti.artist);

                  if (nowplaying)
                    {
                      lastfm_ws_ti.status = PLAYER_STATUS_PLAYING;
                    }
                  else
                    {
                      lastfm_ws_ti.status = PLAYER_STATUS_STOPPED;
                    }

                  lastfm_ws_ti.player = "Last.fm";
                }
            }

          xmlnode_free(response);
        }
      else
        {
          trace("Last.fm response was badly formed XML");
        }
    }
}

void
get_lastfm_ws_info(struct TrackInfo* ti)
{
  const char *user = purple_prefs_get_string(PREF_LASTFM);
  if (!strcmp(user,""))
    {
      trace("No last.fm user name");
      return;
    }
  trace("Got user name: %s",user);

  // Check if it's time to check again
  static int count = 0;
  if (count < 0)
    {
      trace("last.fm ratelimit %d",count);
    }
  else
    {
      count = count - purple_prefs_get_int(PREF_LASTFM_INTERVAL);

      char *url = g_strdup_printf(LASTFM_WS_URL, user, LASTFM_WS_API_KEY);
      trace("URL is %s", url);

      purple_util_fetch_url_request_len(url, TRUE, USER_AGENT, FALSE, NULL, FALSE, -1, lastfm_ws_fetch, NULL);

      g_free(url);
    }
  count = count + INTERVAL_SECONDS;

  *ti = lastfm_ws_ti;
}

static
void cb_lastfm_ws_username_changed(GtkWidget *widget, gpointer data)
{
  purple_prefs_set_string((const char*)data, gtk_entry_get_text(GTK_ENTRY(widget)));
}

static
void cb_lastfm_ws_interval_changed(GtkSpinButton *widget, gpointer data)
{
  purple_prefs_set_int((const char*)data, gtk_spin_button_get_value_as_int(widget));
}

void get_lastfm_ws_pref(GtkBox *box)
{
  GtkWidget *widget, *vbox, *hbox, *label;
  GtkAdjustment *interval_spinner_adj = (GtkAdjustment *) gtk_adjustment_new(purple_prefs_get_int(PREF_LASTFM_INTERVAL),
                                                                             INTERVAL_SECONDS, 600.0, INTERVAL_SECONDS, INTERVAL_SECONDS*5.0, INTERVAL_SECONDS*5.0);

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(box), vbox, FALSE, FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Username:")), FALSE, FALSE, 0);
  widget = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
  gtk_entry_set_text(GTK_ENTRY(widget), purple_prefs_get_string(PREF_LASTFM));
  g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(cb_lastfm_ws_username_changed), (gpointer) PREF_LASTFM);

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("poll interval:")), FALSE, FALSE, 0);
  widget = gtk_spin_button_new(interval_spinner_adj, INTERVAL_SECONDS, 0);
  gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(widget), "value-changed", G_CALLBACK(cb_lastfm_ws_interval_changed), (gpointer) PREF_LASTFM_INTERVAL);

  label = gtk_label_new(_("This is the interval (in seconds) at which we check the Last.fm API for changes"));
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
}
