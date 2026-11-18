param([Parameter(Mandatory=$true)][string]$WorkDirectory)
$ErrorActionPreference='Stop'
$dir=[IO.Path]::GetFullPath($WorkDirectory)
New-Item -ItemType Directory -Path $dir -Force | Out-Null
$inputFile=Join-Path $dir 'catalog.json'
$outputFile=Join-Path $dir 'catalog.h'
$compiler=Join-Path $PSScriptRoot '../tools/compile-translations.ps1'
$utf8=[Text.UTF8Encoding]::new($false)
$catalog=[ordered]@{}
foreach($lang in @('en','fr','it','de','es','ja','ko','pl','pt-br','ru','zh-cn','zh-tw','tr','uk','th')) {
 $catalog[$lang]=@{'item.name'='<b>Text</b> {count}'}
}
function Check-Catalog([bool]$shouldPass) {
 [IO.File]::WriteAllText($inputFile,($catalog | ConvertTo-Json -Depth 4),$utf8)
 $passed=$false
 try { & $compiler -Catalog $inputFile -Output $outputFile -Symbol TestTexts | Out-Null; $passed=$true } catch { if($shouldPass){throw} }
 if($passed -ne $shouldPass){throw 'Unexpected catalogue validation result'}
}
Check-Catalog $true
$uk=$catalog.uk; $catalog.Remove('uk'); Check-Catalog $false; $catalog.uk=$uk
$catalog.uk=@{}; Check-Catalog $false; $catalog.uk=$uk
$catalog.uk['item.name']=''; Check-Catalog $false
$catalog.uk['item.name']='<b>Text {count}'; Check-Catalog $false
$catalog.uk['item.name']='<b>Text</b> {other}'; Check-Catalog $false
$catalog.uk['item.name']="Bad$([char]0)text"; Check-Catalog $false
$catalog.uk['item.name']='<b>'+[char]0x422+'</b> {count}'; Check-Catalog $true
[IO.File]::WriteAllBytes($inputFile,[byte[]](123,255,125))
$failed=$false
try { & $compiler -Catalog $inputFile -Output $outputFile -Symbol TestTexts | Out-Null } catch { $failed=$true }
if(!$failed){throw 'Invalid UTF-8 was accepted'}
Write-Output 'PASS: complete catalogue; missing language/key, empty text, markup, placeholder, controls and invalid UTF-8 rejected.'
