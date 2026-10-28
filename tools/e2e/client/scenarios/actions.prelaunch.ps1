# U4 actions: durability on (4 successful searches) so the scenario can show the durability bar.
$cfgDir = Join-Path (Join-Path $script:ModsDir $script:ModName) 'config'
New-Item -ItemType Directory -Force $cfgDir | Out-Null
# Every key is required by the loader: the defaults of docs/SPEC.md 5.1, compassDurability 4.
$config = [ordered]@{
    version = 1; allowTeleport = $true; maxNextSearches = 25; displayCoordinates = $true; radiusModifier = 2500
    sampleSpaceModifier = 16; biomeBlacklist = @(); defaultXpLevels = 0; perBiomeXpLevels = @(); maxSamples = 50000
    compassDurability = 4
}
Set-Content -Encoding utf8NoBOM -Path (Join-Path $cfgDir 'naturescompass.json') -Value ($config | ConvertTo-Json)
