extern "C" {
#include "musictracker.h"
#include "utils.h"
#include <windows.h>
}
#include "iTunesCOMInterface.h"

#define BSTR_GET(bstr, method, result) \
	res = track->method(&bstr); \
	if ((res == S_OK) && (bstr != NULL)) { \
		char dest[STRLEN]; \
		WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) bstr, -1, dest, STRLEN, NULL, NULL); \
		dest[STRLEN-1] = 0; \
		g_string_assign(result, dest); \
 		SysFreeString(bstr); \
                trace("method '%s' returned '%s'", #method, result->str); \
	}

#define LONG_GET(bstr, method, result) \
	res = track->method(&l); \
	if (res == S_OK) { \
                g_string_printf(result, "%ld", l); \
                trace("method '%s' returned %ld", #method, l); \
	}

extern "C" gboolean get_itunes_info(TrackInfo *ti)
{
	if (!FindWindow("iTunes", NULL)) {
		ti->status = STATUS_OFF;
		return TRUE;
	}

        IiTunes *itunes;
	if (CoCreateInstance(CLSID_iTunesApp, NULL, CLSCTX_LOCAL_SERVER, IID_IiTunes, (PVOID *) &itunes) != S_OK) {
		trace("Failed to get iTunes COM interface");
		return FALSE;
	}

	ITPlayerState state;
	if (itunes->get_PlayerState(&state) != S_OK)
		return FALSE;
	if (state == ITPlayerStatePlaying)
		ti->status = STATUS_NORMAL;
	else if (state == ITPlayerStateStopped)
		ti->status = STATUS_PAUSED;

	IITTrack *track;
	HRESULT res = itunes->get_CurrentTrack(&track);
	if (res == S_FALSE) {
		ti->status = STATUS_OFF;
		return TRUE;
	} else if (res != S_OK)
		return FALSE;

        // the attribute list for an ITTrack doesn't seem to be iterable, so we merely attempt to fetch a known list of possible attributes
        BSTR bstr = 0;
	BSTR_GET(bstr, get_Artist, trackinfo_get_gstring_artist(ti));
	BSTR_GET(bstr, get_Album, trackinfo_get_gstring_album(ti));
	BSTR_GET(bstr, get_Name, trackinfo_get_gstring_track(ti));
	BSTR_GET(bstr, get_Comment, trackinfo_get_gstring_tag(ti, "comment"));
	BSTR_GET(bstr, get_Composer, trackinfo_get_gstring_tag(ti, "composer"));
	BSTR_GET(bstr, get_Genre, trackinfo_get_gstring_tag(ti, "genre"));
	BSTR_GET(bstr, get_Grouping, trackinfo_get_gstring_tag(ti, "grouping"));
	BSTR_GET(bstr, get_KindAsString, trackinfo_get_gstring_tag(ti, "kindasstring"));

        long l = 0;
	LONG_GET(l, get_BitRate, trackinfo_get_gstring_tag(ti, "bitrate"));
	LONG_GET(l, get_BPM, trackinfo_get_gstring_tag(ti, "bpm"));
	LONG_GET(l, get_DiscCount, trackinfo_get_gstring_tag(ti, "disccount"));
	LONG_GET(l, get_DiscNumber, trackinfo_get_gstring_tag(ti, "discnumber"));
	LONG_GET(l, get_PlayedCount, trackinfo_get_gstring_tag(ti, "playedcount"));
	LONG_GET(l, get_Rating, trackinfo_get_gstring_tag(ti, "rating"));
	LONG_GET(l, get_SampleRate, trackinfo_get_gstring_tag(ti, "samplerate"));
	LONG_GET(l, get_TrackCount, trackinfo_get_gstring_tag(ti, "trackcount"));
	LONG_GET(l, get_TrackNumber, trackinfo_get_gstring_tag(ti, "tracknumber"));
	LONG_GET(l, get_VolumeAdjustment, trackinfo_get_gstring_tag(ti, "volumeadjustment"));
	LONG_GET(l, get_Year, trackinfo_get_gstring_tag(ti, "year"));

        long duration = 0;
        track->get_Duration(&duration);
        trackinfo_set_totalSecs(ti, duration);

	long position = 0;
	itunes->get_PlayerPosition(&position);
        trackinfo_set_currentSecs(ti, position);

	return TRUE;
}

