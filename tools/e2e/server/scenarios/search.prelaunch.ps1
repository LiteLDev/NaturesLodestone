# U3 search: a normal (DEFAULT) world so the overworld has real biomes to search for (the other
# scenarios keep the flat world). Only level-type changes; an explicit -Property level-type=... wins.
if (-not (($Property -join ',') -match 'level-type')) {
    $file = Join-Path $script:SlotDir 'server.properties'
    (Get-Content $file) -replace '^\s*level-type\s*=.*$', 'level-type=DEFAULT' | Set-Content -Path $file -Encoding UTF8
    Write-Step 'server.properties: level-type=DEFAULT (real biomes for the search scenario)'
}
