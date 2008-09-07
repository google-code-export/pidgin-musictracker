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
  trace("for hash 0x%x duplicating key '%s', value '%s'", dest, tag, string->str);
  g_hash_table_insert(dest, g_strdup(tag), g_string_new(string->str));
}

//--------------------------------------------------------------------

TrackInfo *trackinfo_copy(const TrackInfo *ti)
{
  TrackInfo *copy_ti = trackinfo_new();
  copy_ti->status = ti->status;
  copy_ti->player = ti->player;
  copy_ti->totalSecs = ti->totalSecs;
  copy_ti->currentSecs = ti->currentSecs;

  // and copy tags
  g_hash_table_foreach(ti->tags, tag_copy_helper, copy_ti->tags);

#if 0
  GHashTableIter iter;
  const char *tag;
  GString *value;
  g_hash_table_iter_init(&iter, ti->tags);
  while (g_hash_table_iter_next(&iter, (gpointer *)&tag, (gpointer *)&value))
  {
    g_hash_table_insert(copy_ti->tags, g_strdup(tag), g_string_new(value->str));
  }
#endif

  return copy_ti;
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

  if (g_hash_table_lookup_extended(ti->tags, tag, (gpointer) &found_key, (gpointer) &found_value))
    {
      return found_value;
    }
  else
    {
      g_hash_table_insert(ti->tags, g_strdup(tag), g_string_new(""));
      return g_hash_table_lookup(ti->tags, tag);
    }
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
