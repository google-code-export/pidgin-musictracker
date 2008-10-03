/*
 * musictracker
 * trackinfo.c
 * imnplementatin of track info storage data type
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

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "trackinfo.h"
#include "utils.h"

// trackinfo contains a hash to hold tags retrieved from the player (if any),
// key is a char * (always converted to lower-case), value is a GString

//--------------------------------------------------------------------

static
void g_string_free_adaptor(gpointer s)
{
  g_string_free((GString *)s, TRUE);
}

//--------------------------------------------------------------------

TrackInfo *trackinfo_new(void)
{
  TrackInfo *ti = malloc(sizeof(TrackInfo));
  if (ti)
    {
      memset(ti, 0, sizeof(TrackInfo));
      ti->tags = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_string_free_adaptor);
    }

  return ti;
}

//--------------------------------------------------------------------

void trackinfo_destroy(TrackInfo *ti)
{
  g_hash_table_destroy(ti->tags);
  free(ti);
}

//--------------------------------------------------------------------

static void tag_copy_helper(gpointer key, gpointer value, gpointer user_data)
{
  GHashTable *dest = (GHashTable *)user_data;
  const char *tag = (const char *)key;
  GString *string = (GString *)value;
  // trace("for hash 0x%x duplicating key '%s', value '%s'", dest, tag, string->str);
  g_hash_table_insert(dest, g_strdup(tag), g_string_new(string->str));
}

//--------------------------------------------------------------------

void trackinfo_assign(TrackInfo *lhs, const TrackInfo *rhs)
{
  lhs->status = rhs->status;
  lhs->player = rhs->player;
  lhs->totalSecs = rhs->totalSecs;
  lhs->currentSecs = rhs->currentSecs;

  // and copy tags
  g_hash_table_foreach(rhs->tags, tag_copy_helper, lhs->tags);
}

//--------------------------------------------------------------------

GString *trackinfo_get_gstring_tag(const TrackInfo *ti, const char *tag)
{
  char *found_key;
  GString *found_value;

  // force tag to lower-case
  gchar *tag_lower = g_ascii_strdown(tag, -1);
  // XXX: also remove punctuation etc. to help tag normalization?

  if (!g_hash_table_lookup_extended(ti->tags, tag_lower, (gpointer) &found_key, (gpointer) &found_value))
    {
      // not found, insert new key
      g_hash_table_insert(ti->tags, g_strdup(tag_lower), g_string_new(""));
      found_value =  g_hash_table_lookup(ti->tags, tag_lower);
    }

  g_free(tag_lower);  

  return found_value;
}

//--------------------------------------------------------------------

gboolean trackinfo_changed(const TrackInfo* one, const TrackInfo* two)
{
  if ((one == NULL) && (two == NULL))
    return FALSE;

  if ((one == NULL) || (two == NULL))
    return TRUE;

  if ((one->status) != (two->status))
    return TRUE;

  if ((one->totalSecs) != (two->totalSecs))
    return TRUE;
  
  if (strcmp(trackinfo_get_track(one), trackinfo_get_track(two)) != 0)
    return TRUE;
  
  if (strcmp(trackinfo_get_artist(one), trackinfo_get_artist(two)) != 0)
    return TRUE;
  
  if (strcmp(trackinfo_get_album(one), trackinfo_get_album(two)) != 0)
    return TRUE;
  
  return FALSE;
}

//--------------------------------------------------------------------
