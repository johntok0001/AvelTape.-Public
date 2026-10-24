; Inno Setup script for Avel Tape (Windows, VST3).
; Build:  iscc /DAppVersion=1.0.0 packaging\windows\AvelTape.iss
; Expects the plugin at build\AvelTape_artefacts\Release\VST3\Avel Tape.vst3

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

#define Root "..\.."

[Setup]
AppId={{DD0900E1-A531-4D2D-8C46-A69F5332202B}
AppName=Avel Tape
AppVersion={#AppVersion}
AppVerName=Avel Tape {#AppVersion}
AppPublisher=Avel
DefaultDirName={commoncf64}\VST3\Avel Tape.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
UninstallFilesDir={autopf}\Avel\Avel Tape
UninstallDisplayName=Avel Tape {#AppVersion} (VST3)
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Root}\dist
OutputBaseFilename=AvelTape-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Messages]
en.WelcomeLabel2=This will install Avel Tape {#AppVersion} (VST3) into%n%nC:\Program Files\Common Files\VST3%n%nClose your DAW before continuing.
ru.WelcomeLabel2=Будет установлен Avel Tape {#AppVersion} (VST3) в папку%n%nC:\Program Files\Common Files\VST3%n%nПеред продолжением закройте DAW.
en.FinishedLabel=Avel Tape is installed. Start your DAW and rescan plugins (REAPER: Preferences > Plug-ins > VST > Re-scan).
ru.FinishedLabel=Avel Tape установлен. Запустите DAW и пересканируйте плагины (REAPER: Preferences > Plug-ins > VST > Re-scan).

[InstallDelete]
; remove files of an older version inside the bundle
Type: filesandordirs; Name: "{commoncf64}\VST3\Avel Tape.vst3"

[Files]
Source: "{#Root}\build\AvelTape_artefacts\Release\VST3\Avel Tape.vst3\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\README.md"; DestDir: "{autopf}\Avel\Avel Tape"; DestName: "README.txt"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Avel Tape.vst3"
