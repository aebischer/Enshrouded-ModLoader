param(
    [Parameter(Mandatory=$true)][ValidateSet('Export','Import')][string]$Action,
    [Parameter(Mandatory=$true)][string]$GamePath,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{32}$')][string]$WorldId,
    [Parameter(Mandatory=$true)][string]$Path
)
$ErrorActionPreference = 'Stop'
if ($WorldId -eq ('0' * 32)) { throw 'Invalid GUID.' }
$GamePath = (Resolve-Path -LiteralPath $GamePath).Path
foreach ($running in @(Get-Process enshrouded -ErrorAction SilentlyContinue)) {
    if (-not $running.Path -or $running.Path -eq (Join-Path $GamePath 'enshrouded.exe')) { throw 'Close the game before transferring a profile.' }
}
$utf8 = New-Object Text.UTF8Encoding($false, $true)
$profile = Join-Path $GamePath ('Mods\EnshroudedModLoader\worlds\' + $WorldId + '.mods')
function Validate-Profile([byte[]]$bytes) {
    if ($bytes.Length -gt 1049664) { throw 'Profile is too large.' }
    $text = $utf8.GetString($bytes).TrimStart([char]0xFEFF)
    $lines = $text -split "`n"
    if ($lines[0].TrimEnd([char]13) -ne 'ENSHROUDED_MODS_V1') { throw 'Invalid header.' }
    $seen = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($line in $lines | Select-Object -Skip 1) {
        $entry = $line.TrimEnd([char]13)
        if (-not $entry) { continue }
        if ($utf8.GetByteCount($entry) -gt 1024 -or $entry -match '[\x00-\x1f:<>"|?*]' -or $entry -notmatch '\.dll$' -or [IO.Path]::IsPathRooted($entry)) { throw 'Invalid path.' }
        foreach ($part in ($entry -split '[\\/]')) {
            if (-not $part -or $part -in @('.', '..') -or $part.EndsWith('.') -or $part.EndsWith(' ')) { throw 'Invalid path component.' }
        }
        if (-not $seen.Add($entry.Replace('\','/')) -or $seen.Count -gt 1024) { throw 'Duplicate entries or too many mods.' }
    }
}
function Read-Bounded([string]$file, [int]$limit) {
    $stream = [IO.File]::Open($file, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        if ($stream.Length -gt $limit) { throw 'File is too large.' }
        $bytes = New-Object byte[] ([int]$stream.Length)
        $offset = 0
        while ($offset -lt $bytes.Length) {
            $read = $stream.Read($bytes, $offset, $bytes.Length - $offset)
            if (-not $read) { throw 'Read interrupted.' }
            $offset += $read
        }
        return ,$bytes
    } finally { $stream.Dispose() }
}
function Digest([byte[]]$bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','') } finally { $sha.Dispose() }
}
if ($Action -eq 'Export') {
    $bytes = Read-Bounded $profile 1049664
    Validate-Profile $bytes
    $bundle = @{ format=1; world=$WorldId; sha256=(Digest $bytes); profile=[Convert]::ToBase64String($bytes) } | ConvertTo-Json
    $destination = [IO.Path]::GetFullPath($Path)
    if (Test-Path -LiteralPath $destination) { throw 'Destination already exists.' }
    [IO.File]::WriteAllText($destination,$bundle,$utf8)
    Write-Output 'Profile exported. Also transfer the game save, settings and matching mod versions.'
} else {
    $bundle = $utf8.GetString((Read-Bounded ([IO.Path]::GetFullPath($Path)) 1500000)) | ConvertFrom-Json
    if ($bundle.format -ne 1 -or $bundle.world -cne $WorldId) { throw 'Package does not match this world.' }
    $bytes = [Convert]::FromBase64String($bundle.profile)
    if ((Digest $bytes) -cne $bundle.sha256) { throw 'Profile integrity check failed.' }
    Validate-Profile $bytes
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($profile)) | Out-Null
    $lock = [IO.File]::Open(($profile+'.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $temp = $profile+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
    try {
        [IO.File]::WriteAllBytes($temp,$bytes)
        if (Test-Path -LiteralPath $profile) {
            [IO.File]::Replace($temp,$profile,($profile+'.before-import'))
        } else { [IO.File]::Move($temp,$profile) }
    } finally {
        $lock.Dispose()
        if (Test-Path -LiteralPath $temp -PathType Leaf) { Remove-Item -LiteralPath $temp }
    }
    Write-Output 'Profile imported; the previous configuration was preserved if present.'
}
