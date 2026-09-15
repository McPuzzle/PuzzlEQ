; PuzzlEQ Inno Setup script. Compile on Windows with Inno Setup 6:
;   ISCC.exe installer\windows\PuzzlEQ.iss
; Expects Release artefacts at build\PuzzlEQ_artefacts\Release\

#define MyAppName "PuzzlEQ"
#define MyAppVersion GetEnv("PUZZLEQ_VERSION")
#if MyAppVersion == ""
  #define MyAppVersion "0.3.0"
#endif
#define Artefacts "build\PuzzlEQ_artefacts\Release"

[Setup]
AppId={{9C3E8B1A-7D4F-4C2A-9E11-A7B3C4D5E6F7}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Puzzl
SourceDir=..\..
DefaultDirName={autopf}\PuzzlEQ
DisableDirPage=yes
DefaultGroupName=PuzzlEQ
OutputDir=dist
OutputBaseFilename=PuzzlEQ-{#MyAppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
SetupLogging=yes
UninstallDisplayName=PuzzlEQ
LicenseFile=LICENSE

[Files]
Source: "{#Artefacts}\VST3\PuzzlEQ.vst3\*"; DestDir: "{commonpf}\VST3\PuzzlEQ.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\CLAP\PuzzlEQ.clap"; DestDir: "{commonpf}\CLAP"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#Artefacts}\Standalone\PuzzlEQ.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\PuzzlEQ"; Filename: "{app}\PuzzlEQ.exe"; Check: FileExists(ExpandConstant('{app}\PuzzlEQ.exe'))
Name: "{group}\Uninstall PuzzlEQ"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\PuzzlEQ.exe"; Description: "Open PuzzlEQ"; Flags: nowait postinstall skipifsilent skipifdoesntexist

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    MsgBox('PuzzlEQ is in your VST3 folder.'#13#10'Rescan plugins in your DAW.', mbInformation, MB_OK);
end;
