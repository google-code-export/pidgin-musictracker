#include "musictracker.h"
#include "utils.h"
#include <windows.h>

void
get_foobar2000_info(struct TrackInfo* ti)
{
	ti->status = PLAYER_STATUS_CLOSED;

        // very brittle way of finding the foobar2000 window...
	HWND mainWindow = FindWindow("{DA7CD0DE-1602-45e6-89A1-C2CA151E008E}/1", NULL); // Foobar 0.9.1
	if (!mainWindow)
		mainWindow = FindWindow("{DA7CD0DE-1602-45e6-89A1-C2CA151E008E}", NULL);
	if (!mainWindow)
		mainWindow = FindWindow("{97E27FAA-C0B3-4b8e-A693-ED7881E99FC1}", NULL); // Foobar 0.9.5.3 
	if (!mainWindow)
		mainWindow = FindWindow("{E7076D1C-A7BF-4f39-B771-BCBE88F2A2A8}", NULL); // Foobar Columns UI
	if (!mainWindow) {
		trace("Failed to find foobar2000 window. Assuming player is closed");
		return;
	}

        char *title = GetWindowTitleUtf8(mainWindow);

	if (strncmp(title, "foobar2000", 10) == 0) {
		ti->status = PLAYER_STATUS_STOPPED;
	}
        else
          {
            ti->status = PLAYER_STATUS_PLAYING;
            pcre *re;
            re = regex("(.*) - (?:\\[([^#]+)[^\\]]+\\] |)(.*) \\[foobar2000.*\\]", 0);
            capture(re, title, strlen(title), ti->artist, ti->album, ti->track);
            pcre_free(re);
          }

        free(title);
}
