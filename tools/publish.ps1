param([Parameter(Mandatory=$true)][string]$ProjectRoot)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath($ProjectRoot)
$staged = Join-Path $taskRoot 'build/staging/TitanQuestMuseum.asi'
$dist = Join-Path $taskRoot 'dist'
$target = Join-Path $dist 'TitanQuestMuseum.asi'
if (!(Test-Path -LiteralPath $staged)) { throw 'Successful staged binary is missing.' }
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$next = Join-Path $dist 'TitanQuestMuseum.asi.next'
Copy-Item -LiteralPath $staged -Destination $next -Force
function FileDigest([string]$Path) {
    $hash = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return [Convert]::ToBase64String($hash.ComputeHash($stream)) }
    finally { $stream.Dispose(); $hash.Dispose() }
}
if ((FileDigest $next) -ne (FileDigest $staged)) { throw 'Staging copy verification failed.' }
if (Test-Path -LiteralPath $target) {
    $pdb = Join-Path $dist 'TitanQuestMuseum.pdb'
    if (Test-Path -LiteralPath $pdb) { Copy-Item -LiteralPath $pdb -Destination (Join-Path $dist 'TitanQuestMuseum.previous.pdb') -Force }
    [IO.File]::Replace($next,$target,(Join-Path $dist 'TitanQuestMuseum.previous.bin'))
} else { [IO.File]::Move($next,$target) }
Copy-Item -LiteralPath (Join-Path $taskRoot 'build/staging/TitanQuestMuseum.pdb') -Destination (Join-Path $dist 'TitanQuestMuseum.pdb') -Force
Write-Output "Published $target"
