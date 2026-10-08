param([Parameter(Mandatory=$true)][string]$ProjectRoot)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath($ProjectRoot)
$stage=Join-Path $taskRoot 'build/staging'
$dist=Join-Path $taskRoot 'dist'
$source=Join-Path $stage 'TitanQuestMobRadar.asi'
$target=Join-Path $dist 'TitanQuestMobRadar.asi'
$next=Join-Path $dist 'TitanQuestMobRadar.asi.next'
Copy-Item -LiteralPath $source -Destination $next -Force
function Digest([string]$Path){
 $hash=[Security.Cryptography.SHA256]::Create()
 $stream=[IO.File]::OpenRead($Path)
 try{return [Convert]::ToBase64String($hash.ComputeHash($stream))}
 finally{$stream.Dispose();$hash.Dispose()}
}
if((Digest $source) -ne (Digest $next)){throw 'Binary copy mismatch'}
if(Test-Path -LiteralPath $target){[IO.File]::Replace($next,$target,(Join-Path $dist 'TitanQuestMobRadar.previous.bin'))}
else{[IO.File]::Move($next,$target)}
Copy-Item -LiteralPath (Join-Path $stage 'TitanQuestMobRadar.pdb') -Destination (Join-Path $dist 'TitanQuestMobRadar.pdb') -Force
Write-Output "Published $target"
