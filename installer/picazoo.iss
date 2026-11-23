; Installateur Windows de Picazoo (Inno Setup 6).
;
; Compilation (après « cmake --install build/windows-release --prefix dist/picazoo ») :
;   ISCC.exe installer\picazoo.iss /DAppVersion=0.2.0 /DOutputBase=Picazoo-0.2.0-win64-setup
; Les chemins sont relatifs à ce dossier « installer ».

#ifndef AppVersion
  #define AppVersion "0.1.0-dev"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\picazoo"
#endif
#ifndef OutputBase
  #define OutputBase "Picazoo-" + AppVersion + "-win64-setup"
#endif

[Setup]
AppId={{6E4F3D0B-8C1A-4B6E-9F27-3A5D2C7B1E90}
AppName=Picazoo
AppVersion={#AppVersion}
AppVerName=Picazoo {#AppVersion}
AppPublisher=Picazoo
AppPublisherURL=https://github.com/Princinho/picazoo
DefaultDirName={autopf}\Picazoo
DefaultGroupName=Picazoo
DisableProgramGroupPage=yes
; Installation pour l'utilisateur seul ou pour tous (choix proposé).
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename={#OutputBase}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\bin\picazoo.exe
UninstallDisplayName=Picazoo
#if FileExists(AddBackslash(SourcePath) + "..\assets\icons\picazoo.ico")
SetupIconFile=..\assets\icons\picazoo.ico
#endif

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "vc_redist.x64.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\bin\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\Picazoo"; Filename: "{app}\bin\picazoo.exe"
Name: "{autodesktop}\Picazoo"; Filename: "{app}\bin\picazoo.exe"; Tasks: desktopicon

[Run]
; Bibliothèques d'exécution Visual C++ (déjà présentes sur la plupart des PC).
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installation des bibliothèques Visual C++…"; Flags: waituntilterminated skipifdoesntexist; Check: IsAdminInstallMode
; Visionneuse autonome (005) : Picazoo apparaît dans « Ouvrir avec » et dans les
; applications par défaut de Windows, sans le devenir : c'est l'utilisateur qui choisit.
Filename: "{app}\bin\picazoo-viewer.exe"; Parameters: "--register"; StatusMsg: "Inscription de la visionneuse auprès de Windows…"; Flags: runhidden waituntilterminated; Check: not IsAdminInstallMode
Filename: "{app}\bin\picazoo-viewer.exe"; Parameters: "--register --machine"; StatusMsg: "Inscription de la visionneuse auprès de Windows…"; Flags: runhidden waituntilterminated; Check: IsAdminInstallMode
Filename: "{app}\bin\picazoo.exe"; Description: "{cm:LaunchProgram,Picazoo}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Retire exactement ce que --register a écrit (contracts/storage-and-registry.md de la 005).
Filename: "{app}\bin\picazoo-viewer.exe"; Parameters: "--unregister"; Flags: runhidden waituntilterminated; RunOnceId: "PicazooViewerUnregisterUser"; Check: not IsAdminInstallMode
Filename: "{app}\bin\picazoo-viewer.exe"; Parameters: "--unregister --machine"; Flags: runhidden waituntilterminated; RunOnceId: "PicazooViewerUnregisterMachine"; Check: IsAdminInstallMode

; Les données de Picazoo (%LOCALAPPDATA%\Picazoo) sont conservées à la désinstallation :
; ce ne sont qu'un index et des vignettes, reconstruits au besoin.
