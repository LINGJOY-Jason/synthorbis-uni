$base = "D:\vs2022"
if (Test-Path $base) {
    $dirs = Get-ChildItem $base -Directory
    foreach ($dir in $dirs) {
        $files = Get-ChildItem $dir.FullName -Recurse -File -ErrorAction SilentlyContinue
        $totalSize = ($files | Measure-Object -Property Length -Sum -ErrorAction SilentlyContinue).Sum
        $count = $files.Count
        $sizeGB = [math]::Round($totalSize / 1GB, 2)
        $sizeMB = [math]::Round($totalSize / 1MB, 0)
        Write-Host "$($dir.Name) : $count files, $sizeMB MB ($sizeGB GB)"
    }
} else {
    Write-Host "VS2022 folder not found"
}
