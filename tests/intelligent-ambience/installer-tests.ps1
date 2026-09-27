# SPDX-License-Identifier: GPL-2.0-or-later
<# Isolated production-function tests. Does not execute installer modes, inspect
the real OpenRGB process, load a DLL, or access any live installation/config. #>
[CmdletBinding()]
param([string]$Installer)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if(!$Installer){$Installer=Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '../../tools/install-intelligent-ambience.ps1'}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Resolve-Path -LiteralPath $Installer).Path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Installer syntax errors.'}
# Only function declarations, plus the two public filename allowlists. Never
# dot-source the entrypoint, whose Apply/Rollback branches access live paths.
foreach($function in $ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$true)) {
    Invoke-Expression $function.Extent.Text
}
foreach($name in @('relativeFiles','profileNames')) {
    $assignment=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.AssignmentStatementAst]},$true) |
        Where-Object {$_.Left.Extent.Text -eq ('$'+$name)})
    if($assignment.Count -ne 1){throw "Cannot extract allowlist $name."}
    Set-Variable -Name $name -Scope Script -Value (Invoke-Expression $assignment[0].Right.Extent.Text)
}
$script:checks=0;$script:AppChecks=0;$script:BeforeClosedCheck=$null
$script:TestRoot=[IO.Path]::Combine([IO.Path]::GetTempPath(),'OpenRGB-InstallerTests-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($script:TestRoot)|Out-Null
$script:Junctions=@()
function Check([bool]$Good,[string]$Message){++$script:checks;if(!$Good){throw "CHECK $script:checks : $Message"}}
function Throws([scriptblock]$Action,[string]$Message){$thrown=$false;try{& $Action|Out-Null}catch{$thrown=$true};Check $thrown $Message}
function Guard-TestPath([string]$Path){
    $full=[IO.Path]::GetFullPath($Path)
    if(!$full.StartsWith($script:TestRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture escaped its private temporary directory.'}
}
function Put([string]$Path,[string]$Value){Guard-TestPath $Path;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path))|Out-Null;[IO.File]::WriteAllText($Path,$Value)}
# The only overridden production function. The stub is restricted to fixture
# roots and counts calls; it never enumerates/stops/starts the user's process.
function Assert-AppClosed {
    foreach($path in @($script:InstallRoot,$script:ConfigRoot,$script:BackupDirectory)){Guard-TestPath $path}
    ++$script:AppChecks
    if($script:BeforeClosedCheck){$action=$script:BeforeClosedCheck;$script:BeforeClosedCheck=$null;& $action}
}
function New-Fixture {
    $base=Join-Path $script:TestRoot ([Guid]::NewGuid().ToString('N'))
    $script:InstallRoot=Join-Path $base 'installation';$script:ConfigRoot=Join-Path $base 'configuration';$script:BackupDirectory=Join-Path $base 'backup'
    foreach($path in @($script:InstallRoot,$script:ConfigRoot,$script:BackupDirectory)){[IO.Directory]::CreateDirectory($path)|Out-Null}
    $files=@();$profiles=@();$index=0
    foreach($relative in $script:relativeFiles){
        $target=Join-Path $script:InstallRoot $relative;$backup=Join-Path $script:BackupDirectory ('binaries/'+$relative)
        $exists=$index%2 -eq 0;$original='';if($exists){Put $backup ('original-'+$index);$original=Hash-File $backup}
        Put $target ('installed-'+$index)
        $files += [pscustomobject]@{path=$relative;existed=$exists;original_sha256=$original;installed_sha256=(Hash-File $target)}
        ++$index
    }
    foreach($name in $script:profileNames){
        $relative='profiles/'+$name;$target=Join-Path $script:ConfigRoot $relative;$backup=Join-Path $script:BackupDirectory ('profile-binaries/'+$relative)
        $exists=$index%2 -eq 0;$original='';if($exists){Put $backup ('original-profile-'+$index);$original=Hash-File $backup}
        Put $target ('installed-profile-'+$index)
        $profiles += [pscustomobject]@{path=$relative;existed=$exists;original_sha256=$original;installed_sha256=(Hash-File $target)}
        ++$index
    }
    Put (Join-Path $script:ConfigRoot 'OpenRGB.json') '{"unchanged":true}'
    return [pscustomobject]@{schema=1;state='applied';install_root=$script:InstallRoot;config_root=$script:ConfigRoot;files=$files;profile_files=$profiles}
}
function Targets($Deployment){
    foreach($f in $Deployment.files){[pscustomobject]@{data=$f;target=(Join-Path $script:InstallRoot $f.path);backup=(Join-Path $script:BackupDirectory ('binaries/'+$f.path))}}
    foreach($f in $Deployment.profile_files){[pscustomobject]@{data=$f;target=(Join-Path $script:ConfigRoot $f.path);backup=(Join-Path $script:BackupDirectory ('profile-binaries/'+$f.path))}}
}
function Assert-Originals($Deployment){
    foreach($item in @(Targets $Deployment)){
        if($item.data.existed){Check ((Hash-File $item.target) -eq $item.data.original_sha256) 'original restored exactly'}
        else{Check (![IO.File]::Exists($item.target)) 'new file removed'}
    }
    Check ([IO.File]::ReadAllText((Join-Path $script:ConfigRoot 'OpenRGB.json')) -eq '{"unchanged":true}') 'unrelated current configuration untouched'
}
function Snapshot($Deployment){$out=@{};foreach($item in @(Targets $Deployment)){$out[$item.target]=if([IO.File]::Exists($item.target)){Hash-File $item.target}else{'absent'}};return $out}
function Assert-Same($Before){foreach($entry in $Before.GetEnumerator()){$now=if([IO.File]::Exists($entry.Key)){Hash-File $entry.Key}else{'absent'};Check ($now -eq $entry.Value) 'prevalidation leaves every deployed file unchanged'}}
try {
    foreach($invalid in @('','relative','C:relative','\root-relative','C:\','\\server\share\')){Throws {Full-Path $invalid} 'relative paths/volume or share roots rejected'}
    Check ((Full-Path ($script:TestRoot+'\folder\..\sub')) -eq ($script:TestRoot+'\sub')) 'absolute paths canonicalized'
    $f=New-Fixture;Restore-Binaries $f $true;Assert-Originals $f
    Restore-Binaries $f $true;Assert-Originals $f # Interrupted/repeated rollback is idempotent.
    $f=New-Fixture;$items=@(Targets $f)
    foreach($item in @($items[0],$items[1],$items[6])){
        if($item.data.existed){[IO.File]::Copy($item.backup,$item.target,$true)}else{[IO.File]::Delete($item.target)}
    }
    Restore-Binaries $f $false;Assert-Originals $f # Mixed partial Apply, automatic failure recovery.
    foreach($changed in @('binary','profile')){
        $f=New-Fixture;$items=@(Targets $f);$target=if($changed -eq 'binary'){$items[2].target}else{$items[-1].target}
        Put $target 'concurrent-user-change';$before=Snapshot $f
        Throws {Restore-Binaries $f $false} 'automatic rollback refuses changed files too';Assert-Same $before
    }
    $f=New-Fixture;$before=Snapshot $f;Put (@(Targets $f)[0].backup) 'corrupt-backup'
    Throws {Restore-Binaries $f $true} 'backup checksum corruption refused before writes';Assert-Same $before
    $f=New-Fixture;$before=Snapshot $f;$f.files[0].path='../outside.dll'
    Throws {Restore-Binaries $f $true} 'non-allowlisted path refused';Assert-Same $before
    $f=New-Fixture;$before=Snapshot $f;$f.profile_files[0].path='profiles/../'+$script:profileNames[0]
    Throws {Restore-Binaries $f $true} 'profile path traversal refused';Assert-Same $before
    $f=New-Fixture;$script:RaceTarget=@(Targets $f)[0].target
    $script:BeforeClosedCheck={Put $script:RaceTarget 'concurrent-after-validation'}
    Throws {Restore-Binaries $f $true} 'per-item rollback checks catch changes after initial validation'
    Check ([IO.File]::ReadAllText($script:RaceTarget) -eq 'concurrent-after-validation') 'late concurrent change preserved'
    # Real NTFS junction within the fixture only. Neither existing nor missing
    # children may traverse it, including destination creation through a junction.
    $real=Join-Path $script:TestRoot 'outside-fixture-target';[IO.Directory]::CreateDirectory($real)|Out-Null
    Put (Join-Path $real 'existing.txt') 'keep'
    $junction=Join-Path $script:TestRoot 'linked-parent'
    New-Item -ItemType Junction -Path $junction -Target $real|Out-Null;$script:Junctions += $junction
    Throws {Assert-NoReparse (Join-Path $junction 'existing.txt')} 'existing child under junction rejected'
    Throws {Assert-NoReparse (Join-Path $junction 'missing/deeper.txt')} 'absent child under junction rejected'
    Throws {Copy-Verified (Join-Path $real 'existing.txt') (Join-Path $junction 'new.txt') (Hash-File (Join-Path $real 'existing.txt'))} 'copy cannot escape through parent junction'
    Check (![IO.File]::Exists((Join-Path $real 'new.txt'))) 'junction target not modified'
    Check ($script:AppChecks -gt 0) 'production mutation path requests closed-app guard'
    Write-Output "$script:checks installer production-function checks PASS; temporary fixtures only, no live Apply/Rollback."
} finally {
    # Remove only the junction objects first, never recursively follow them.
    foreach($path in $script:Junctions){if([IO.Directory]::Exists($path)){[IO.Directory]::Delete($path)}}
    $resolved=[IO.Path]::GetFullPath($script:TestRoot)
    $expectedParent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if(!$resolved.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase) -or
       [IO.Path]::GetFileName($resolved) -notlike 'OpenRGB-InstallerTests-*'){throw 'Refusing unsafe fixture cleanup.'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
