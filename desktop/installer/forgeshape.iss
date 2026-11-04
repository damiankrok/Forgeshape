; ForgeShape for Windows: the installer (`DESKTOP-R1`).
;
; Built by Inno Setup (a build tool; nothing of it ships inside the product):
;   ISCC.exe /DAppVersion=0.1.0 /DBuildDir=<dir with ForgeShape.exe>
;            /DIconFile=<ForgeShape.ico> desktop\installer\forgeshape.iss
;
; A per-user install (no administrator rights) under
; %LOCALAPPDATA%\Programs\ForgeShape, a Start-menu entry, an optional desktop
; shortcut, .forge files opening in ForgeShape, and an uninstaller. UPDATING is
; running a newer setup: the AppId below never changes, so it installs over the
; old version in place and keeps the user's theme file and projects.

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\desktop\Release"
#endif
#ifndef IconFile
  #define IconFile "..\..\build\desktop\ForgeShape.ico"
#endif

[Setup]
AppId={{8B4E2A1C-3F6D-4E9A-9C21-5A7F0D3E6B42}
AppName=ForgeShape
AppVersion={#AppVersion}
AppVerName=ForgeShape {#AppVersion}
AppPublisher=ForgeShape
DefaultDirName={localappdata}\Programs\ForgeShape
DefaultGroupName=ForgeShape
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=ForgeShape-Setup-{#AppVersion}
SetupIconFile={#IconFile}
UninstallDisplayIcon={app}\ForgeShape.exe
UninstallDisplayName=ForgeShape
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#BuildDir}\ForgeShape.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\ForgeShape"; Filename: "{app}\ForgeShape.exe"
Name: "{autodesktop}\ForgeShape"; Filename: "{app}\ForgeShape.exe"; Tasks: desktopicon

[Registry]
; .forge opens in ForgeShape, for this user only.
Root: HKCU; Subkey: "Software\Classes\.forge"; ValueType: string; ValueName: ""; ValueData: "ForgeShape.Project"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\ForgeShape.Project"; ValueType: string; ValueName: ""; ValueData: "ForgeShape project"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\ForgeShape.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\ForgeShape.exe,0"
Root: HKCU; Subkey: "Software\Classes\ForgeShape.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\ForgeShape.exe"" ""%1"""

[Run]
Filename: "{app}\ForgeShape.exe"; Description: "{cm:LaunchProgram,ForgeShape}"; Flags: nowait postinstall skipifsilent
