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
$dll = Join-Path $GamePath 'dinput8.dll'
$data = Join-Path $GamePath 'RandomWorld'
$receipt = Join-Path $data 'installed.sha256'
Assert-PlainPath $dll
Assert-PlainPath $data
New-Item -ItemType Directory -Path $data -Force | Out-Null
$installLock = [IO.File]::Open((Join-Path $data 'install.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$pending = Join-Path $data 'install.pending.json'
$previous = Join-Path $data 'install.previous.dll'
# Move only identified legacy packages. Relative package IDs and world settings stay unchanged.
$modsRoot = Join-Path $GamePath 'Mods'
$packageRoot = Join-Path $modsRoot 'EnshroudedModLoader'
# Copy/verify first; retire legacy files only after a successful DLL installation.
# Restarting any interrupted phase is idempotent. Different destination bytes
# are always a conflict, never an invitation to overwrite player settings.
$profileOld = Join-Path $GamePath 'ModLoader/worlds'
$profileNew = Join-Path $packageRoot 'worlds'
$profileFiles = @()
function Get-ProfileFiles([string]$directory) {
    Assert-PlainPath $directory
    if (-not (Test-Path -LiteralPath $directory)) { return }
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) { throw 'Profile directory is a file' }
    foreach($entry in Get-ChildItem -LiteralPath $directory -Force) {
        Assert-PlainPath $entry.FullName
        if($entry.PSIsContainer){ Get-ProfileFiles $entry.FullName }
        else { $entry.FullName }
    }
}
function Prepare-ProfileMigration {
    Assert-PlainPath $profileOld; Assert-PlainPath $profileNew
    $script:profileFiles=@()
    foreach($file in @(Get-ProfileFiles $profileOld)) {
        $relative=$file.Substring($profileOld.Length).TrimStart('\','/')
        $destination=Join-Path $profileNew $relative
        Assert-PlainPath $destination
        $hash=(Get-FileHash -LiteralPath $file).Hash
        if(Test-Path -LiteralPath $destination){
            if(-not (Test-Path -LiteralPath $destination -PathType Leaf) -or (Get-FileHash -LiteralPath $destination).Hash -ne $hash){throw "Profile migration conflict; both copies preserved: $relative"}
        }
        $script:profileFiles+=@{source=$file;destination=$destination;relative=$relative;hash=$hash}
    }
    if(!$profileFiles.Count){return}
    $backup=Join-Path $data ('worlds-backup.'+[guid]::NewGuid().ToString('N'))
    Assert-PlainPath $backup
    foreach($entry in $profileFiles){
        $copy=Join-Path $backup $entry.relative
        New-Item -ItemType Directory -Path (Split-Path $copy -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $entry.source -Destination $copy
        if((Get-FileHash -LiteralPath $copy).Hash -ne $entry.hash){throw 'Profile backup hash mismatch'}
    }
    foreach($entry in $profileFiles){
        if((Get-FileHash -LiteralPath $entry.source).Hash -ne $entry.hash){throw 'Profile changed during migration'}
        if(Test-Path -LiteralPath $entry.destination){continue}
        New-Item -ItemType Directory -Path (Split-Path $entry.destination -Parent) -Force | Out-Null
        $temp=$entry.destination+'.migration.'+[guid]::NewGuid().ToString('N')
        try {
            Copy-Item -LiteralPath $entry.source -Destination $temp
            if((Get-FileHash -LiteralPath $temp).Hash -ne $entry.hash){throw 'Profile copy hash mismatch'}
            [IO.File]::Move($temp,$entry.destination)
        } finally {if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp}}
    }
    Write-Output "Profile backup verified: $backup"
}
function Complete-ProfileMigration {
    # Verify the complete source set again before deleting any original file.
    $current=@(Get-ProfileFiles $profileOld)
    if($current.Count -ne $profileFiles.Count){throw 'Profile source set changed; originals retained'}
    foreach($entry in $profileFiles){
        Assert-PlainPath $entry.source;Assert-PlainPath $entry.destination
        if((Get-FileHash -LiteralPath $entry.source).Hash -ne $entry.hash -or (Get-FileHash -LiteralPath $entry.destination).Hash -ne $entry.hash){throw 'Profile verification failed; originals retained'}
    }
    foreach($entry in $profileFiles){Remove-Item -LiteralPath $entry.source}
    # Empty directories are harmless; never recursively delete player data.
    Write-Output 'World profiles/settings now use Mods/EnshroudedModLoader/worlds.'
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
    $record = Get-Content -LiteralPath $pending -Raw -Encoding UTF8 | ConvertFrom-Json
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
function Assert-LoaderBinary([byte[]]$bytes, [string[]]$required=@('DirectInput8Create','DllCanUnloadNow','DllGetClassObject','DllRegisterServer','DllUnregisterServer','GetdfDIJoystick')) {
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
    if($functions -lt $required.Count -or $functions -gt 65536 -or $names -lt $required.Count -or $names -gt 65536){throw 'Invalid loader exports.'}
    $f=Offset (U32 ($e+28)) ($functions*4)
    $n=Offset (U32 ($e+32)) ($names*4)
    $ord=Offset (U32 ($e+36)) ($names*2)
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
# Called only after all shared helpers are defined and while install.lock is held.
function Get-ManagedBytesHash([byte[]]$bytes) {
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','') } finally {$sha.Dispose()}
}
function Get-ManagedPath([string]$relative) {
    if($relative -notmatch '^(dinput8\.dll|RandomWorld/(installed\.sha256|kiln-installed\.json)|kiln/native/enshrouded-modloader/(modloader\.dll|native\.json)|kiln/chain/[^/\\:]+\.dll|kiln/chain\.ini|dinput8-chain\.ini)$') {throw "Invalid managed path: $relative"}
    $path=Join-Path $GamePath $relative
    Assert-PlainPath $path
    return $path
}
function Read-ManagedFile([string]$relative) {
    $path=Get-ManagedPath $relative
    if(Test-Path -LiteralPath $path) {
        if(-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -gt 128MB){throw "Invalid managed file: $path"}
        return ,([IO.File]::ReadAllBytes($path))
    }
    return $null
}
function Write-ManagedFile([string]$relative,$bytes) {
    $path=Get-ManagedPath $relative
    if($null -eq $bytes) {if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path};return}
    New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
    $temp=$path+'.'+[guid]::NewGuid().ToString('N')+'.new'
    try {
        [IO.File]::WriteAllBytes($temp,[byte[]]$bytes)
        if(Test-Path -LiteralPath $path){[IO.File]::Replace($temp,$path,$temp+'.old')}else{[IO.File]::Move($temp,$path)}
    } finally {foreach($p in @($temp,($temp+'.old'))){if(Test-Path -LiteralPath $p){Remove-Item -LiteralPath $p}}}
}
function Managed-Hash($bytes) {if($null -eq $bytes){return ''};return Get-ManagedBytesHash $bytes}
function Restore-DualInstall {
    $journal=Join-Path $data 'dual-install.pending.json'
    Assert-PlainPath $journal
    if(-not (Test-Path -LiteralPath $journal)){return}
    if((Get-Item -LiteralPath $journal).Length -gt 256MB){throw 'Recovery journal too large'}
    $record=Get-Content -LiteralPath $journal -Raw -Encoding UTF8 | ConvertFrom-Json
    if($record.format -ne 1 -or @($record.files).Count -gt 128){throw 'Invalid dual installation journal'}
    $seen=@{}
    # Validate the entire journal before restoring anything. Never accept a foreign change.
    foreach($entry in $record.files) {
        $null=Get-ManagedPath $entry.path
        if($seen.ContainsKey($entry.path)){throw 'Duplicate recovery path'};$seen[$entry.path]=$true
        $before=if($null -eq $entry.before){$null}else{[Convert]::FromBase64String($entry.before)}
        if((Managed-Hash $before) -ne $entry.beforeHash -or ($entry.afterHash -and $entry.afterHash -notmatch '^[A-F0-9]{64}$')){throw 'Invalid recovery checksum'}
        $current=Managed-Hash (Read-ManagedFile $entry.path)
        if($current -ne $entry.beforeHash -and $current -ne $entry.afterHash){throw "File changed outside installer: $($entry.path)"}
    }
    foreach($name in @($record.packageMoves)) {
        if(-not $name){continue};$paths=Get-MovePaths $name
        if((Test-Path -LiteralPath $paths[0]) -and (Test-Path -LiteralPath $paths[1])){throw 'Recovery package collision'}
        if(!(Test-Path -LiteralPath $paths[0]) -and !(Test-Path -LiteralPath $paths[1])){throw 'Recovery package missing'}
    }
    $reverse=@($record.files);[array]::Reverse($reverse)
    foreach($entry in $reverse) {
        if((Managed-Hash (Read-ManagedFile $entry.path)) -eq $entry.beforeHash){continue}
        $before=if($null -eq $entry.before){$null}else{[Convert]::FromBase64String($entry.before)}
        Write-ManagedFile $entry.path $before
    }
    Restore-PackageMoves $record
    Remove-Item -LiteralPath $journal
}
function Install-DualMode([bool]$kiln) {
    $nativeReceipt='RandomWorld/kiln-installed.json'
    $nativeDll='kiln/native/enshrouded-modloader/modloader.dll'
    $nativeManifest='kiln/native/enshrouded-modloader/native.json'
    $oldNative=Read-ManagedFile $nativeReceipt
    $nativeRecord=if($null -ne $oldNative){[Text.Encoding]::UTF8.GetString($oldNative) | ConvertFrom-Json}else{$null}
    foreach($entry in @(@($nativeDll,'dll'),@($nativeManifest,'manifest'))) {
        $bytes=Read-ManagedFile $entry[0]
        if($null -ne $bytes -and (!$nativeRecord -or (Managed-Hash $bytes) -ne $nativeRecord.($entry[1]))){throw "Unmanaged or modified file: $($entry[0])"}
    }
    $ownedHash=if(Test-Path -LiteralPath $receipt -PathType Leaf){(Get-Content -LiteralPath $receipt -Raw).Trim()}else{''}
    if($ownedHash -and $ownedHash -notmatch '^[A-F0-9]{64}$'){throw 'Invalid standalone receipt'}
    $changes=[ordered]@{}
    $packageMoves=@()
    if(-not $Uninstall) {
        $packageMoves=@(Get-LegacyMoves)
        foreach($name in $packageMoves){$paths=Get-MovePaths $name;if(Test-Path -LiteralPath $paths[1]){throw 'Package destination collision'}}
    }
    if(-not $Uninstall) {
        if((Get-FileHash -LiteralPath $exe).Hash -ne $expected){throw 'Unsupported executable'}
        $source=Join-Path $PSScriptRoot 'dist/dinput8.dll'
        if((Get-Item -LiteralPath $source).Length -gt 128MB){throw 'Source too large'}
        $bytes=[IO.File]::ReadAllBytes($source)
        Assert-LoaderBinary $bytes
        Assert-LoaderBinary $bytes @('kiln_native_init')
        if($kiln) {
            $manifest=[Text.Encoding]::UTF8.GetBytes('{"format":1,"id":"enshrouded-modloader","version":"0.6.15","name":"Enshrouded ModLoader","side":"client","dlls":["modloader.dll"],"phase":"boot","init":"kiln_native_init","enabled":true,"kiln":">=0.2.0-dev","builds":["'+$expected+'"]}')
            $changes[$nativeDll]=$bytes;$changes[$nativeManifest]=$manifest
            $changes[$nativeReceipt]=[Text.Encoding]::UTF8.GetBytes((@{dll=(Managed-Hash $bytes);manifest=(Managed-Hash $manifest)} | ConvertTo-Json -Compress))
        } else {
            $current=Read-ManagedFile 'dinput8.dll'
            if($null -ne $current -and (!$ownedHash -or (Managed-Hash $current) -ne $ownedHash)){throw 'Foreign root proxy; nothing replaced'}
            $changes['dinput8.dll']=$bytes
            $changes['RandomWorld/installed.sha256']=[Text.Encoding]::ASCII.GetBytes((Managed-Hash $bytes))
        }
    }
    if($Uninstall -and !$kiln) {
        $current=Read-ManagedFile 'dinput8.dll'
        if($null -ne $current) {
            if(!$ownedHash -or (Managed-Hash $current) -ne $ownedHash){throw 'Foreign root proxy; uninstall refused'}
            $changes['dinput8.dll']=$null;$changes['RandomWorld/installed.sha256']=$null
        }
    }
    if(-not $kiln -or $Uninstall) {
        $changes[$nativeDll]=$null;$changes[$nativeManifest]=$null;$changes[$nativeReceipt]=$null
    }
    # The old standalone receipt identifies exactly our proxy moved by Kiln.
    # Remove its chain entries and file together; never touch another loader.
    $removePaths=@()
    $chain=Join-Path $GamePath 'kiln/chain'
    Assert-PlainPath $chain
    if($ownedHash -and (Test-Path -LiteralPath $chain)) {
        foreach($f in Get-ChildItem -LiteralPath $chain -File -Filter '*.dll') {
            $relative='kiln/chain/'+$f.Name
            if((Managed-Hash (Read-ManagedFile $relative)) -eq $ownedHash){$removePaths+=$relative;$changes[$relative]=$null}
        }
    }
    if($kiln -and $ownedHash){$changes['RandomWorld/installed.sha256']=$null}
    foreach($ini in @('kiln/chain.ini','dinput8-chain.ini')) {
        $raw=Read-ManagedFile $ini
        if($null -eq $raw -or !$removePaths.Count){continue}
        $utf8=[Text.UTF8Encoding]::new($false,$true)
        $text=$utf8.GetString($raw)
        $base=if($ini -eq 'kiln/chain.ini'){Join-Path $GamePath 'kiln'}else{$GamePath}
        $new=[regex]::Replace($text,'(?m)^[^\r\n]*(?:\r?\n|$)',{
            param($match)
            $line=$match.Value.Trim().Trim('"')
            if($ini -eq 'dinput8-chain.ini' -and $line.Contains('=')){$line=$line.Substring($line.IndexOf('=')+1).Trim().Trim('"')}
            if($line -and !$line.StartsWith('[') -and !$line.StartsWith(';') -and !$line.StartsWith('#')) {
                $candidate=[IO.Path]::GetFullPath((Join-Path $base $line))
                foreach($removed in $removePaths){if($candidate -ieq (Get-ManagedPath $removed)){return ''}}
            }
            return $match.Value
        })
        if($new -cne $text){$changes[$ini]=$utf8.GetBytes($new)}
    }
    if($Uninstall -and !$nativeRecord -and !$removePaths.Count){throw 'No recognized native installation'}
    $entries=@()
    foreach($key in $changes.Keys) {
        $before=Read-ManagedFile $key;$after=$changes[$key]
        if((Managed-Hash $before) -eq (Managed-Hash $after)){continue}
        $entries+=@{path=$key;before=$(if($null -eq $before){$null}else{[Convert]::ToBase64String($before)});beforeHash=(Managed-Hash $before);afterHash=(Managed-Hash $after)}
    }
    if(!$entries.Count -and !$packageMoves.Count){Write-Output 'Managed files already up to date.';return}
    $journal=Join-Path $data 'dual-install.pending.json'
    Assert-PlainPath $journal
    # Journal precedes every mutation and doubles as a permanent backup on success.
    $json=@{format=1;files=$entries;packageMoves=$packageMoves} | ConvertTo-Json -Depth 5
    $temp=$journal+'.new';[IO.File]::WriteAllText($temp,$json,[Text.UTF8Encoding]::new($true));[IO.File]::Move($temp,$journal)
    try {
        if($packageMoves.Count){New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null}
        foreach($name in $packageMoves){$paths=Get-MovePaths $name;Move-Item -LiteralPath $paths[0] -Destination $paths[1]}
        foreach($entry in $entries){
            if((Managed-Hash (Read-ManagedFile $entry.path)) -ne $entry.beforeHash){throw 'File changed before publication'}
            Write-ManagedFile $entry.path $changes[$entry.path]
        }
        foreach($entry in $entries){if((Managed-Hash (Read-ManagedFile $entry.path)) -ne $entry.afterHash){throw 'Publication hash mismatch'}}
        $backup=Join-Path $data ('dual-install.backup.'+[guid]::NewGuid().ToString('N')+'.json')
        [IO.File]::Move($journal,$backup)
    } catch { $failure=$_;Restore-DualInstall;throw $failure }
    Write-Output ('ModLoader 0.6.15 '+$(if($Uninstall){'removed'}elseif($kiln){'installed for Kiln'}else{'installed standalone'})+'. Profiles and packages preserved.')
}

try {
Restore-DualInstall
$kiln=$false
if(Test-Path -LiteralPath $dll -PathType Leaf) {
    if((Get-Item -LiteralPath $dll).Length -le 128MB) {
        try {Assert-LoaderBinary ([IO.File]::ReadAllBytes($dll)) @('KilnProxyInfo');$kiln=$true} catch {}
    }
}
if($kiln -or (Test-Path -LiteralPath (Join-Path $data 'kiln-installed.json'))) {
    if(Test-Path -LiteralPath $pending){throw 'Finish the interrupted standalone installation before changing loader mode.'}
    if(-not $Uninstall){Prepare-ProfileMigration}
    Install-DualMode $kiln
    if(-not $Uninstall){Complete-ProfileMigration}
    return
}
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
Prepare-ProfileMigration
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
Complete-ProfileMigration
Write-Host "ModLoader 0.6.15 installed and verified in $GamePath."
} finally { $installLock.Dispose() }
