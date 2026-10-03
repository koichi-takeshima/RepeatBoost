#preproc ispp

#define StageDir GetEnv("REPEATBOOST_INSTALLER_STAGE")
#define ProductVersion GetEnv("REPEATBOOST_PRODUCT_VERSION")
#define WindowsVersion GetEnv("REPEATBOOST_WINDOWS_VERSION")

#if StageDir == ""
  #error REPEATBOOST_INSTALLER_STAGE must be supplied by eng/build-installer.ps1
#endif
#if ProductVersion == ""
  #error REPEATBOOST_PRODUCT_VERSION must be supplied by eng/build-installer.ps1
#endif
#if WindowsVersion == ""
  #error REPEATBOOST_WINDOWS_VERSION must be supplied by eng/build-installer.ps1
#endif

#define DotNetRuntimeUrl "https://builds.dotnet.microsoft.com/dotnet/Runtime/10.0.12/dotnet-runtime-10.0.12-win-x64.exe"
#define WindowsAppRuntimeUrl "https://aka.ms/windowsappsdk/2.5/2.5.1/windowsappruntimeinstall-x64.exe"
#define VcRuntimeUrl "https://aka.ms/vc14/vc_redist.x64.exe"

[Setup]
AppId={{41B40CE3-9167-42C2-BDF0-6AD312CA1581}
AppName=RepeatBoost
AppVersion={#ProductVersion}
AppPublisher=Koichi Takeshima
DefaultDirName={autopf}\RepeatBoost
PrivilegesRequired=lowest
SetupArchitecture=x64
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.22000
CloseApplications=yes
RestartApplications=no
SetupIconFile=..\assets\RepeatBoost.ico
UninstallDisplayName=RepeatBoost
UninstallDisplayIcon={app}\RepeatBoost.Engine.exe
OutputDir=..\artifacts\installer
OutputBaseFilename=RepeatBoost-Setup-{#ProductVersion}
Compression=lzma2
SolidCompression=yes
VersionInfoVersion={#WindowsVersion}
VersionInfoProductName=RepeatBoost
VersionInfoCompany=Koichi Takeshima
VersionInfoDescription=RepeatBoost Setup
DisableProgramGroupPage=yes

[Files]
Source: "{#StageDir}\RepeatBoost.Engine.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\settings\*"; DestDir: "{app}\settings"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\RepeatBoost"; Filename: "{app}\RepeatBoost.Engine.exe"; IconFilename: "{app}\RepeatBoost.Engine.exe"

[Code]
const
  EngineMutexName = 'RepeatBoost.Engine.Instance';
  RunKeyPath = 'Software\Microsoft\Windows\CurrentVersion\Run';
  RunValueName = 'RepeatBoost';
  DotNetRuntimeUrl = '{#DotNetRuntimeUrl}';
  WindowsAppRuntimeUrl = '{#WindowsAppRuntimeUrl}';
  VcRuntimeUrl = '{#VcRuntimeUrl}';

var
  UpgradeEngineStateCaptured: Boolean;
  UpgradeEngineWasRunning: Boolean;

function HasDotNet10AtRoot(const Root: String): Boolean;
var
  FindRec: TFindRec;
  Pattern: String;
  Version: Int64;
begin
  Result := False;
  if Root = '' then
    Exit;

  Pattern := AddBackslash(Root) + 'shared\Microsoft.NETCore.App\10.0.*';
  if FindFirst(Pattern, FindRec) then
  begin
    try
      repeat
        if ((FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0) and
           StrToVersion(FindRec.Name, Version) then
        begin
          Result := True;
          Exit;
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

function HasDotNet10: Boolean;
var
  Root: String;
begin
  Root := GetEnv('DOTNET_ROOT_X64');
  if Root <> '' then
  begin
    Result := HasDotNet10AtRoot(Root);
    Exit;
  end;

  Root := GetEnv('DOTNET_ROOT');
  if Root <> '' then
  begin
    Result := HasDotNet10AtRoot(Root);
    Exit;
  end;

  if RegQueryStringValue(
       HKEY_LOCAL_MACHINE_64,
       'SOFTWARE\dotnet\Setup\InstalledVersions\x64',
       'InstallLocation',
       Root) then
  begin
    Result := HasDotNet10AtRoot(Root);
    Exit;
  end;

  Result := HasDotNet10AtRoot(ExpandConstant('{commonpf64}\dotnet'));
end;

function HasWindowsAppRuntime: Boolean;
var
  ScriptPath: String;
  Script: AnsiString;
  PowerShellPath: String;
  ExitCode: Integer;
begin
  ScriptPath := ExpandConstant('{tmp}\RepeatBoost-CheckWindowsAppRuntime.ps1');
  Script :=
    '$ErrorActionPreference = "Stop"' + #13#10 +
    '$min = [version]"2.5.1.0"' + #13#10 +
    '$publisherSuffix = "_8wekyb3d8bbwe"' + #13#10 +
    '$packages = @(Get-AppxPackage)' + #13#10 +
    'function Has-Package([string]$name, [string]$arch, [bool]$isFramework) {' + #13#10 +
    '  @($packages | Where-Object { $_.Name -eq $name -and "$($_.Architecture)" -eq $arch -and $_.IsFramework -eq $isFramework -and [version]$_.Version -ge $min -and "$($_.Status)" -eq "Ok" -and $_.PackageFamilyName.EndsWith($publisherSuffix) }).Count -gt 0' + #13#10 +
    '}' + #13#10 +
    '$frameworkX86 = Has-Package "Microsoft.WindowsAppRuntime.2" "X86" $true' + #13#10 +
    '$frameworkX64 = Has-Package "Microsoft.WindowsAppRuntime.2" "X64" $true' + #13#10 +
    '$mainX64 = Has-Package "MicrosoftCorporationII.WinAppRuntime.Main.2" "X64" $false' + #13#10 +
    '$singletonX64 = Has-Package "MicrosoftCorporationII.WinAppRuntime.Singleton" "X64" $false' + #13#10 +
    '$ddlmX86 = @($packages | Where-Object { $_.Name -match "^Microsoft\.WinAppRuntime\.DDLM\.2\." -and "$($_.Architecture)" -eq "X86" -and -not $_.IsFramework -and [version]$_.Version -ge $min -and "$($_.Status)" -eq "Ok" -and $_.PackageFamilyName.EndsWith($publisherSuffix) }).Count -gt 0' + #13#10 +
    '$ddlmX64 = @($packages | Where-Object { $_.Name -match "^Microsoft\.WinAppRuntime\.DDLM\.2\." -and "$($_.Architecture)" -eq "X64" -and -not $_.IsFramework -and [version]$_.Version -ge $min -and "$($_.Status)" -eq "Ok" -and $_.PackageFamilyName.EndsWith($publisherSuffix) }).Count -gt 0' + #13#10 +
    'if ($frameworkX86 -and $frameworkX64 -and $mainX64 -and $singletonX64 -and $ddlmX86 -and $ddlmX64) { exit 0 }' + #13#10 +
    'exit 1' + #13#10;

  if not SaveStringToFile(ScriptPath, Script, False) then
  begin
    Result := False;
    Exit;
  end;

  PowerShellPath := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
  Result := Exec(
    PowerShellPath,
    '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + ScriptPath + '"',
    '',
    SW_HIDE,
    ewWaitUntilTerminated,
    ExitCode) and (ExitCode = 0);
end;

function HasVcRuntime: Boolean;
var
  Installed: Cardinal;
  Major: Cardinal;
  Minor: Cardinal;
  Build: Cardinal;
  Key: String;
begin
  Result := False;
  Key := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64';

  if not RegQueryDWordValue(HKEY_LOCAL_MACHINE_64, Key, 'Installed', Installed) or
     (Installed <> 1) or
     not RegQueryDWordValue(HKEY_LOCAL_MACHINE_64, Key, 'Major', Major) or
     not RegQueryDWordValue(HKEY_LOCAL_MACHINE_64, Key, 'Minor', Minor) or
     not RegQueryDWordValue(HKEY_LOCAL_MACHINE_64, Key, 'Bld', Build) then
    Exit;

  Result :=
    (Major > 14) or
    ((Major = 14) and
     ((Minor > 51) or
      ((Minor = 51) and (Build >= 36247))));
end;

function DownloadPrerequisite(
  const Url: String;
  const BaseName: String;
  var ErrorText: String): Boolean;
begin
  try
    DownloadTemporaryFile(Url, BaseName, '', nil);
    Result := True;
  except
    ErrorText := 'Failed to download ' + BaseName + ': ' + GetExceptionMessage;
    Result := False;
  end;
end;

function EnsureDotNetRuntime(var NeedsRestart: Boolean; var ErrorText: String): Boolean;
var
  InstallerPath: String;
  ExitCode: Integer;
begin
  if HasDotNet10 then
  begin
    Result := True;
    Exit;
  end;

  if not DownloadPrerequisite(
       DotNetRuntimeUrl,
       'dotnet-runtime-10.0.12-win-x64.exe',
       ErrorText) then
  begin
    Result := False;
    Exit;
  end;

  InstallerPath := ExpandConstant('{tmp}\dotnet-runtime-10.0.12-win-x64.exe');
  if not ShellExec(
       'runas',
       InstallerPath,
       '/install /quiet /norestart',
       '',
       SW_SHOWNORMAL,
       ewWaitUntilTerminated,
       ExitCode) then
  begin
    ErrorText := 'The .NET Runtime installer could not be started or elevation was cancelled.';
    Result := False;
    Exit;
  end;

  if (ExitCode <> 0) and (ExitCode <> 3010) then
  begin
    ErrorText := 'The .NET Runtime installer failed with exit code ' + IntToStr(ExitCode) + '.';
    Result := False;
    Exit;
  end;

  if ExitCode = 3010 then
  begin
    NeedsRestart := True;
    ErrorText := 'The .NET Runtime installer requires Windows to restart before RepeatBoost can be installed.';
    Result := False;
    Exit;
  end;

  Result := HasDotNet10;
  if not Result then
    ErrorText := 'The required .NET 10 x64 Runtime was not detected after installation.';
end;

function EnsureWindowsAppRuntime(var ErrorText: String): Boolean;
var
  InstallerPath: String;
  ExitCode: Integer;
begin
  if HasWindowsAppRuntime then
  begin
    Result := True;
    Exit;
  end;

  if not DownloadPrerequisite(
       WindowsAppRuntimeUrl,
       'WindowsAppRuntimeInstall-x64.exe',
       ErrorText) then
  begin
    Result := False;
    Exit;
  end;

  InstallerPath := ExpandConstant('{tmp}\WindowsAppRuntimeInstall-x64.exe');
  if not Exec(
       InstallerPath,
       '--quiet',
       '',
       SW_SHOWNORMAL,
       ewWaitUntilTerminated,
       ExitCode) then
  begin
    ErrorText := 'The Windows App Runtime installer could not be started.';
    Result := False;
    Exit;
  end;

  if ExitCode <> 0 then
  begin
    ErrorText := 'The Windows App Runtime installer failed with exit code ' + IntToStr(ExitCode) + '.';
    Result := False;
    Exit;
  end;

  Result := HasWindowsAppRuntime;
  if not Result then
    ErrorText := 'The required Windows App Runtime complete set was not detected after installation.';
end;

function EnsureVcRuntime(var ErrorText: String): Boolean;
var
  InstallerPath: String;
  ExitCode: Integer;
begin
  if HasVcRuntime then
  begin
    Result := True;
    Exit;
  end;

  if not DownloadPrerequisite(
       VcRuntimeUrl,
       'vc_redist.x64.exe',
       ErrorText) then
  begin
    Result := False;
    Exit;
  end;

  InstallerPath := ExpandConstant('{tmp}\vc_redist.x64.exe');
  if not ShellExec(
       'runas',
       InstallerPath,
       '/install /quiet /norestart',
       '',
       SW_SHOWNORMAL,
       ewWaitUntilTerminated,
       ExitCode) then
  begin
    ErrorText := 'The Visual C++ Runtime installer could not be started or elevation was cancelled.';
    Result := False;
    Exit;
  end;

  if ExitCode <> 0 then
  begin
    ErrorText := 'The Visual C++ Runtime installer failed with exit code ' + IntToStr(ExitCode) + '.';
    Result := False;
    Exit;
  end;

  Result := HasVcRuntime;
  if not Result then
    ErrorText := 'The required Visual C++ v14 x64 Runtime was not detected after installation.';
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ErrorText: String;
begin
  Result := '';

  if not UpgradeEngineStateCaptured then
  begin
    UpgradeEngineWasRunning :=
      FileExists(ExpandConstant('{app}\RepeatBoost.Engine.exe')) and
      CheckForMutexes(EngineMutexName);
    UpgradeEngineStateCaptured := True;
  end;

  if not EnsureDotNetRuntime(NeedsRestart, ErrorText) then
  begin
    Result := ErrorText;
    Exit;
  end;

  if not EnsureWindowsAppRuntime(ErrorText) then
  begin
    Result := ErrorText;
    Exit;
  end;

  if not EnsureVcRuntime(ErrorText) then
  begin
    Result := ErrorText;
    Exit;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ExitCode: Integer;
begin
  if (CurStep = ssPostInstall) and UpgradeEngineWasRunning then
  begin
    if not Exec(
         ExpandConstant('{app}\RepeatBoost.Engine.exe'),
         '',
         ExpandConstant('{app}'),
         SW_SHOWNORMAL,
         ewNoWait,
         ExitCode) then
      Log('Upgrade completed, but RepeatBoost.Engine could not be restarted.');
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  CurrentCommand: String;
  ExpectedCommand: String;
begin
  if CurUninstallStep <> usUninstall then
    Exit;

  ExpectedCommand := '"' + ExpandConstant('{app}\RepeatBoost.Engine.exe') + '"';
  if RegQueryStringValue(
       HKEY_CURRENT_USER,
       RunKeyPath,
       RunValueName,
       CurrentCommand) and
     (CompareText(CurrentCommand, ExpectedCommand) = 0) then
    RegDeleteValue(HKEY_CURRENT_USER, RunKeyPath, RunValueName);
end;
