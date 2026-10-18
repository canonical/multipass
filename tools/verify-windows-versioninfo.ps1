param(
    [string]$BinDir = "build/bin",
    [switch]$FailOnMissing
)

$ErrorActionPreference = "Continue"

# Explorer's Details tab typically shows the fixed numeric file version,
# not the string-table FileVersion/ProductVersion values.
function Get-NumericVersionString {
    param([System.Diagnostics.FileVersionInfo]$Info)

    return "{0}.{1}.{2}.{3}" -f $Info.FileMajorPart, $Info.FileMinorPart, $Info.FileBuildPart, $Info.FilePrivatePart
}

# Keep the expected descriptions alongside the binaries we validate so the
# script can be reused both locally and in CI.
$targets = @(
    @{ File = "multipass.exe";    Description = "Multipass CLI" },
    @{ File = "multipassd.exe";   Description = "Multipass Daemon" },
    @{ File = "sshfs_server.exe"; Description = "Multipass SSHFS Server" }
)

$hasError = $false

foreach ($target in $targets) {
    $path = Join-Path -Path $BinDir -ChildPath $target.File

    if (-not (Test-Path -Path $path)) {
        Write-Warning "Missing file: $path"
        $hasError = $true
        continue
    }

    $resolvedPath = (Resolve-Path -Path $path).Path
    $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($resolvedPath)

    # Show both the human-readable string-table version and the fixed numeric
    # version that Windows surfaces in Properties -> Details.
    $numericFileVersion = Get-NumericVersionString -Info $info
    $numericProductVersion = "{0}.{1}.{2}.{3}" -f $info.ProductMajorPart, $info.ProductMinorPart, $info.ProductBuildPart, $info.ProductPrivatePart

    Write-Host "File                           : $path"
    Write-Host "FileVersion_StringTable        : $($info.FileVersion)"
    Write-Host "ProductVersion_StringTable     : $($info.ProductVersion)"
    Write-Host "FileVersion_DetailsTabStyle    : $numericFileVersion"
    Write-Host "ProductVersion_DetailsTabStyle : $numericProductVersion"
    Write-Host "CompanyName                    : $($info.CompanyName)"
    Write-Host "Copyright                      : $($info.LegalCopyright)"
    Write-Host "FileDescription                : $($info.FileDescription)"
    Write-Host "ProductName                    : $($info.ProductName)"
    Write-Host ""

    if ($FailOnMissing) {
        # Validate both resource representations because either one can regress
        # independently if RC generation or linker inputs change.
        if ([string]::IsNullOrWhiteSpace($info.FileVersion)) {
            Write-Error "Missing FileVersion string for $path"
            $hasError = $true
        }
        if ([string]::IsNullOrWhiteSpace($info.ProductVersion)) {
            Write-Error "Missing ProductVersion string for $path"
            $hasError = $true
        }
        if ($numericFileVersion -eq "0.0.0.0") {
            Write-Error "Missing/invalid fixed FileVersion (Explorer Details value) for $path"
            $hasError = $true
        }
        if ($numericProductVersion -eq "0.0.0.0") {
            Write-Error "Missing/invalid fixed ProductVersion (Explorer Details value) for $path"
            $hasError = $true
        }
        if ($info.CompanyName -ne "Canonical Ltd.") {
            Write-Error "Unexpected CompanyName '$($info.CompanyName)' for $path. Expected 'Canonical Ltd.'"
            $hasError = $true
        }
        if ($info.LegalCopyright -ne "Copyright (C) Canonical, Ltd. All rights reserved.") {
            Write-Error "Unexpected Copyright '$($info.LegalCopyright)' for $path. Expected 'Copyright (C) Canonical, Ltd. All rights reserved.'"
            $hasError = $true
        }
        if ($info.FileDescription -ne $target.Description) {
            Write-Error "Unexpected FileDescription '$($info.FileDescription)' for $path. Expected '$($target.Description)'"
            $hasError = $true
        }
        if ([string]::IsNullOrWhiteSpace($info.ProductName)) {
            Write-Error "Missing ProductName for $path"
            $hasError = $true
        }
    }
}

# CI uses the exit code to fail the workflow when metadata is missing or wrong.
if ($hasError -and $FailOnMissing) {
    exit 1
}
