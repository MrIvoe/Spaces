; Inno Setup Script for Spaces
; This script creates a professional Windows installer for the Spaces application

#define MyAppName "Spaces"
#define MyAppVersion "1.01.001"
#define MyAppPublisher "SimpleSpaces"
#define MyAppURL "https://github.com/MrIvoe/Spaces"
#define MyAppExeName "Spaces.exe"

; Build output
#define BuildDir GetEnv("BUILD_OUTPUT_DIR")
#if BuildDir == ""
  #define BuildDir "build\bin\Debug"
#endif

[Setup]
AppId={{5A5F5B5E-5C5D-5E5F-5A5B5C5D5E5F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=output
OutputBaseFilename=Spaces-Setup-{#MyAppVersion}
SetupIconFile=installer\assets\Spaces.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoVersion={#MyAppVersion}
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoCopyright=Copyright (c) SimpleSpaces Contributors
LicenseFile=LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "&Create a desktop shortcut"; GroupDescription: "Additional tasks:"; Flags: unchecked
Name: "quicklaunchicon"; Description: "Create a &Quick Launch icon"; GroupDescription: "Additional tasks:"; Flags: unchecked,skipifdoesntwork
Name: "startup"; Description: "Launch {#MyAppName} on &startup"; GroupDescription: "Additional tasks:"

[Files]
; Application executable and runtime files
Source: "{#BuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; License and documentation
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion

; Plugin catalog (initial)
Source: "installer\assets\plugin-catalog.json"; DestDir: "{app}\assets"; Flags: ignoreversion

; Note: User data folders (%LOCALAPPDATA%\SimpleSpaces\) are created by the app on first run

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\{#MyAppPublisher} on GitHub"; Filename: "{#MyAppURL}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Registry]
; Register app in Add/Remove Programs
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: startup

; File associations and file type registration can be added here later
; Root: HKCR; Subkey: ".spaces"; ValueType: string; ValueName: ""; ValueData: "SpacesFile"; Flags: uninsdeletekey

[Code]
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
