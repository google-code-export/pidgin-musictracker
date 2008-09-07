/*
 * musictracker
 * trackinfo.h
 * interface to track info storage data type
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

#ifndef trackinfo_h
#define trackinfo_h

typedef struct TrackInfo_t
{
  int status;
  GHashTable *tags;
  const char* player;
  int totalSecs;
  int currentSecs;
} TrackInfo;

TrackInfo *trackinfo_new(void);
TrackInfo *trackinfo_copy(const TrackInfo *ti);
void trackinfo_destroy(TrackInfo *ti);
void trackinfo_assign(TrackInfo *lhs, const TrackInfo *rhs);

#define trackinfo_get_track(ti) trackinfo_get_gstring_tag(ti, "track")->str
#define trackinfo_get_artist(ti) trackinfo_get_gstring_tag(ti, "artist")->str
#define trackinfo_get_album(ti) trackinfo_get_gstring_tag(ti, "album")->str

#define trackinfo_get_gstring_track(ti) trackinfo_get_gstring_tag(ti, "track")
#define trackinfo_get_gstring_artist(ti) trackinfo_get_gstring_tag(ti, "artist")
#define trackinfo_get_gstring_album(ti) trackinfo_get_gstring_tag(ti, "album")

GString *trackinfo_get_gstring_tag(const TrackInfo *ti, const char *tag);

static inline const char *trackinfo_get_player(const TrackInfo* ti) { return ti->player; }
static inline int trackinfo_get_totalSecs(const TrackInfo* ti) { return ti->totalSecs; }
static inline int trackinfo_get_currentSecs(const TrackInfo* ti) { return ti->currentSecs; }
static inline int trackinfo_get_status(const TrackInfo* ti) { return ti->status; }

static inline void trackinfo_set_player(TrackInfo* ti, const char *player) { ti->player = player; }
static inline void trackinfo_set_totalSecs(TrackInfo* ti, int secs) { ti->totalSecs = secs; }
static inline void trackinfo_set_currentSecs(TrackInfo* ti, int secs) { ti->currentSecs = secs; }
static inline void trackinfo_set_status(TrackInfo* ti, int status) { ti->status = status; }

gboolean trackinfo_changed(const TrackInfo* one, const TrackInfo* two);

#define g_string_empty(s) g_string_assign(s, "")

#endif
