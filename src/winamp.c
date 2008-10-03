#include "musictracker.h"
#include "utils.h"
#include <windows.h>
#include "wa_ipc.h"

static HWND hWnd;
static HANDLE hProcess;

gboolean winamp_get_w(const wchar_t *filename, const char *key, GString *result)
{
        // convert key to wchar_t
        wchar_t wkey[STRLEN];
        MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, STRLEN-1);
        wkey[STRLEN-1] = 0;

	// Allocate memory inside Winamp's address space to exchange data with it
	char *winamp_info = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT, PAGE_READWRITE);
	wchar_t *winamp_filename = (wchar_t*)(winamp_info+1024);
	wchar_t *winamp_key = (wchar_t*)(winamp_info+2048);
	wchar_t *winamp_value = (wchar_t*)(winamp_info+3072);

        // Setup structure with pointers into Winamp address space and store it into that space too
        extendedFileInfoStructW info;
        info.filename = winamp_filename;
        info.metadata = winamp_key;
        info.ret = winamp_value;	
        info.retlen = 1024;
        WriteProcessMemory(hProcess, winamp_info, &info, sizeof(info), NULL);

        WriteProcessMemory(hProcess, winamp_filename, filename, sizeof(wchar_t)*(wcslen(filename)+1), NULL);
	WriteProcessMemory(hProcess, winamp_key, wkey, sizeof(wchar_t)*(wcslen(wkey)+1), NULL);
	int rc = SendMessage(hWnd, WM_WA_IPC, (WPARAM)winamp_info, IPC_GET_EXTENDED_FILE_INFOW);

	SIZE_T bytesRead;
        char dest[STRLEN];
        wchar_t wdest[STRLEN];
	ReadProcessMemory(hProcess, winamp_value, wdest, STRLEN-1, &bytesRead);
	wdest[bytesRead] = 0;

        WideCharToMultiByte(CP_UTF8, 0, wdest, -1, dest, STRLEN, NULL, NULL);

        g_string_assign(result, dest);
        trace("Got info for key '%s' is '%s', return value %d", key, dest, rc);

        return (rc != 1);
}

gboolean winamp_get(const char *filename, const char *key, GString *result)
{
        // Allocate memory inside Winamp's address space to exchange data with it
        char *winamp_info = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT, PAGE_READWRITE);
        char *winamp_filename = (char*)(winamp_info+1024);
        char *winamp_key = (char*)(winamp_info+2048);
        char *winamp_value = (char*)(winamp_info+3072);

        // Setup structure with pointers into Winamp address space and store it into that space too
        extendedFileInfoStruct info;
        info.filename = winamp_filename;
        info.metadata = winamp_key;
        info.ret = winamp_value;        
        info.retlen = 1024;
        WriteProcessMemory(hProcess, winamp_info, &info, sizeof(info), NULL);

        WriteProcessMemory(hProcess, winamp_filename, filename, strlen(filename)+1, NULL);
        WriteProcessMemory(hProcess, winamp_key, key, strlen(key)+1, NULL);
        int rc = SendMessage(hWnd, WM_WA_IPC, (WPARAM)winamp_info, IPC_GET_EXTENDED_FILE_INFO);

        SIZE_T bytesRead;
        char dest[STRLEN];
        ReadProcessMemory(hProcess, winamp_value, dest, STRLEN-1, &bytesRead);
        dest[bytesRead] = 0;

        g_string_assign(result, dest);        
        trace("Got info for key '%s' is '%s', return value %d", key, dest, rc);

        return (rc != 1);
}

gboolean get_winamp_info(TrackInfo* ti)
{
	hWnd = FindWindow("Winamp v1.x", NULL);
	if (!hWnd) {
		trace("Failed to find winamp window. Assuming player is off");
		ti->status = STATUS_OFF;
		return TRUE;
	}
	int version = SendMessage(hWnd, WM_WA_IPC, 0, IPC_GETVERSION);
	trace("Winamp version %d", version);

	DWORD processId;
	GetWindowThreadProcessId(hWnd, &processId);
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, processId);

	int playing = SendMessage(hWnd, WM_WA_IPC, 1, IPC_ISPLAYING);
	if (playing == 0)
		ti->status = STATUS_OFF;
	else if (playing == 3)
		ti->status = STATUS_PAUSED;
	else
		ti->status = STATUS_NORMAL;

	ti->totalSecs = SendMessage(hWnd, WM_WA_IPC, 1, IPC_GETOUTPUTTIME);
	ti->currentSecs = SendMessage(hWnd, WM_WA_IPC, 0, IPC_GETOUTPUTTIME)/1000;

	int position = SendMessage(hWnd, WM_WA_IPC, 0, IPC_GETLISTPOS);

        // the metadata list for the track doesn't seem to be iterable, so we merely 
        // attempt to fetch a known list of possible metadata values
        const char *metadataList[] =
          {
            "title", "artist", "albumartist", "album", "genre", "year", "disc", "publisher", "comment", "track", "composer", "conductor",
            "bitrate", "streamtitle", 0
          };

        // first try wchar interface
	LPCVOID address = (LPCVOID) SendMessage(hWnd, WM_WA_IPC, position, IPC_GETPLAYLISTFILEW);
        if ((unsigned int)address > 1)
          {
            wchar_t wfilename[512];
            ReadProcessMemory(hProcess, address, wfilename, 512, 0);
            char *f = wchar_to_utf8(wfilename);
            trace("Filename(widechar): %s", f);
            free(f);

            for (int i = 0; metadataList[i] != 0; i++)
              {
                winamp_get_w(wfilename, metadataList[i], trackinfo_get_gstring_tag(ti, metadataList[i]));
              }
          }
        else
          {
            // if that fails, fall back to char interface
            // (this is not preferred as it cannot support east asian characters)
            address = (LPCVOID) SendMessage(hWnd, WM_WA_IPC, position, IPC_GETPLAYLISTFILE);
            if ((unsigned int)address > 1)
              {
                char filename[512];
                ReadProcessMemory(hProcess, address, filename, 512, 0);
                trace("Filename: %s", filename);
                
                for (int i = 0; metadataList[i] != 0; i++)
                  {
                    winamp_get(filename, metadataList[i], trackinfo_get_gstring_tag(ti, metadataList[i]));
                  }
              }
          }

        // if these are all empty, which seems to happen when listening to a stream, try something cruder
        // XXX: really should try to work out how to get winamp to resolve it's tag %streamtitle% for us...
        if (((trackinfo_get_gstring_album(ti)->len) == 0) && ((trackinfo_get_gstring_artist(ti)->len) == 0) && ((trackinfo_get_gstring_track(ti)->len) == 0))
          {
            char *title = GetWindowTitleUtf8(hWnd);
            pcre *re;
            re = regex("^\\d*\\. (.*?)(?: - Winamp|)$", 0);
            capture_gstring(re, title, strlen(title), trackinfo_get_gstring_track(ti));
            pcre_free(re);
            free(title);
          }

	return TRUE;
}
