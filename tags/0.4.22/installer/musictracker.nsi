; The name of the installer
Name "MusicTracker Plugin for Pidgin"

; The file to write
OutFile "pidgin-musictracker-${VERSION}.exe"

; Compression level
SetCompressor /SOLID lzma

; We want to write the plugin to Program Files, so request privileges
; XXX: could we install the plugin to %APPDATA%/.purple/plugin instead if we don't have ?
RequestExecutionLevel admin

; manifest for modern-style common controls
XPStyle on

!define PIDGIN_REG_KEY                          "SOFTWARE\pidgin"
!define MUSICTRACKER_UNINSTALL_KEY		"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Pidgin-Musictracker"

# Message to display if not admin rights
!define Message_Not_Admin "You do not have Administrator privileges."

;--------------------------------

; Pages

Page directory
Page instfiles

;--------------------------------

; The stuff to install
Section "Musictracker" ;No components page, name is not important

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  ; ensure plugins directory exists
  ; (just in case someone wants to install musictracker without having already installed pidgin :-))
  CreateDirectory $INSTDIR\plugins

  ; Put files there
  File /oname=plugins\musictracker.dll ..\src\musictracker.dll
  File /oname=wmpuice.dll wmpuice.dll

  !include po_install.nsi

  RegDLL wmpuice.dll

  WriteUninstaller $INSTDIR\pidgin-musictracker-uninst.exe

  WriteRegStr HKLM "${MUSICTRACKER_UNINSTALL_KEY}" "DisplayName" "Pidgin-Musictracker plugin (remove only)"
  WriteRegStr HKLM "${MUSICTRACKER_UNINSTALL_KEY}" "HelpLink" "http://code.google.com/p/pidgin-musictracker/"
  WriteRegDWORD HKLM "${MUSICTRACKER_UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${MUSICTRACKER_UNINSTALL_KEY}" "NoRepair" 1
  WriteRegStr HKLM "${MUSICTRACKER_UNINSTALL_KEY}" "UninstallString" "$INSTDIR\pidgin-musictracker-uninst.exe"

SectionEnd ; end the section

;--------------------------------

; The stuff to uninstall
Section "Uninstall"

  Delete $INSTDIR\plugins\musictracker.dll

  UnRegDLL $INSTDIR\wmpuice.dll
  Delete $INSTDIR\wmpuice.dll

  !include po_uninstall.nsi

  DeleteRegKey HKLM "${MUSICTRACKER_UNINSTALL_KEY}"

  Delete $INSTDIR\pidgin-musictracker-uninst.exe

SectionEnd

;--------------------------------

Function .onInit

  Call IsUserAdmin
  Pop $R0
  StrCmp $R0 "true" Okadmin 0
  MessageBox MB_ICONSTOP "${Message_Not_Admin}"
  Abort

Okadmin:

; based on the .onInit function from pidgin's .nsi script
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

;--------------------------------

Function un.onInit

  Call un.IsUserAdmin
  Pop $R0
  StrCmp $R0 "true" Okadmin 0
  MessageBox MB_ICONSTOP "${Message_Not_Admin}"
  Abort
Okadmin:

FunctionEnd

;--------------------------------

; IsUserAdmin
;
; Usage:
;   Call IsUserAdmin
;   Pop $R0   ; at this point $R0 is "true" or "false"
!macro IsUserAdminMacro UN
Function ${UN}IsUserAdmin
  Push $R0
  Push $R1
  Push $R2

  ClearErrors
  UserInfo::GetName
  IfErrors IsUserAdmin_Win9x

  ; Assuming Windows NT
  Pop $R1
  UserInfo::GetAccountType
  Pop $R2
  StrCmp $R2 "Admin" 0 IsUserAdmin_Continue
  StrCpy $R0 "true"
  Goto IsUserAdmin_end

IsUserAdmin_Continue:
  StrCmp $R2 "" IsUserAdmin_Win9x
  StrCpy $R0 "false"
  Goto IsUserAdmin_end

IsUserAdmin_Win9x:
; This one means you don't need to care about admin or
; not admin because Windows 9x doesn't either
  StrCpy $R0 "true"

IsUserAdmin_end:
  ;MessageBox MB_OK 'User= "$R1"  AccountType= "$R2"  IsUserAdmin= "$R0"'
  Pop $R2
  Pop $R1
  Exch $R0

FunctionEnd
!macroend
!insertmacro IsUserAdminMacro ""
!insertmacro IsUserAdminMacro "un."

