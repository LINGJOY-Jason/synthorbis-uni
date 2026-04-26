$base = "D:\SynthOrbisUNI\engine\librime\dist_static"
Write-Host "=== lib ==="
Get-ChildItem "$base\lib" | ForEach-Object {
    $mb = [math]::Round($_.Length / 1MB, 2)
    Write-Host "$($_.Name)  :  $mb MB"
}
Write-Host "`n=== include ==="
Get-ChildItem "$base\include" | ForEach-Object {
    Write-Host $_.Name
}
Write-Host "`n=== share ==="
Get-ChildItem "$base\share\cmake\rime" -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host $_.Name
}
