; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "Baader Planetarium SteelDrive II Focuser X2 plugin"
#define MyAppVersion "1.0"
#define MyAppPublisher "RTI-Zone"
#define MyAppURL "https://rti-zone.org"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{B3AEF9EE-248D-11E8-8528-4FE071C4F632}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={code:TSXInstallDir}\Resources\Common
DefaultGroupName={#MyAppName}

; Need to customise these
; First is where you want the installer to end up
OutputDir=installer
; Next is the name of the installer
OutputBaseFilename=SteeldriveII_X2_Installer
; Final one is the icon you would like on the installer. Comment out if not needed.
SetupIconFile=rti_zone_logo.ico
Compression=lzma
SolidCompression=yes
; We don't want users to be able to select the drectory since read by TSXInstallDir below
DisableDirPage=yes
; Uncomment this if you don't want an uninstaller.
;Uninstallable=no
CloseApplications=yes
DirExistsWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Dirs]
Name: "{app}\Plugins\FocuserPlugins";
Name: "{app}\Plugins64\FocuserPlugins";

[Files]
; WIll also need to customise these!
Source: "focuserlist SteelDriveII.txt";                         DestDir: "{app}\Miscellaneous Files"; Flags: ignoreversion
Source: "focuserlist SteelDriveII.txt";                         DestDir: "{app}\Miscellaneous Files"; DestName: "focuserlist64 SteelDriveII.txt"; Flags: ignoreversion
; 32 bit
Source: "libSteeldriveII\\Win32\Release\libSteeldriveII.dll";   DestDir: "{app}\Plugins\FocuserPlugins"; Flags: ignoreversion
Source: "SteeldriveII.ui";                                      DestDir: "{app}\Plugins\FocuserPlugins"; Flags: ignoreversion
; 64 bit
Source: "libSteeldriveII\x64\Release\libSteeldriveII.dll";      DestDir: "{app}\Plugins64\FocuserPlugins"; Flags: ignoreversion; Check: DirExists(ExpandConstant('{app}\Plugins64\FocuserPlugins'))
Source: "SteeldriveII.ui";                                      DestDir: "{app}\Plugins64\FocuserPlugins"; Flags: ignoreversion; Check: DirExists(ExpandConstant('{app}\Plugins64\FocuserPlugins'))


[Code]
{* Below is a function to read TheSkyXInstallPath.txt and confirm that the directory does exist
   This is then used in the DefaultDirName above
   *}
var
  Location: String;
  LoadResult: Boolean;

function TSXInstallDir(Param: String) : String;
begin
  LoadResult := LoadStringFromFile(ExpandConstant('{userdocs}') + '\Software Bisque\TheSkyX Professional Edition\TheSkyXInstallPath.txt', Location);
  if not LoadResult then
    LoadResult := LoadStringFromFile(ExpandConstant('{userdocs}') + '\Software Bisque\TheSky Professional Edition 64\TheSkyXInstallPath.txt', Location);
    if not LoadResult then
      LoadResult := BrowseForFolder('Please locate the installation path for TheSkyX', Location, False);
      if not LoadResult then
        RaiseException('Unable to find the installation path for TheSkyX');
  if not DirExists(Location) then
    RaiseException('TheSkyX installation directory ' + Location + ' does not exist');
  Result := Location;
end;
