/*
 * musictracker
 * msn-compat.c
 * retrieve track info being sent to MSN
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

#include <windows.h>
#include <musictracker.h>
#include <utils.h>

// put up a hidden window called "MsnMsgrUIManager", which things know to send a WM_COPYDATA message to, 
// containing strangely formatted string, for setting the now-playing status of MSN Messenger
// this might be broken by the sending side only sending to the first window of the class "MsnMsgrUIManager"
// it finds if there are more than one (for e.g., if the real MSN Messenger is running as well :D)

static TrackInfo *msnti;

static
void process_message(wchar_t *MSNTitle)
{
  char *s = wchar_to_utf8(MSNTitle);
  static char player[STRLEN];
  char enabled[STRLEN], title[STRLEN], artist[STRLEN], album[STRLEN], uuid[STRLEN];
  
  // this has to be escaped quite carefully to prevent literals being interpreted as metacharacters by the compiler or in the pcre pattern
  // so yes, four \ before a 0 is required to match a literal \0 in the regex :-)
  pcre *re1 = regex("^(.*)\\\\0Music\\\\0(.*)\\\\0\\{0\\} - \\{1\\}\\\\0(.*)\\\\0(.*)\\\\0(.*)\\\\0(.*)\\\\0$", 0);
  pcre *re2 = regex("^(.*)\\\\0Music\\\\0(.*)\\\\0(.*) - (.*)\\\\0$", 0);
  if (capture(re1, s, strlen(s), player, enabled, artist, title, album, uuid) > 0)
    {
      if (strlen(uuid) > 0)
        {
          // swapped artist and title, muppets
          char temp[STRLEN];
          strcpy(temp, artist);
          strcpy(artist, title);
          strcpy(title, temp);
        }

      trace("player '%s', enabled '%s', artist '%s', title '%s', album '%s', uuid '%s'", player, enabled, artist, title, album, uuid);

      if (strcmp(enabled, "1") == 0)
        {
          trackinfo_set_player(msnti,player);
          trackinfo_set_status(msnti, STATUS_NORMAL);
          g_string_assign(trackinfo_get_gstring_artist(msnti), artist);
          g_string_assign(trackinfo_get_gstring_album(msnti), album);
          g_string_assign(trackinfo_get_gstring_track(msnti), title);
        }
      else
        {
          trackinfo_set_status(msnti, STATUS_OFF);
        }
    }
  else if (capture(re2, s, strlen(s), player, enabled, artist, title) > 0)
    {
      trace("player '%s', enabled '%s', artist '%s', title '%s'", player, enabled, artist, title);

      if (strcmp(enabled, "1") == 0)
        {
          trackinfo_set_player(msnti, player);
          trackinfo_set_status(msnti, STATUS_NORMAL);
          g_string_assign(trackinfo_get_gstring_artist(msnti),artist);
          g_string_empty(trackinfo_get_gstring_album(msnti));
          g_string_assign(trackinfo_get_gstring_track(msnti),title);
        }
      else
        {
          trackinfo_set_status(msnti, STATUS_OFF);
        }
    }
  else
    {
      trackinfo_set_status(msnti, STATUS_OFF);
    }

  pcre_free(re1);
  pcre_free(re2);

  if (strlen(trackinfo_get_player(msnti)))
    {
      trackinfo_set_player(msnti, "Unknown");
    }

  free(s);
}

static
LRESULT CALLBACK MSNWinProc(
                            HWND hwnd,      // handle to window
                            UINT uMsg,      // message identifier
                            WPARAM wParam,  // first message parameter
                            LPARAM lParam   // second message parameter
                            )
{
  switch(uMsg) {
  case WM_COPYDATA:
    {
      wchar_t MSNTitle[2048];
      COPYDATASTRUCT *cds = (COPYDATASTRUCT *) lParam;
      CopyMemory(MSNTitle,cds->lpData,cds->cbData);
      MSNTitle[2047]=0;
      process_message(MSNTitle);
      return TRUE;
    }
  default:
    return DefWindowProc(hwnd,uMsg,wParam,lParam);
  }
}

gboolean get_msn_compat_info(TrackInfo *ti)
{
  static HWND MSNWindow = 0;
  
  if (!MSNWindow)
    {
      msnti = trackinfo_new();

      WNDCLASSEX MSNClass = {sizeof(WNDCLASSEX),0,MSNWinProc,0,0,GetModuleHandle(NULL),NULL,NULL,NULL,NULL,"MsnMsgrUIManager",NULL};
      ATOM a = RegisterClassEx(&MSNClass);
      trace("RegisterClassEx returned 0x%x",a);
  
      MSNWindow = CreateWindowEx(0,"MsnMsgrUIManager","MSN message compatibility window for pidgin-musictracker",
                                      0,0,0,0,0,
                                      HWND_MESSAGE,NULL,GetModuleHandle(NULL),NULL);
      trace("CreateWindowEx returned 0x%x", MSNWindow);

      // Alternatively could use setWindowLong() to override WndProc for this window ?
    }

  // did we receive a message with something useful in it?
  if (trackinfo_get_status(msnti)== STATUS_NORMAL)
    {
      trackinfo_assign(ti, msnti);
      return TRUE;
    }

  return FALSE;
}

// XXX: cleanup needed on musictracker unload?
// XXX: we've also heard tell that HKEY_CURRENT_USER\Software\Microsoft\MediaPlayer\CurrentMetadata has been used to pass this data....
