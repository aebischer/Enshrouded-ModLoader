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
    if (-not $GamePath) { $GamePath = Read-Host 'Dossier contenant enshrouded.exe' }
}
$GamePath = (Resolve-Path -LiteralPath $GamePath).Path
$exe = Join-Path $GamePath 'enshrouded.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Enshrouded.exe introuvable.' }
foreach ($running in @(Get-Process enshrouded -ErrorAction SilentlyContinue)) {
    if (-not $running.Path -or $running.Path -eq $exe) { throw 'Fermez Enshrouded avant de continuer.' }
}
$dll = Join-Path $GamePath 'dinput8.dll'
$data = Join-Path $GamePath 'RandomWorld'
$receipt = Join-Path $data 'installed.sha256'
New-Item -ItemType Directory -Path $data -Force | Out-Null
$installLock = [IO.File]::Open((Join-Path $data 'install.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$pending = Join-Path $data 'install.pending.json'
$previous = Join-Path $data 'install.previous.dll'
function Restore-PendingInstall {
    if (-not (Test-Path -LiteralPath $pending)) { return }
    $record = Get-Content -LiteralPath $pending -Raw | ConvertFrom-Json
    if ($record.newHash -notmatch '^[A-F0-9]{64}$' -or ($record.oldHash -and $record.oldHash -notmatch '^[A-F0-9]{64}$')) { throw 'Journal de restauration invalide.' }
    $current = if (Test-Path -LiteralPath $dll -PathType Leaf) { (Get-FileHash -LiteralPath $dll).Hash } else { '' }
    if ($current -and $current -ne $record.newHash -and $current -ne $record.oldHash) { throw 'La DLL a change pendant une installation interrompue. Restauration refusee.' }
    if ($record.oldHash) {
        if ($current -ne $record.oldHash) {
            if (-not (Test-Path -LiteralPath $previous -PathType Leaf) -or (Get-FileHash -LiteralPath $previous).Hash -ne $record.oldHash) { throw 'Copie de restauration absente ou alteree.' }
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
    Remove-Item -LiteralPath $pending
}
function Assert-LoaderBinary([byte[]]$bytes) {
    function Need([long]$offset,[long]$count) {
        if($offset -lt 0 -or $count -lt 0 -or $offset -gt $bytes.Length -or $count -gt $bytes.Length-$offset) {throw 'DLL tronquee ou table PE invalide.'}
    }
    function U16([long]$offset) {Need $offset 2; return [BitConverter]::ToUInt16($bytes,[int]$offset)}
    function U32([long]$offset) {Need $offset 4; return [BitConverter]::ToUInt32($bytes,[int]$offset)}
    Need 0 64
    if((U16 0) -ne 0x5a4d){throw 'DLL source invalide.'}
    $pe=[long](U32 60)
    Need $pe 264
    if((U32 $pe) -ne 0x4550 -or (U16 ($pe+4)) -ne 0x8664 -or ((U16 ($pe+22)) -band 0x2000) -eq 0 -or (U16 ($pe+24)) -ne 0x20b -or (U16 ($pe+20)) -ne 240){throw 'Le chargeur doit etre une DLL PE32+ x64.'}
    $count=U16 ($pe+6)
    if($count -lt 1 -or $count -gt 96 -or (U32 ($pe+132)) -lt 16){throw 'Tables PE invalides.'}
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
        throw 'RVA PE invalide.'
    }
    $exports=[long](U32 ($pe+136)); $exportSize=[long](U32 ($pe+140))
    if($exports -eq 0 -or $exportSize -lt 40){throw 'Exports du chargeur absents.'}
    $e=Offset $exports 40
    $functions=[long](U32 ($e+20)); $names=[long](U32 ($e+24))
    if($functions -lt 6 -or $functions -gt 65536 -or $names -lt 6 -or $names -gt 65536){throw 'Exports du chargeur invalides.'}
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
            if($c -lt 32 -or $c -gt 126){throw 'Nom export invalide.'}
            $name += [char]$c
        }
        if(-not $ended){throw 'Nom export trop long.'}
        if($required -ccontains $name) {
            if($found.ContainsKey($name)){throw 'Export duplique.'}
            $index=U16 ($ord+2*$i)
            if($index -ge $functions){throw 'Ordinal invalide.'}
            $address=[long](U32 ($f+4*$index))
            if($address -ge $exports -and $address -lt $exports+$exportSize){throw 'Export redirige interdit pour le chargeur.'}
            $null=Offset $address 1
            $executable=$false
            foreach($section in $sections){if($address -ge $section.rva -and $address -lt $section.rva+$section.virtual -and ($section.flags -band 0x20000000)){$executable=$true}}
            if(-not $executable){throw 'Export non executable.'}
            $found[$name]=$true
        }
    }
    foreach($name in $required){if(-not $found.ContainsKey($name)){throw "Export du chargeur absent : $name"}}
}
try {
Restore-PendingInstall
if ($Uninstall) {
    if (-not (Test-Path -LiteralPath $receipt)) { throw 'Installation de ce prototype non identifiee.' }
    if (Test-Path -LiteralPath $dll) {
        if ((Get-FileHash -LiteralPath $dll).Hash -ne (Get-Content -LiteralPath $receipt -Raw).Trim()) { throw 'La DLL a change : desinstallation automatique refusee.' }
        Remove-Item -LiteralPath $dll
    }
    Remove-Item -LiteralPath $receipt
    Write-Host 'Mod retire. Journaux et sauvegardes conserves.'
    exit
}
if ((Get-FileHash -LiteralPath $exe).Hash -ne $expected) { throw 'Version du jeu non prise en charge par ce prototype.' }
if (Test-Path -LiteralPath $dll) {
    if (-not (Test-Path -LiteralPath $receipt)) { throw 'Un autre dinput8.dll est present. Aucun fichier remplace.' }
    if ((Get-FileHash -LiteralPath $dll).Hash -ne (Get-Content -LiteralPath $receipt -Raw).Trim()) { throw 'Conflit : DLL differente de notre installation.' }
}
foreach ($other in @('powrprof.dll','dbghelp.dll')) {
    if (Test-Path -LiteralPath (Join-Path $GamePath $other)) { throw "Autre chargeur present ($other). Integration a verifier." }
}
$source = Join-Path $PSScriptRoot 'dist\dinput8.dll'
if (-not (Test-Path -LiteralPath $source)) { throw 'Compilez le projet avant installation.' }
if (Test-Path -LiteralPath $receipt -PathType Container) { throw 'Le recu est un dossier. Installation non modifiee.' }
New-Item -ItemType Directory -Path (Join-Path $GamePath 'Mods') -Force | Out-Null
$transactionId = [guid]::NewGuid().ToString('N')
$stagedDll = Join-Path $GamePath ('dinput8.' + $transactionId + '.new')
$stagedReceipt = Join-Path $data ('receipt.' + $transactionId + '.new')
$stagedPending = Join-Path $data ('pending.' + $transactionId + '.new')
try {
    Copy-Item -LiteralPath $source -Destination $stagedDll
    $newHash = (Get-FileHash -LiteralPath $stagedDll).Hash
    if ($newHash -ne (Get-FileHash -LiteralPath $source).Hash) { throw 'Verification de la copie echouee.' }
    if((Get-Item -LiteralPath $stagedDll).Length -gt 128MB){throw 'DLL source trop volumineuse.'}
    Assert-LoaderBinary ([IO.File]::ReadAllBytes($stagedDll))
    [IO.File]::WriteAllText($stagedReceipt, $newHash, [Text.Encoding]::ASCII)
    $oldHash = if (Test-Path -LiteralPath $dll) { (Get-FileHash -LiteralPath $dll).Hash } else { '' }
    if ($oldHash) {
        Copy-Item -LiteralPath $dll -Destination $previous -Force
        if ((Get-FileHash -LiteralPath $previous).Hash -ne $oldHash) { throw 'Sauvegarde de restauration invalide.' }
    }
    @{newHash=$newHash; oldHash=$oldHash} | ConvertTo-Json -Compress | Set-Content -LiteralPath $stagedPending -Encoding ASCII
    [IO.File]::Move($stagedPending, $pending)
    if ($oldHash) { [IO.File]::Replace($stagedDll, $dll, ($stagedDll + '.old')) } else { [IO.File]::Move($stagedDll, $dll) }
    if (Test-Path -LiteralPath $receipt) { [IO.File]::Replace($stagedReceipt, $receipt, ($stagedReceipt + '.old')) } else { [IO.File]::Move($stagedReceipt, $receipt) }
    if ((Get-FileHash -LiteralPath $dll).Hash -ne $newHash -or (Get-Content -LiteralPath $receipt -Raw).Trim() -ne $newHash) { throw 'Verification finale echouee.' }
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
Write-Host "Chargeur 0.6.9 installe et verifie dans $GamePath."
} finally { $installLock.Dispose() }
