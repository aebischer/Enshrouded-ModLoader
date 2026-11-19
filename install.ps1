param([string]$GamePath, [switch]$Uninstall)
$ErrorActionPreference = 'Stop'
$expected = '5D9EF2F140B8ED49E30DBEE655841B89D08BAEC20F799A1817E171A18A850C77'
if (-not $GamePath) {
    $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    if (-not $steam) { $steam = Join-Path ${env:ProgramFiles(x86)} 'Steam' }
    $roots = @($steam)
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path -LiteralPath $vdf) {
        $roots += [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"') | ForEach-Object { $_.Groups[1].Value.Replace('\\','\') }
    }
    $GamePath = $roots | Select-Object -Unique | ForEach-Object { Join-Path $_ 'steamapps\common\Enshrouded' } | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'enshrouded.exe') } | Select-Object -First 1
    if (-not $GamePath) { $GamePath = Read-Host 'Folder containing enshrouded.exe' }
}
$GamePath = (Resolve-Path -LiteralPath $GamePath).Path
$exe = Join-Path $GamePath 'enshrouded.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Enshrouded.exe was not found.' }
foreach ($running in @(Get-Process enshrouded -ErrorAction SilentlyContinue)) {
    if (-not $running.Path -or $running.Path -eq $exe) { throw 'Close Enshrouded before continuing.' }
}
$dll = Join-Path $GamePath 'dinput8.dll'
$data = Join-Path $GamePath 'RandomWorld'
$receipt = Join-Path $data 'installed.sha256'
New-Item -ItemType Directory -Path $data -Force | Out-Null
$installLock = [IO.File]::Open((Join-Path $data 'install.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$pending = Join-Path $data 'install.pending.json'
$previous = Join-Path $data 'install.previous.dll'
# Move only identified legacy packages. Relative package IDs and world settings stay unchanged.
$modsRoot = Join-Path $GamePath 'Mods'
$packageRoot = Join-Path $modsRoot 'EnshroudedModLoader'
function Assert-PlainPath([string]$path) {
    $full = [IO.Path]::GetFullPath($path)
    $boundary = [IO.Path]::GetFullPath($GamePath).TrimEnd('\') + '\'
    if (-not $full.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)) { throw "Path outside game folder: $path" }
    $cursor = $full
    while ($cursor.Length -ge $boundary.Length) {
        if (Test-Path -LiteralPath $cursor) {
            if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked package path refused: $cursor" }
        }
        $cursor = Split-Path $cursor -Parent
    }
}
function Test-PackageHeader([string]$manifest) {
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { return $false }
    Assert-PlainPath $manifest
    if ((Get-Item -LiteralPath $manifest).Length -gt 16384) { return $false }
    $reader = [IO.StreamReader]::new($manifest, [Text.Encoding]::UTF8, $true)
    try { return $reader.ReadLine() -ceq 'ENSHROUDED_PACKAGE_V1' } finally { $reader.Dispose() }
}
function Get-LegacyMoves {
    Assert-PlainPath $modsRoot
    Assert-PlainPath $packageRoot
    if (-not (Test-Path -LiteralPath $modsRoot)) { return }
    foreach ($entry in Get-ChildItem -LiteralPath $modsRoot -Force) {
        if ($entry.Name -ieq 'EnshroudedModLoader') { continue }
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
        if (-not $entry.PSIsContainer) {
            if ($entry.Extension -ieq '.dll' -and (Test-PackageHeader ($entry.FullName + '.mod'))) {
                $entry.Name
                $entry.Name + '.mod'
            }
            continue
        }
        $queue = [Collections.Generic.Queue[string]]::new()
        $queue.Enqueue($entry.FullName)
        $ours = $false; $foreign = $false; $linked = $false; $count = 0
        while ($queue.Count) {
            foreach ($child in Get-ChildItem -LiteralPath $queue.Dequeue() -Force) {
                if (++$count -gt 100000) { throw "Legacy scan limit: $($entry.FullName). Separate our packages before installing." }
                if ($child.Attributes -band [IO.FileAttributes]::ReparsePoint) { $linked=$true; continue }
                if ($child.PSIsContainer) { $queue.Enqueue($child.FullName); continue }
                if ($child.Extension -ieq '.dll') {
                    if (Test-PackageHeader ($child.FullName + '.mod')) { $ours=$true } else { $foreign=$true }
                }
            }
        }
        if ($ours) {
            if ($foreign -or $linked) { throw "Mixed or linked legacy package folder: $($entry.FullName). Nothing was moved; separate our packages first." }
            $entry.Name
        }
    }
}
function Get-MovePaths([string]$name) {
    if (-not $name -or $name -in @('.', '..', 'EnshroudedModLoader') -or $name.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) { throw 'Invalid package recovery path.' }
    $from = Join-Path $modsRoot $name; $to = Join-Path $packageRoot $name
    Assert-PlainPath $from; Assert-PlainPath $to
    return @($from, $to)
}
function Restore-PackageMoves($record) {
    $moves = @($record.packageMoves)
    [array]::Reverse($moves)
    foreach ($name in $moves) {
        if (-not $name) { continue }
        $paths = Get-MovePaths $name
        if (Test-Path -LiteralPath $paths[1]) {
            if (Test-Path -LiteralPath $paths[0]) { throw "Package recovery conflict: $name. No files overwritten." }
            Move-Item -LiteralPath $paths[1] -Destination $paths[0]
        } elseif (-not (Test-Path -LiteralPath $paths[0])) { throw "Package recovery file missing: $name" }
    }
}
function Restore-PendingInstall {
    if (-not (Test-Path -LiteralPath $pending)) { return }
    $record = Get-Content -LiteralPath $pending -Raw | ConvertFrom-Json
    if ($record.newHash -notmatch '^[A-F0-9]{64}$' -or ($record.oldHash -and $record.oldHash -notmatch '^[A-F0-9]{64}$')) { throw 'Invalid recovery journal.' }
    $current = if (Test-Path -LiteralPath $dll -PathType Leaf) { (Get-FileHash -LiteralPath $dll).Hash } else { '' }
    if ($current -and $current -ne $record.newHash -and $current -ne $record.oldHash) { throw 'The DLL changed during an interrupted installation. Recovery refused.' }
    if ($record.oldHash) {
        if ($current -ne $record.oldHash) {
            if (-not (Test-Path -LiteralPath $previous -PathType Leaf) -or (Get-FileHash -LiteralPath $previous).Hash -ne $record.oldHash) { throw 'Recovery copy is missing or altered.' }
            Copy-Item -LiteralPath $previous -Destination $dll -Force
        }
        # If the old receipt is already correct (possibly locked), no rewrite is needed.
        if (-not (Test-Path -LiteralPath $receipt -PathType Leaf) -or (Get-Content -LiteralPath $receipt -Raw).Trim() -ne $record.oldHash) {
            [IO.File]::WriteAllText($receipt, $record.oldHash, [Text.Encoding]::ASCII)
        }
    } else {
        if ($current) { Remove-Item -LiteralPath $dll }
        if (Test-Path -LiteralPath $receipt -PathType Leaf) { Remove-Item -LiteralPath $receipt }
    }
    Restore-PackageMoves $record
    Remove-Item -LiteralPath $pending
}
function Assert-LoaderBinary([byte[]]$bytes) {
    function Need([long]$offset,[long]$count) {
        if($offset -lt 0 -or $count -lt 0 -or $offset -gt $bytes.Length -or $count -gt $bytes.Length-$offset) {throw 'Truncated DLL or invalid PE table.'}
    }
    function U16([long]$offset) {Need $offset 2; return [BitConverter]::ToUInt16($bytes,[int]$offset)}
    function U32([long]$offset) {Need $offset 4; return [BitConverter]::ToUInt32($bytes,[int]$offset)}
    Need 0 64
    if((U16 0) -ne 0x5a4d){throw 'Invalid source DLL.'}
    $pe=[long](U32 60)
    Need $pe 264
    if((U32 $pe) -ne 0x4550 -or (U16 ($pe+4)) -ne 0x8664 -or ((U16 ($pe+22)) -band 0x2000) -eq 0 -or (U16 ($pe+24)) -ne 0x20b -or (U16 ($pe+20)) -ne 240){throw 'The loader must be an x64 PE32+ DLL.'}
    $count=U16 ($pe+6)
    if($count -lt 1 -or $count -gt 96 -or (U32 ($pe+132)) -lt 16){throw 'Invalid PE tables.'}
    $sections=@()
    for($i=0;$i -lt $count;$i++) {
        $o=$pe+264+40*$i; Need $o 40
        $sections+=,@{rva=[long](U32 ($o+12)); virtual=[long](U32 ($o+8)); size=[long](U32 ($o+16)); raw=[long](U32 ($o+20)); flags=(U32 ($o+36))}
        Need $sections[-1].raw $sections[-1].size
    }
    function Offset([long]$rva,[long]$length) {
        foreach($section in $sections) {
            $delta=$rva-$section.rva
            if($delta -ge 0 -and $delta -lt $section.size -and $length -le $section.size-$delta) {Need ($section.raw+$delta) $length; return ($section.raw+$delta)}
        }
        throw 'Invalid PE RVA.'
    }
    $exports=[long](U32 ($pe+136)); $exportSize=[long](U32 ($pe+140))
    if($exports -eq 0 -or $exportSize -lt 40){throw 'Loader exports are missing.'}
    $e=Offset $exports 40
    $functions=[long](U32 ($e+20)); $names=[long](U32 ($e+24))
    if($functions -lt 6 -or $functions -gt 65536 -or $names -lt 6 -or $names -gt 65536){throw 'Invalid loader exports.'}
    $f=Offset (U32 ($e+28)) ($functions*4)
    $n=Offset (U32 ($e+32)) ($names*4)
    $ord=Offset (U32 ($e+36)) ($names*2)
    $required=@('DirectInput8Create','DllCanUnloadNow','DllGetClassObject','DllRegisterServer','DllUnregisterServer','GetdfDIJoystick')
    $found=@{}
    for($i=0;$i -lt $names;$i++) {
        $nameRva=[long](U32 ($n+4*$i)); $name=''; $ended=$false
        for($j=0;$j -lt 256;$j++) {
            $c=$bytes[(Offset ($nameRva+$j) 1)]
            if($c -eq 0){$ended=$true;break}
            if($c -lt 32 -or $c -gt 126){throw 'Invalid export name.'}
            $name += [char]$c
        }
        if(-not $ended){throw 'Export name is too long.'}
        if($required -ccontains $name) {
            if($found.ContainsKey($name)){throw 'Duplicate export.'}
            $index=U16 ($ord+2*$i)
            if($index -ge $functions){throw 'Invalid ordinal.'}
            $address=[long](U32 ($f+4*$index))
            if($address -ge $exports -and $address -lt $exports+$exportSize){throw 'Forwarded exports are not allowed for the loader.'}
            $null=Offset $address 1
            $executable=$false
            foreach($section in $sections){if($address -ge $section.rva -and $address -lt $section.rva+$section.virtual -and ($section.flags -band 0x20000000)){$executable=$true}}
            if(-not $executable){throw 'Export is not executable.'}
            $found[$name]=$true
        }
    }
    foreach($name in $required){if(-not $found.ContainsKey($name)){throw "Missing loader export: $name"}}
}
try {
Restore-PendingInstall
if ($Uninstall) {
    if (-not (Test-Path -LiteralPath $receipt)) { throw 'No recognized installation receipt was found.' }
    if (Test-Path -LiteralPath $dll) {
        if ((Get-FileHash -LiteralPath $dll).Hash -ne (Get-Content -LiteralPath $receipt -Raw).Trim()) { throw 'The DLL has changed: automatic removal refused.' }
        Remove-Item -LiteralPath $dll
    }
    Remove-Item -LiteralPath $receipt
    Write-Host 'Loader removed. Logs and saved data preserved.'
    exit
}
if ((Get-FileHash -LiteralPath $exe).Hash -ne $expected) { throw 'This game version is not supported by this build.' }
if (Test-Path -LiteralPath $dll) {
    if (-not (Test-Path -LiteralPath $receipt)) { throw 'Another dinput8.dll is present. No file was replaced.' }
    if ((Get-FileHash -LiteralPath $dll).Hash -ne (Get-Content -LiteralPath $receipt -Raw).Trim()) { throw 'Conflict: DLL differs from the managed installation.' }
}
foreach ($other in @('powrprof.dll','dbghelp.dll')) {
    if (Test-Path -LiteralPath (Join-Path $GamePath $other)) { throw "Another loader is present ($other). Integration must be checked." }
}
$source = Join-Path $PSScriptRoot 'dist\dinput8.dll'
if (-not (Test-Path -LiteralPath $source)) { throw 'Source DLL missing. Build the project or extract the complete release ZIP.' }
if (Test-Path -LiteralPath $receipt -PathType Container) { throw 'The receipt path is a directory. Installation was not changed.' }
$packageMoves = @(Get-LegacyMoves)
foreach ($name in $packageMoves) {
    $paths = Get-MovePaths $name
    if (Test-Path -LiteralPath $paths[1]) { throw "Package destination already exists: $($paths[1]). Nothing was replaced." }
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
$transactionId = [guid]::NewGuid().ToString('N')
$stagedDll = Join-Path $GamePath ('dinput8.' + $transactionId + '.new')
$stagedReceipt = Join-Path $data ('receipt.' + $transactionId + '.new')
$stagedPending = Join-Path $data ('pending.' + $transactionId + '.new')
try {
    Copy-Item -LiteralPath $source -Destination $stagedDll
    $newHash = (Get-FileHash -LiteralPath $stagedDll).Hash
    if ($newHash -ne (Get-FileHash -LiteralPath $source).Hash) { throw 'Copied file verification failed.' }
    if((Get-Item -LiteralPath $stagedDll).Length -gt 128MB){throw 'Source DLL is too large.'}
    Assert-LoaderBinary ([IO.File]::ReadAllBytes($stagedDll))
    [IO.File]::WriteAllText($stagedReceipt, $newHash, [Text.Encoding]::ASCII)
    $oldHash = if (Test-Path -LiteralPath $dll) { (Get-FileHash -LiteralPath $dll).Hash } else { '' }
    if ($oldHash) {
        Copy-Item -LiteralPath $dll -Destination $previous -Force
        if ((Get-FileHash -LiteralPath $previous).Hash -ne $oldHash) { throw 'Invalid recovery backup.' }
    }
    @{newHash=$newHash; oldHash=$oldHash; packageMoves=$packageMoves} | ConvertTo-Json -Compress | Set-Content -LiteralPath $stagedPending -Encoding UTF8
    [IO.File]::Move($stagedPending, $pending)
    foreach ($name in $packageMoves) {
        $paths = Get-MovePaths $name
        Move-Item -LiteralPath $paths[0] -Destination $paths[1]
    }
    if ($oldHash) { [IO.File]::Replace($stagedDll, $dll, ($stagedDll + '.old')) } else { [IO.File]::Move($stagedDll, $dll) }
    if (Test-Path -LiteralPath $receipt) { [IO.File]::Replace($stagedReceipt, $receipt, ($stagedReceipt + '.old')) } else { [IO.File]::Move($stagedReceipt, $receipt) }
    if ((Get-FileHash -LiteralPath $dll).Hash -ne $newHash -or (Get-Content -LiteralPath $receipt -Raw).Trim() -ne $newHash) { throw 'Final verification failed.' }
    Remove-Item -LiteralPath $pending
} catch {
    $installError = $_
    Restore-PendingInstall
    throw $installError
} finally {
    foreach ($tempFile in @($stagedDll, $stagedReceipt, $stagedPending, ($stagedDll + '.old'), ($stagedReceipt + '.old'))) {
        if (Test-Path -LiteralPath $tempFile -PathType Leaf) { Remove-Item -LiteralPath $tempFile -ErrorAction SilentlyContinue }
    }
}
Write-Host "ModLoader 0.6.12 installed and verified in $GamePath."
} finally { $installLock.Dispose() }
