#include <string.h>
#include "musictracker.h"
#include "utils.h"

gboolean
dcop_query(const char* command, GString *dest)
{
	FILE* pipe = popen(command, "r");
	if (!pipe) {
		trace("Failed to open pipe");
		return FALSE;
	}

        char temp[STRLEN];
	if (!readline(pipe, temp, STRLEN)) {
                g_string_empty(dest);
	}
        else
          {
            g_string_assign(dest, temp);
          }

	pclose(pipe);

        trace("dcop_query: '%s' => '%s'", command, dest->str);
	return TRUE;
}

gboolean
get_amarok_info(TrackInfo* ti)
{
        GString *temp = g_string_new("");

        if (!dcop_query("dcopserver --serverid 2>&1", temp) || ((temp->len) == 0))
        {
          trace("Failed to find dcopserver. Assuming off state.");
          g_string_free(temp, TRUE);
          return FALSE;
        }

        trace ("dcopserverid query returned status '%s'", temp->str);

	if (!dcop_query("dcop amarok default status 2>/dev/null", temp)) {
		trace("Failed to run dcop. Assuming off state.");
                g_string_free(temp, TRUE);
		return TRUE;
	}

        trace ("dcop returned status '%s'", temp->str);

        int s = STATUS_OFF;
	sscanf(temp->str, "%d", &s);
        trackinfo_set_status(ti, s);

	if (s != STATUS_OFF) {
                int secs;
		trace("Got valid dcop status... retrieving song info");
		dcop_query("dcop amarok default trackTotalTime", temp);
		sscanf(temp->str, "%d", &secs);
                trackinfo_set_totalSecs(ti, secs);
		dcop_query("dcop amarok default trackCurrentTime", temp);
		sscanf(temp->str, "%d", &secs);
                trackinfo_set_currentSecs(ti, secs);

                const char *methodList[] = 
                  {
                    "artist", "album", "title",
                    "sampleRate", "score", "rating", "trackPlayCounter", "labels", "bitrate", "comment",
                    "encodedURL", "engine", "genre", "lyrics", "path", "track", "type", "year",
                    0
                  };

                for (int i = 0; methodList[i] != 0; i++)
                  {
                    char command[STRLEN];
                    sprintf(command, "dcop amarok default %s", methodList[i]);
                    dcop_query(command, trackinfo_get_gstring_tag(ti, methodList[i]));
                  }
	}

        g_string_free(temp, TRUE);
	return TRUE;
}
