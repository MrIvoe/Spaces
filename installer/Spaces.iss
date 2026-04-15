; Inno Setup Script for Spaces
; This script creates a professional Windows installer for the Spaces application

#define MyAppName "Spaces"
#define MyAppVersion "1.01.012"
#define MyAppPublisher "SimpleSpaces"
#define MyAppURL "https://github.com/MrIvoe/Spaces"
#define MyAppExeName "Spaces.exe"

; Build output
#define BuildDir GetEnv("BUILD_OUTPUT_DIR")
#if BuildDir == ""
  #define BuildDir "..\build\bin\Release"
#endif

#ifexist "assets\Spaces.ico"
  #define SetupIconPath "assets\Spaces.ico"
#endif

#ifexist "..\LICENSE"
  #define LicensePath "..\LICENSE"
#endif

[Setup]
AppId=SimpleSpaces.Spaces
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#SourcePath}output
OutputBaseFilename=Spaces.{#MyAppVersion}
#ifdef SetupIconPath
SetupIconFile={#SetupIconPath}
#endif
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoVersion={#MyAppVersion}
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoCopyright=Copyright (c) SimpleSpaces Contributors
#ifdef LicensePath
LicenseFile={#LicensePath}
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "&Create a desktop shortcut"; GroupDescription: "Additional tasks:"; Flags: unchecked
Name: "startup"; Description: "Launch {#MyAppName} on &startup"; GroupDescription: "Additional tasks:"

[Files]
; Application executable and runtime files
Source: "{#BuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; License and documentation
#ifdef LicensePath
Source: "{#LicensePath}"; DestDir: "{app}"; Flags: ignoreversion
#endif
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion

; Plugin catalog (initial)
Source: "assets\plugin-catalog.json"; DestDir: "{app}\assets"; Flags: ignoreversion

; Note: User data folders (%LOCALAPPDATA%\SimpleSpaces\) are created by the app on first run

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\{#MyAppPublisher} on GitHub"; Filename: "{#MyAppURL}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[InstallDelete]
; Remove all DLLs from a previous installation so renamed/removed libraries
; do not accumulate across upgrades.
Type: files; Name: "{app}\*.dll"
; Remove the main executable so the new build is always laid down fresh.
Type: files; Name: "{app}\{#MyAppExeName}"

[Registry]
; Register app in Add/Remove Programs
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: startup

; File associations and file type registration can be added here later
; Root: HKCR; Subkey: ".spaces"; ValueType: string; ValueName: ""; ValueData: "SpacesFile"; Flags: uninsdeletekey

[Code]
{ Kill any running Spaces.exe instances before files are written so the
  installer does not fail with "file in use" errors. }
procedure KillRunningApp;
var
  ResultCode: Integer;
begin
  Exec('taskkill.exe', '/F /IM {#MyAppExeName}', '', SW_HIDE,
       ewWaitUntilTerminated, ResultCode);
  { Brief pause to let Windows fully release the file handles }
  Sleep(800);
end;

{ Called by Inno Setup before files are installed – the ideal hook for
  stopping the running process. Returning a non-empty string aborts setup. }
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  KillRunningApp;
  Result := '';
end;

procedure InitializeWizard;
begin
  { Custom initialization can happen here }
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  if CurPageID = wpSelectDir then
  begin
    { Validate selected directory here if needed }
    Result := True;
  end
  else
    Result := True;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    { Post-installation tasks can be added here }
    { For example, initialization of plugin cache, etc. }
  end;
end;

procedure DeinitializeSetup;
begin
  { Cleanup on uninstall/cancellation can be added here }
end;
