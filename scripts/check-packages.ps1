$pkg = "D:\vs2022\setup\packages"
if (Test-Path $pkg) {
    $files = Get-ChildItem $pkg -Recurse -File
    $totalSize = ($files | Measure-Object -Property Length -Sum).Sum
    $count = $files.Count
    $sizeGB = [math]::Round($totalSize / 1GB, 2)
    Write-Host "File count: $count"
    Write-Host "Total size: $sizeGB GB"
} else {
    Write-Host "Packages folder not found"
}
