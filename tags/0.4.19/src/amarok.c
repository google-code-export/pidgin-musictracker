#include <string.h>
#include "musictracker.h"
#include "utils.h"

gboolean
dcop_query(const char* command, char *dest, int len)
{
	FILE* pipe = popen(command, "r");
	if (!pipe) {
		trace("Failed to open pipe");
		return FALSE;
	}

	if (!readline(pipe, dest, len)) {
		dest[0] = 0;
	}
	pclose(pipe);
        trace("dcop_query: '%s' => '%s'", command, dest);
	return TRUE;
}

void
get_amarok_info(struct TrackInfo* ti)
{
	char status[STRLEN];

        ti->player = "Amarok";
        ti->status = PLAYER_STATUS_CLOSED;

        if (!dcop_query("dcopserver --serverid 2>&1", status, STRLEN) || (strlen(status) == 0))
        {
          trace("Failed to find dcopserver. Assuming closed state.");
          return;
        }

        trace ("dcopserverid query returned status '%s'", status);

	if (!dcop_query("dcop amarok default status 2>/dev/null", status, STRLEN)) {
		trace("Failed to run dcop. Assuming closed state.");
		return;
	}

        trace ("dcop returned status '%s'", status);

        int status_value;
	sscanf(status, "%d", &status_value);
        switch (status_value)
          {
          case 0:
            ti->status = PLAYER_STATUS_STOPPED;
            break;
          case 1:
            ti->status = PLAYER_STATUS_PAUSED;
            break;
          case 2:
            ti->status = PLAYER_STATUS_PLAYING;
            break;
          default:
            ti->status = PLAYER_STATUS_CLOSED;
          }

	if (ti->status > PLAYER_STATUS_STOPPED) {
		char time[STRLEN];
		trace("Got valid dcop status... retrieving song info");
		dcop_query("dcop amarok default artist", ti->artist, STRLEN);
		dcop_query("dcop amarok default album", ti->album, STRLEN);
		dcop_query("dcop amarok default title", ti->track, STRLEN);

		dcop_query("dcop amarok default trackTotalTime", time, STRLEN);
		sscanf(time, "%d", &(ti->totalSecs));
		dcop_query("dcop amarok default trackCurrentTime", time, STRLEN);
		sscanf(time, "%d", &(ti->currentSecs));
	}
}
