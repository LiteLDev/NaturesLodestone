# U8 flow (local mode): a DEFAULT world with the server harness's seed (real biomes), no
# coordinates line or command feedback in the screenshots, and a search config that keeps the
# compass Searching for a few seconds so the HUD can be photographed in that state:
# sampleSpaceModifier 1 (a sample every 4 blocks) and maxSamples 1000000. One XP level per search
# so the screen shows level costs. Everything else is NC's default (docs/SPEC.md 5.1).
if ($script:Mode -ne 'local') { return }   # remote modes use the BDS host's (flat) world and config

. (Join-Path $PSScriptRoot '../../common/nbt.ps1')
$levelDat = Join-Path (Join-Path (Get-WorldsDir) $script:WorldName) 'level.dat'
$ld = Read-LevelDat $levelDat
Set-NbtValue $ld.Root 'Generator' 3 1            # 1 = infinite (DEFAULT), 2 = flat
Set-NbtValue $ld.Root 'RandomSeed' 4 ([long]20260521)
Set-NbtValue $ld.Root 'showcoordinates' 1 ([byte]0)
Set-NbtValue $ld.Root 'sendcommandfeedback' 1 ([byte]0)
Set-NbtValue $ld.Root 'dodaylightcycle' 1 ([byte]0)
Set-NbtValue $ld.Root 'doweathercycle' 1 ([byte]0)
Write-LevelDat $ld $levelDat
Write-Step 'level.dat: DEFAULT generator, seed 20260521, no coordinates / command feedback'

$cfgDir = Join-Path (Join-Path $script:ModsDir $script:ModName) 'config'
New-Item -ItemType Directory -Force $cfgDir | Out-Null
$config = [ordered]@{
    version = 1; allowTeleport = $true; maxNextSearches = 25; displayCoordinates = $true; radiusModifier = 2500
    sampleSpaceModifier = 1; biomeBlacklist = @(); defaultXpLevels = 1; perBiomeXpLevels = @('minecraft:deep_dark,3', 'minecraft:*ocean*,2')
    maxSamples = 1000000; compassDurability = 0
}
Set-Content -Encoding utf8NoBOM -Path (Join-Path $cfgDir 'naturescompass.json') -Value ($config | ConvertTo-Json)
