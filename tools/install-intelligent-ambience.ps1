# SPDX-License-Identifier: GPL-2.0-or-later
<#
Stage is the default and never modifies OpenRGB. Apply/Rollback require every
OpenRGB process to be closed. No process is started or stopped by this script.
Backups may contain private settings: keep StageDirectory outside publication.
#>
[CmdletBinding()]
param(
    [ValidateSet('Stage','Apply','Rollback')][string]$Mode = 'Stage',
    [string]$SourceDll,
    [string]$RuntimeRoot,
    [Parameter(Mandatory=$true)][string]$StageDirectory,
    [string]$InstallRoot,
    [string]$ConfigRoot,
    [string]$ProfileRoot,
    [string]$BackupDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Full-Path([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or $Path -notmatch '^(?:[A-Za-z]:[\\/]|\\\\[^\\/]+[\\/][^\\/]+[\\/])') {
        throw 'All supplied filesystem paths must be absolute.'
    }
    $full=[IO.Path]::GetFullPath($Path)
    $root=[IO.Path]::GetPathRoot($full)
    if ($full.TrimEnd('\','/') -eq $root.TrimEnd('\','/')) { throw 'A drive/share root cannot be a deployment directory.' }
    Assert-NoReparse $full
    return $full.TrimEnd('\','/')
}
function Hash-File([string]$Path) {
    Assert-NoReparse $Path
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}
function Write-Json([string]$Path, $Value) {
    Assert-NoReparse $Path
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
}
function Read-Json([string]$Path) {
    Assert-NoReparse $Path
    if (([IO.FileInfo]$Path).Length -gt 1048576) { throw 'Manifest exceeds one MiB.' }
    return [IO.File]::ReadAllText($Path) | ConvertFrom-Json
}
function Assert-X64Dll([string]$Path) {
    Assert-NoReparse $Path
    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 128 -or $reader.ReadUInt16() -ne 0x5a4d) { throw 'Not a PE DLL.' }
        $stream.Position = 0x3c; $pe = $reader.ReadUInt32()
        if ($pe -gt $stream.Length - 24) { throw 'Invalid PE header.' }
        $stream.Position = $pe
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) { throw 'Expected a Windows x64 DLL.' }
        $stream.Position = $pe + 22
        if (($reader.ReadUInt16() -band 0x2000) -eq 0) { throw 'PE image is not a DLL.' }
    } finally { $reader.Dispose(); $stream.Dispose() }
}
function Is-OpenRGBProcessName([string]$Name) { return $Name -ieq 'OpenRGB' }
function Assert-AppClosed {
    $active = @()
    foreach ($process in [Diagnostics.Process]::GetProcesses()) {
        try { if (Is-OpenRGBProcessName $process.ProcessName) { $active += $process.Id } }
        catch [InvalidOperationException] { }
        finally { $process.Dispose() }
    }
    if ($active.Count) { throw "OpenRGB must be closed before this operation (PID: $($active -join ', ')). Nothing was stopped." }
}
function Copy-Verified([string]$From,[string]$To,[string]$Expected) {
    Assert-NoReparse $From
    Assert-NoReparse $To
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($To)) | Out-Null
    [IO.File]::Copy($From,$To,$true)
    if ((Hash-File $To) -ne $Expected) { throw "Hash mismatch after copying $([IO.Path]::GetFileName($To))." }
}
function Install-File([string]$From,[string]$To,[string]$Expected) {
    Assert-AppClosed
    Assert-NoReparse $To
    $temporary = $To + '.intelligence-new-' + [Guid]::NewGuid().ToString('N')
    try {
        Copy-Verified $From $temporary $Expected
        if ([IO.File]::Exists($To)) { [IO.File]::Replace($temporary,$To,[NullString]::Value) }
        else { [IO.File]::Move($temporary,$To) }
    } finally { if ([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) } }
}
function Assert-NoReparse([string]$Path) {
    $current=[IO.Path]::GetFullPath($Path)
    while ($current) {
        $item=Get-Item -LiteralPath $current -Force -ErrorAction SilentlyContinue
        if ($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Reparse points are not supported anywhere in deployment paths.' }
        $parent=[IO.Directory]::GetParent($current)
        $current=if($parent){$parent.FullName}else{$null}
    }
}
function Backup-Tree([string]$From,[string]$To,[string]$Prefix) {
    Assert-NoReparse $From
    foreach ($item in Get-ChildItem -LiteralPath $From -Force) {
        Assert-NoReparse $item.FullName
        $relative = if ($Prefix) { "$Prefix/$($item.Name)" } else { $item.Name }
        if ($item.PSIsContainer) { Backup-Tree $item.FullName (Join-Path $To $item.Name) $relative }
        else {
            $hash = Hash-File $item.FullName
            Copy-Verified $item.FullName (Join-Path $To $item.Name) $hash
            [pscustomobject]@{path=$relative;sha256=$hash;bytes=$item.Length}
        }
    }
}

$relativeFiles = @(
    'plugins/OpenRGBEffectsPlugin.dll',
    'plugins/IntelligentAmbience/onnxruntime.dll',
    'plugins/IntelligentAmbience/onnxruntime_providers_shared.dll',
    'plugins/IntelligentAmbience/LICENSE',
    'plugins/IntelligentAmbience/ThirdPartyNotices.txt'
)
$runtimeHashes = @(
    '7e39e2bdbba836d98071ef28620735ba36a47c554cf794585269aecc50fab0da',
    'b9b7ab9e2a8b08ee7ae4a7ac1c8bfd44a741f17ad8d0f764953140a57de0b796',
    'c250d6278f0b47a6439fb7592b08b58a55eb9f535aa49a1db63211c3f982b674',
    'c53a76501ef60db6f865f20599f220761201ac4057683acefdab37d861b86622'
)
$profileNames = @('IA - Video.json','IA - Musique.json','IA - Hybride.json')
$StageDirectory = Full-Path $StageDirectory

if ($Mode -eq 'Stage') {
    $SourceDll = Full-Path $SourceDll
    $RuntimeRoot = Full-Path $RuntimeRoot
    if ([IO.Directory]::Exists($StageDirectory) -or [IO.File]::Exists($StageDirectory)) { throw 'Choose a new, empty staging directory.' }
    $sources = @($SourceDll,(Join-Path $RuntimeRoot 'lib/onnxruntime.dll'),
        (Join-Path $RuntimeRoot 'lib/onnxruntime_providers_shared.dll'),
        (Join-Path $RuntimeRoot 'LICENSE'),(Join-Path $RuntimeRoot 'ThirdPartyNotices.txt'))
    $files = @()
    $profiles = @()
    for ($i=0; $i -lt $sources.Count; ++$i) {
        $hash = Hash-File $sources[$i]
        if ($i -gt 0 -and $hash -ne $runtimeHashes[$i-1]) { throw 'Runtime files differ from pinned Microsoft ONNX Runtime1.30.0 x64 CPU package.' }
        if ($i -lt 3) { Assert-X64Dll $sources[$i] }
        $files += [pscustomobject]@{path=$relativeFiles[$i];sha256=$hash;bytes=([IO.FileInfo]$sources[$i]).Length}
    }
    if ($ProfileRoot) {
        $ProfileRoot=Full-Path $ProfileRoot
        foreach ($name in $profileNames) {
            $profile=Join-Path $ProfileRoot $name
            $null=Read-Json $profile
            $profiles += [pscustomobject]@{path=('profiles/'+$name);sha256=(Hash-File $profile);bytes=([IO.FileInfo]$profile).Length}
        }
    }
    [IO.Directory]::CreateDirectory($StageDirectory) | Out-Null
    for ($i=0; $i -lt $sources.Count; ++$i) {
        Copy-Verified $sources[$i] (Join-Path $StageDirectory ('payload/'+$relativeFiles[$i])) $files[$i].sha256
    }
    foreach ($profile in $profiles) {
        Copy-Verified (Join-Path $ProfileRoot ([IO.Path]::GetFileName($profile.path))) (Join-Path $StageDirectory ('profile-payload/'+$profile.path)) $profile.sha256
    }
    Write-Json (Join-Path $StageDirectory 'manifest.json') ([ordered]@{
        schema=1;created_utc=[DateTime]::UtcNow.ToString('o');runtime='Microsoft ONNX Runtime1.30.0 CPU x64';
        runtime_archive_sha256='c6ba983baf5681af108599675d2a89c2d145512d02de28aed0bff177cd0ba949';files=$files;profiles=$profiles
    })
    Write-Output "Staged and hash-verified five files. OpenRGB/configuration untouched: $StageDirectory"
    return
}

$InstallRoot = Full-Path $InstallRoot
$ConfigRoot = Full-Path $ConfigRoot
if (-not [IO.File]::Exists((Join-Path $InstallRoot 'OpenRGB.exe'))) { throw 'InstallRoot must contain OpenRGB.exe.' }
if (-not [IO.Directory]::Exists($ConfigRoot)) { throw 'ConfigRoot does not exist.' }
Assert-AppClosed
foreach ($path in @($StageDirectory,$InstallRoot,$ConfigRoot)) { Assert-NoReparse $path }
if ($StageDirectory.StartsWith($InstallRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
    $StageDirectory.StartsWith($ConfigRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
    $StageDirectory -eq $InstallRoot -or $StageDirectory -eq $ConfigRoot) {
    throw 'StageDirectory must be separate from the installation and configuration.'
}

function Restore-Binaries($Deployment,[bool]$CheckCurrent) {
    # CheckCurrent is retained for callers; both automatic/manual recovery now
    # require the same hash-conditional policy, including partially applied work.
    if ($Deployment.install_root -ne $InstallRoot -or $Deployment.config_root -ne $ConfigRoot -or
        $Deployment.files.Count -ne $relativeFiles.Count) { throw 'Backup targets do not match this installation.' }
    $seen = @{}; $plan=@()
    foreach ($file in $Deployment.files) {
        if ($relativeFiles -notcontains $file.path -or $seen.ContainsKey($file.path)) { throw 'Unexpected backup entry.' }
        $seen[$file.path]=$true
        $target=Join-Path $InstallRoot $file.path
        if ($file.existed -and (Hash-File (Join-Path $BackupDirectory ('binaries/'+$file.path))) -ne $file.original_sha256) {
            throw 'Backup hash mismatch.'
        }
        $action=Restore-Disposition $file $InstallRoot
        $plan += [pscustomobject]@{file=$file;root=$InstallRoot;backup='binaries/';action=$action}
    }
    $profileSeen=@{}
    foreach ($profile in $Deployment.profile_files) {
        if ($profileNames -notcontains [IO.Path]::GetFileName($profile.path) -or
            $profile.path -ne ('profiles/'+[IO.Path]::GetFileName($profile.path)) -or $profileSeen.ContainsKey($profile.path)) { throw 'Unexpected profile backup entry.' }
        $profileSeen[$profile.path]=$true
        $target=Join-Path $ConfigRoot $profile.path
        if ($profile.existed -and (Hash-File (Join-Path $BackupDirectory ('profile-binaries/'+$profile.path))) -ne $profile.original_sha256) { throw 'Profile backup hash mismatch.' }
        $action=Restore-Disposition $profile $ConfigRoot
        $plan += [pscustomobject]@{file=$profile;root=$ConfigRoot;backup='profile-binaries/';action=$action}
    }
    foreach ($item in $plan) {
        Assert-AppClosed
        # Recheck immediately before each mutation: do not overwrite newer work.
        $file=$item.file
        if ((Restore-Disposition $file $item.root) -eq 'Skip') { continue }
        $target=Join-Path $item.root $file.path
        if ($file.existed) { Install-File (Join-Path $BackupDirectory ($item.backup+$file.path)) $target $file.original_sha256 }
        elseif ([IO.File]::Exists($target)) { [IO.File]::Delete($target) }
    }
}
function Restore-Disposition($File,[string]$Root) {
    $target=Join-Path $Root $File.path
    Assert-NoReparse $target
    if ([IO.Directory]::Exists($target)) { throw 'A file target became a directory.' }
    $exists=[IO.File]::Exists($target)
    $current=if($exists){Hash-File $target}else{''}
    if (($File.existed -and $exists -and $current -eq $File.original_sha256) -or
        (-not $File.existed -and -not $exists)) { return 'Skip' }
    if ($exists -and $current -eq $File.installed_sha256) { return 'Restore' }
    throw 'A deployment target was changed concurrently; recovery refuses to overwrite it.'
}
function Assert-Original($File,[string]$Root) {
    $target=Join-Path $Root $File.path
    Assert-NoReparse $target
    if ([IO.Directory]::Exists($target)) { throw 'A file target became a directory.' }
    $exists=[IO.File]::Exists($target)
    if (($File.existed -and (-not $exists -or (Hash-File $target) -ne $File.original_sha256)) -or
        (-not $File.existed -and $exists)) { throw 'A target changed after its backup; installation aborted.' }
}
if ($Mode -eq 'Rollback') {
    $BackupDirectory=Full-Path $BackupDirectory
    $deployment=Read-Json (Join-Path $BackupDirectory 'deployment.json')
    Restore-Binaries $deployment $true
    $deployment.state='rolled-back';Write-Json (Join-Path $BackupDirectory 'deployment.json') $deployment
    Write-Output 'Rollback complete for deployed binaries and the three optional new profiles. Other current settings/profiles were not changed.'
    return
}

$manifest=Read-Json (Join-Path $StageDirectory 'manifest.json')
if ($manifest.schema -ne 1 -or $manifest.files.Count -ne $relativeFiles.Count) { throw 'Invalid staging manifest.' }
$seen=@{}
foreach ($file in $manifest.files) {
    if ($relativeFiles -notcontains $file.path -or $seen.ContainsKey($file.path)) { throw 'Unexpected staging entry.' }
    $seen[$file.path]=$true
    $payload=Join-Path $StageDirectory ('payload/'+$file.path)
    if ((Hash-File $payload) -ne $file.sha256 -or ([IO.FileInfo]$payload).Length -ne $file.bytes) { throw 'Staged payload changed.' }
    $index=[Array]::IndexOf($relativeFiles,$file.path)
    if ($index -gt 0 -and $file.sha256 -ne $runtimeHashes[$index-1]) { throw 'Runtime pin mismatch.' }
    if ($index -lt 3) { Assert-X64Dll $payload }
}
$seenProfiles=@{}
foreach ($profile in $manifest.profiles) {
    if ($profileNames -notcontains [IO.Path]::GetFileName($profile.path) -or
        $profile.path -ne ('profiles/'+[IO.Path]::GetFileName($profile.path)) -or $seenProfiles.ContainsKey($profile.path)) { throw 'Unexpected staged profile.' }
    $seenProfiles[$profile.path]=$true
    $payload=Join-Path $StageDirectory ('profile-payload/'+$profile.path)
    $null=Read-Json $payload
    if ((Hash-File $payload) -ne $profile.sha256 -or ([IO.FileInfo]$payload).Length -ne $profile.bytes) { throw 'Staged profile changed.' }
}
$BackupDirectory=Join-Path $StageDirectory ('backup-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($BackupDirectory) | Out-Null
$backupFiles=@()
$backupProfiles=@()
foreach ($file in $manifest.files) {
    $target=Join-Path $InstallRoot $file.path
    $existed=[IO.File]::Exists($target);$original=''
    if ($existed) { Assert-NoReparse $target;$original=Hash-File $target;Copy-Verified $target (Join-Path $BackupDirectory ('binaries/'+$file.path)) $original }
    $backupFiles += [pscustomobject]@{path=$file.path;existed=$existed;original_sha256=$original;installed_sha256=$file.sha256}
}
foreach ($profile in $manifest.profiles) {
    $target=Join-Path $ConfigRoot $profile.path
    $existed=[IO.File]::Exists($target);$original=''
    if ($existed) { Assert-NoReparse $target;$original=Hash-File $target;Copy-Verified $target (Join-Path $BackupDirectory ('profile-binaries/'+$profile.path)) $original }
    $backupProfiles += [pscustomobject]@{path=$profile.path;existed=$existed;original_sha256=$original;installed_sha256=$profile.sha256}
}
$configuration=@()
foreach ($name in @('OpenRGB.json','Configuration.json','last-session.json')) {
    $path=Join-Path $ConfigRoot $name
    if ([IO.File]::Exists($path)) { Assert-NoReparse $path;$hash=Hash-File $path;Copy-Verified $path (Join-Path $BackupDirectory ('config/'+$name)) $hash;$configuration += [pscustomobject]@{path=$name;sha256=$hash;bytes=([IO.FileInfo]$path).Length} }
}
foreach ($name in @('plugins/settings','profiles')) {
    $path=Join-Path $ConfigRoot $name
    if ([IO.Directory]::Exists($path)) { $configuration += @(Backup-Tree $path (Join-Path $BackupDirectory ('config/'+$name)) $name) }
}
$deployment=[pscustomobject]@{schema=1;state='backed-up';created_utc=[DateTime]::UtcNow.ToString('o');install_root=$InstallRoot;config_root=$ConfigRoot;files=$backupFiles;profile_files=$backupProfiles;config_files=$configuration}
Write-Json (Join-Path $BackupDirectory 'deployment.json') $deployment
try {
    foreach ($file in $manifest.files) {
        $original=$backupFiles | Where-Object { $_.path -eq $file.path }
        Assert-Original $original $InstallRoot
        Install-File (Join-Path $StageDirectory ('payload/'+$file.path)) (Join-Path $InstallRoot $file.path) $file.sha256
    }
    foreach ($profile in $manifest.profiles) {
        $original=$backupProfiles | Where-Object { $_.path -eq $profile.path }
        Assert-Original $original $ConfigRoot
        Install-File (Join-Path $StageDirectory ('profile-payload/'+$profile.path)) (Join-Path $ConfigRoot $profile.path) $profile.sha256
    }
    $deployment.state='applied';Write-Json (Join-Path $BackupDirectory 'deployment.json') $deployment
    Write-Output "Applied five verified files and $($manifest.profiles.Count) optional profiles. No profile was activated; last-session/settings unchanged. Backup: $BackupDirectory"
} catch {
    $failure=$_.Exception.Message
    try { Restore-Binaries $deployment $false;$deployment.state='failed-rolled-back' }
    catch { $deployment.state='failed-rollback-incomplete';$failure += ' Rollback also failed: '+$_.Exception.Message }
    Write-Json (Join-Path $BackupDirectory 'deployment.json') $deployment
    throw "$failure Backup/journal: $BackupDirectory"
}
