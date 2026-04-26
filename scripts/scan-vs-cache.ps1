$locations = @(
    "D:\vs2022\setup\packages",
    "$env:LOCALAPPDATA\Temp\vs_",
    "$env:LOCALAPPDATA\Microsoft\VisualStudio\17.0_*
",
    "$env:APPDATA\Microsoft\VisualStudio\17.0_*",
    "C:\ProgramData\VS2022"
)

foreach ($loc in $locations) {
    $expanded = [System.Environment]::ExpandEnvironmentVariables($loc)
    if (Test-Path $expanded) {
        $files = Get-ChildItem $expanded -Recurse -File -ErrorAction SilentlyContinue
        $totalSize = ($files | Measure-Object -Property Length -Sum -ErrorAction SilentlyContinue).Sum
        $count = $files.Count
        $sizeGB = [math]::Round($totalSize / 1GB, 2)
        Write-Host "FOUND: $expanded"
        Write-Host "  Files: $count, Size: $sizeGB GB"
    } else {
        Write-Host "NOT FOUND: $expanded"
    }
}
