; The name of the installer
Name "MusicTracker Plugin for Pidgin"

; The file to write
OutFile "pidgin-musictracker-${VERSION}.exe"

; We want to write the plugin to Program Files, so request privileges
; XXX: could we install the plugin to %APPDATA%/.purple/plugin instead if we don't have ?
RequestExecutionLevel admin

!define PIDGIN_REG_KEY                          "SOFTWARE\pidgin"

;--------------------------------

; Pages

Page directory
Page instfiles

;--------------------------------

; The stuff to install
Section "" ;No components page, name is not important

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put files there
  File /oname=plugins\musictracker.dll ..\src\musictracker.dll
  File /oname=wmpuice.dll wmpuice.dll

  !include po_list.nsi

  RegDLL wmpuice.dll  

SectionEnd ; end the section

;--------------------------------

; based on the .onInit function from pidgin's .nsi script
Function .onInit

; If install path was set on the command, use it.
StrCmp $INSTDIR "" 0 instdir_done

; If pidgin is currently installed, we should default to where it is currently installed
ClearErrors
ReadRegStr $INSTDIR HKCU "${PIDGIN_REG_KEY}" ""
IfErrors +2
StrCmp $INSTDIR "" 0 instdir_done
ClearErrors
ReadRegStr $INSTDIR HKLM "${PIDGIN_REG_KEY}" ""
IfErrors +2
StrCmp $INSTDIR "" 0 instdir_done

; The default installation directory
StrCpy $INSTDIR "$PROGRAMFILES\Pidgin"

  instdir_done:
FunctionEnd
