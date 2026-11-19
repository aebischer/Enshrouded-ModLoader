$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$game=Join-Path $root ('work/worlds-migration-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $game | Out-Null
Copy-Item 'D:/SteamLibrary/steamapps/common/Enshrouded/enshrouded.exe' $game
function Put($path,$value){$p=Join-Path $game $path;New-Item -ItemType Directory -Path (Split-Path $p -Parent) -Force | Out-Null;[IO.File]::WriteAllText($p,$value,[Text.UTF8Encoding]::new($false))}
function Run($pass,$remove=$false){
 $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $root 'install.ps1')+'"'),'-GamePath',('"'+$game+'"'));if($remove){$args+='-Uninstall'}
 $p=Start-Process powershell.exe -ArgumentList $args -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $game 'out.txt') -RedirectStandardError (Join-Path $game 'err.txt')
 if(($p.ExitCode -eq 0) -ne $pass){throw (Get-Content (Join-Path $game 'err.txt') -Raw)}
}
$old='ModLoader/worlds/';$new='Mods/EnshroudedModLoader/worlds/'
Put ($old+'a.mods') "ENSHROUDED_MODS_V1`ncarryon/carryon.dll`n"
Put ($old+'a.settings-v2/clé.ini') 'value=true'
Put ($old+'a.settings/lantern/old.ini') 'legacy sentinel'
Put ($new+'b.mods') 'existing destination sentinel'
# Simulate a previous interrupted copy: one identical target already exists.
Put ($new+'a.settings-v2/clé.ini') 'value=true'
$expected=@{};foreach($f in Get-ChildItem (Join-Path $game $old) -Recurse -File){$expected[$f.FullName.Substring((Join-Path $game $old).Length)]=(Get-FileHash $f.FullName).Hash}
Run $true
foreach($rel in $expected.Keys){if((Get-FileHash (Join-Path $game ($new+$rel))).Hash -ne $expected[$rel]){throw 'Migration changed bytes'}}
if(@(Get-ChildItem (Join-Path $game $old) -Recurse -File).Count){throw 'Legacy files remain'}
if((Get-Content (Join-Path $game ($new+'b.mods'))) -ne 'existing destination sentinel'){throw 'Destination-only file changed'}
if(!(Get-ChildItem (Join-Path $game 'RandomWorld') -Directory -Filter 'worlds-backup.*')){throw 'Missing backup'}
Run $true
# Conflicting files must leave the old and new data and DLL untouched.
Put ($old+'conflict.mods') 'old choice';Put ($new+'conflict.mods') 'new choice'
$hash=(Get-FileHash (Join-Path $game 'dinput8.dll')).Hash
Run $false
if((Get-Content (Join-Path $game ($old+'conflict.mods'))) -ne 'old choice' -or (Get-Content (Join-Path $game ($new+'conflict.mods'))) -ne 'new choice' -or (Get-FileHash (Join-Path $game 'dinput8.dll')).Hash -ne $hash){throw 'Conflict changed data'}
Put ($new+'conflict.mods') 'old choice'
# An install failure after copies leaves every old file available to the old DLL.
$lock=[IO.File]::Open((Join-Path $game 'RandomWorld/installed.sha256'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try{Run $false}finally{$lock.Dispose()}
if((Get-Content (Join-Path $game ($old+'conflict.mods'))) -ne 'old choice'){throw 'Failed install retired originals'}
Run $true
Run $true $true
foreach($rel in $expected.Keys){if((Get-FileHash (Join-Path $game ($new+$rel))).Hash -ne $expected[$rel]){throw 'Uninstall changed settings'}}
'PASS: profile/settings migration, existing identical/destination-only files, legacy settings, Unicode, backup, interrupted copy retry, conflicting copies, install failure, idempotence and uninstall preservation'
