$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$fixture = Join-Path $root ('build\transfer-test-'+[guid]::NewGuid().ToString('N'))
$world = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
$dir = Join-Path $fixture 'ModLoader\worlds'
New-Item -ItemType Directory -Path $dir | Out-Null
$profile = Join-Path $dir ($world+'.mods')
[IO.File]::WriteAllText($profile,"ENSHROUDED_MODS_V1`ncarry/chest.dll`n",(New-Object Text.UTF8Encoding($false)))
$hash = (Get-FileHash -LiteralPath $profile).Hash
$tool = Join-Path $root 'tools/transfer-profile.ps1'
$bundle = Join-Path $fixture 'profile.worldmods'
& $tool -Action Export -GamePath $fixture -WorldId $world -Path $bundle
[IO.File]::WriteAllText($profile,"ENSHROUDED_MODS_V1`n")
& $tool -Action Import -GamePath $fixture -WorldId $world -Path $bundle
if ((Get-FileHash -LiteralPath $profile).Hash -ne $hash -or -not (Test-Path -LiteralPath ($profile+'.before-import'))) {throw 'Transfer/backup failed'}
$record = Get-Content -LiteralPath $bundle -Raw | ConvertFrom-Json
$record.world = 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'
$record | ConvertTo-Json | Set-Content -LiteralPath $bundle -Encoding UTF8
$refused = $false
try { & $tool -Action Import -GamePath $fixture -WorldId $world -Path $bundle } catch {$refused=$true}
if (-not $refused -or (Get-FileHash -LiteralPath $profile).Hash -ne $hash) {throw 'World identity guard failed'}
$record.world = $world
$record.sha256 = '0'*64
$record | ConvertTo-Json | Set-Content -LiteralPath $bundle -Encoding UTF8
$refused = $false
try { & $tool -Action Import -GamePath $fixture -WorldId $world -Path $bundle } catch {$refused=$true}
if (-not $refused -or (Get-FileHash -LiteralPath $profile).Hash -ne $hash) {throw 'Corruption guard failed'}
Write-Output 'PASS: profile export/import, identity, backup, corruption rejection preserves previous profile.'
