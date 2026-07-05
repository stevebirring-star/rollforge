; RollForge -- Inno Setup installer script (Phase 7).
;
; Compiled by packaging/windows/build-packages.ps1, which passes the built exe,
; version and output dir as /D defines. Sensible defaults let you also run it by
; hand: iscc packaging\windows\rollforge.iss

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceExe
  #define SourceExe "..\..\build\RollForge_artefacts\Release\RollForge.exe"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif

#define MyAppName "RollForge"
#define MyAppPublisher "StackBase"
#define MyAppExeName "RollForge.exe"
#define MyAppURL "https://github.com/stevebirring-star/rollforge"

[Setup]
; A fixed AppId identifies the app across versions for upgrades/uninstall.
AppId={{7B2E9C4A-1F3D-4E6B-9A2C-8D5F0E1A3B7C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
OutputDir={#OutDir}
OutputBaseFilename=RollForge-{#MyAppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
