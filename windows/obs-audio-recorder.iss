#define MyAppName "OBS Audio Recorder"
#define MyAppPublisher "Podders"
#define MyAppURL "https://github.com/Podders/obs-audio-recorder"
#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif

[Setup]
AppId={{7C0F7A6B-4B66-4E2C-8C4A-2E934D5D2F65}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonappdata}\obs-studio\plugins\obs-audio-recorder
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=obs-audio-recorder-{#MyAppVersion}-windows-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UsePreviousAppDir=no
UninstallDisplayIcon={app}\bin\64bit\obs-audio-recorder.dll

[Files]
Source: "..\release\obs-audio-recorder\bin\64bit\*"; DestDir: "{app}\bin\64bit"; Flags: recursesubdirs createallsubdirs ignoreversion; Excludes: "*.pdb"
Source: "..\release\obs-audio-recorder\data\*"; DestDir: "{app}\data"; Flags: recursesubdirs createallsubdirs ignoreversion

[InstallDelete]

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
