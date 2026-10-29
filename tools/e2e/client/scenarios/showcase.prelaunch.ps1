# U8 showcase: the flow scenario's world, with Nature's Compass' default search config (a sample
# every 64 blocks), so the deserts it finds and the teleport are what players get. One XP level
# per search stays on, so Select Biome shows level costs.
. (Join-Path $PSScriptRoot 'flow.prelaunch.ps1')
if ($script:Mode -ne 'local') { return }
$cfgFile = Join-Path (Join-Path (Join-Path $script:ModsDir $script:ModName) 'config') 'naturescompass.json'
$cfg = Get-Content -Raw $cfgFile | ConvertFrom-Json
$cfg.sampleSpaceModifier = 16
$cfg.maxSamples = 50000
Set-Content -Encoding utf8NoBOM -Path $cfgFile -Value ($cfg | ConvertTo-Json)
