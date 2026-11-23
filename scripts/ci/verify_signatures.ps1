# Vérifie les signatures Authenticode des fichiers livrés (signtool verify /pa).
#
#   pwsh scripts/ci/verify_signatures.ps1 -Dist dist/picazoo -Extra dist/Picazoo-…-setup.exe [-Strict]
#
# - Nos fichiers : picazoo.exe, picazoo-viewer.exe et les fichiers passés dans -Extra
#   (l'installateur). Avec -Strict, le script échoue si l'un d'eux n'est pas signé.
# - Les autres .exe et .dll (Qt, dépendances vcpkg, vc_redist) viennent d'autres projets :
#   SignPath Foundation interdit de les signer avec notre certificat. Ils sont comptés
#   (signés par leur éditeur ou non) sans faire échouer la vérification.
#
# Sans -Strict (CI sans SignPath), le script ne fait que rendre compte.
param(
    [Parameter(Mandatory)][string]$Dist,
    [string[]]$Extra = @(),
    [switch]$Strict
)

$ErrorActionPreference = 'Stop'
$ours = @('picazoo.exe', 'picazoo-viewer.exe')

$signtool = (Get-Command signtool.exe -ErrorAction SilentlyContinue).Source
if (-not $signtool) {
    $signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $signtool) { throw 'signtool.exe introuvable (Windows SDK).' }
Write-Host "signtool : $signtool"

$extraPaths = @($Extra | Where-Object { $_ } | ForEach-Object { (Resolve-Path $_).Path })
$files = @(Get-ChildItem $Dist -Recurse -File -Include *.exe, *.dll) + @($extraPaths | ForEach-Object { Get-Item $_ })

$rows = foreach ($f in $files) {
    # Un fichier non signé écrit sur stderr : ce n'est pas une erreur du script.
    & { $ErrorActionPreference = 'Continue'; & $signtool verify /pa /q $f.FullName *> $null }
    $signed = ($LASTEXITCODE -eq 0)
    $signer = ''
    if ($signed) {
        $cert = (Get-AuthenticodeSignature $f.FullName).SignerCertificate
        if ($cert) { $signer = $cert.GetNameInfo([System.Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false) }
    }
    [pscustomobject]@{
        Path    = $f.FullName.Replace("$((Get-Location).Path)\", '')
        Name    = $f.Name
        Own     = ($ours -contains $f.Name) -or ($extraPaths -contains $f.FullName)
        Signed  = $signed
        Signer  = $signer
        Product = $f.VersionInfo.ProductName
        Version = $f.VersionInfo.ProductVersion
    }
}
$global:LASTEXITCODE = 0

$mine = @($rows | Where-Object Own)
$upstream = @($rows | Where-Object { -not $_.Own })
$upstreamUnsigned = @($upstream | Where-Object { -not $_.Signed })

Write-Host '--- Nos fichiers ---'
$mine | Format-Table Path, Signed, Signer, Product, Version -AutoSize | Out-String -Width 200 | Write-Host
Write-Host "--- Fichiers d'autres projets : $($upstream.Count), dont $($upstreamUnsigned.Count) non signés par leur éditeur ---"
$upstreamUnsigned | Format-Table Path -AutoSize | Out-String -Width 200 | Write-Host

if ($env:GITHUB_STEP_SUMMARY) {
    $md = @('### Signatures', '', '| Fichier | Signé | Signataire | Produit | Version |', '|---|---|---|---|---|')
    foreach ($r in $mine) {
        $md += "| ``$($r.Path)`` | $(if ($r.Signed) { 'oui' } else { 'non' }) | $($r.Signer) | $($r.Product) | $($r.Version) |"
    }
    $md += ''
    $md += "Fichiers d'autres projets (Qt, vcpkg, Visual C++) : $($upstream.Count), dont $($upstreamUnsigned.Count) non signés par leur éditeur."
    $md -join "`n" | Out-File $env:GITHUB_STEP_SUMMARY -Append -Encoding utf8
}

foreach ($name in $ours) {
    if (-not ($mine | Where-Object Name -eq $name)) {
        Write-Host "::error::$name introuvable dans $Dist"
        exit 1
    }
}
$missing = @($mine | Where-Object { -not $_.Signed })
if ($missing.Count -gt 0) {
    $list = ($missing | ForEach-Object Name) -join ', '
    if ($Strict) {
        Write-Host "::error::Fichiers de Picazoo non signés : $list"
        exit 1
    }
    Write-Host "::notice::Signature SignPath non configurée : $list non signés."
}
exit 0
