extern "C" {
#include "musictracker.h"
#include "utils.h"
#include <windows.h>
}
#include "iTunesCOMInterface.h"

#define BSTR_GET(bstr, result) \
	if (bstr != NULL) { \
		char dest[STRLEN]; \
		WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) bstr, -1, dest, STRLEN, NULL, NULL); \
		dest[STRLEN-1] = 0; \
		g_string_assign(result, dest); \
 		SysFreeString(bstr); \
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

	BSTR bstr;
	track->get_Artist(&bstr);
	BSTR_GET(bstr, trackinfo_get_gstring_artist(ti));
	track->get_Album(&bstr);
	BSTR_GET(bstr, trackinfo_get_gstring_album(ti));
	track->get_Name(&bstr);
	BSTR_GET(bstr, trackinfo_get_gstring_track(ti));

        long duration = 0;
        track->get_Duration(&duration);
        trackinfo_set_totalSecs(ti, duration);

	long position = 0;
	itunes->get_PlayerPosition(&position);
        trackinfo_set_currentSecs(ti, position);

	return TRUE;
}

