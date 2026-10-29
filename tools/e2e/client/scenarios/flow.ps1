# U8 flow: Nature's Compass end to end in a real client, as a player uses it.
#
# local (DEFAULT world + slow search config from flow.prelaunch.ps1): give the compass, right-click
# -> Select Biome, type a filter, cycle Sort By, pick Desert, Search -> HUD Searching -> Found with
# coordinates / distance, the needle turning with the player, Search for Next from the screen,
# Teleport, sneak + right-click reset; then the selftests of the integrated server (showcase,
# search, actions, hud). Screenshots: out/flow/*.png.
#
# remote-vanilla / remote-mod (a client without / with the mod on a BDS, flat world): the form
# opens, Plains is searched from the screen, the HUD renders (vanilla: the server's fallback
# through the resource pack; modded: the native overlay), and normal /title and actionbar texts
# still show while and after the compass block is on screen; T + ESCAPE opens and closes the chat.
#
# showcase.ps1 dot-sources this file with $script:FlowFrames = $true to also record frame bursts
# (frame-<clip>-NNN.png) for the docs/showcase clips.

$vanilla = $script:Mode -eq 'remote-vanilla'
$remote  = $script:Mode -ne 'local'   # the BDS host runs a flat world with the default config
if (-not (Get-Variable FlowFrames -Scope Script -ErrorAction SilentlyContinue)) { $script:FlowFrames = $false }

function Get-LinesSince([int]$Mark, [string]$Pattern) { @(Get-ServerLog -Since $Mark | Where-Object { $_ -match $Pattern }) }

function Use-Compass([switch]$Sneak) {
    if ($Sneak) { [BsE2E.Native]::Key([uint16]0x10, $false); Start-Sleep -Milliseconds 400 }
    Click 0.5 0.5 -Relative -Right
    if ($Sneak) { Start-Sleep -Milliseconds 300; [BsE2E.Native]::Key([uint16]0x10, $true) }
}

# Screenshots until `$UntilPattern` shows up in the log (and `$After` more), for a clip.
function Save-Burst([string]$Clip, [int]$Mark, [string]$UntilPattern, [int]$Max = 120, [int]$After = 6, [int]$DelayMs = 0) {
    $n = 0; $left = $After
    while ($n -lt $Max) {
        Save-Screenshot ('frame-{0}-{1:D3}' -f $Clip, $n) | Out-Null
        $n++
        if ($DelayMs) { Start-Sleep -Milliseconds $DelayMs }
        if ($UntilPattern -and (Get-LinesSince $Mark $UntilPattern).Count -gt 0) { if (--$left -le 0) { break } }
    }
    Write-Step "clip '$Clip': $n frames"
}

# Data-driven Select Biome form, 1600x900 client area (out/gui/ddui-open.png of the gui scenario):
# the search field, then Search, Search for Next, Sort By, Teleport, Cancel, then the rows.
$ui = @{ Field = 0.285; Search = 0.389; Next = 0.472; Sort = 0.555; Teleport = 0.639; Cancel = 0.722; FirstRow = 0.838 }

function Open-SelectBiome([string]$Name) {
    $m = Get-LogMark
    Use-Compass
    $line = Wait-LogLine 'Select Biome opened for ' 30 -Since $m
    Start-Sleep -Milliseconds 1500
    if ($Name) { Save-Screenshot $Name | Out-Null }
    return $line
}

function Invoke-FormButton([string]$What, [double]$Y, [string]$Expect) {
    $m = Get-LogMark
    Click 0.5 $Y -Relative
    Wait-LogLine $Expect 15 -Since $m | Out-Null
    Write-Step "pressed $What"
}

Invoke-Chat '/gamerule sendcommandfeedback false'
Invoke-Chat '/gamerule showcoordinates false'
Invoke-Chat '/clear @s'
Invoke-Chat '/xp 10L @s'
Invoke-Chat '/give @s natureslodestone:naturescompass'
Press-Key 1
Start-Sleep -Milliseconds 4000   # the "given" chat lines fade
Save-Screenshot 'given' | Out-Null

# ---- Select Biome: open, filter, sort, select, Search ----
$opened = Open-SelectBiome 'gui-open'
if (-not $vanilla -and $opened -notmatch 'data-driven') { throw "expected the data-driven form: $opened" }
$biome  = if ($remote) { 'plains' } else { 'desert' }

$m = Get-LogMark
Click 0.5 $ui.Field -Relative
Start-Sleep -Milliseconds 400
Type-Text $biome
Wait-LogLine "Select Biome: .* filter '$biome' \(\d+ listed\)" 15 -Since $m | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'gui-filter' | Out-Null

Invoke-FormButton 'Sort By' $ui.Sort 'Select Biome: .* sort by Source'
Start-Sleep -Milliseconds 600
Save-Screenshot 'gui-sort' | Out-Null

Invoke-FormButton "the $biome row" $ui.FirstRow "Select Biome: .* selected minecraft:$biome"
Start-Sleep -Milliseconds 800
Save-Screenshot 'gui-selected' | Out-Null

$m = Get-LogMark
Invoke-FormButton 'Search' $ui.Search "Select Biome: .* chose Search \(minecraft:$biome\)"
Wait-LogLine 'BiomeSearchWorker [0-9a-f]{8}: Starting search: ' 15 -Since $m | Out-Null
if ($script:FlowFrames) { Save-Burst 'search' $m 'BiomeSearchWorker [0-9a-f]{8}: Search (succeeded|failed)' -Max 150 -After 8 }
else {
    Start-Sleep -Milliseconds 700
    Save-Screenshot 'hud-searching' | Out-Null
    if ((Get-LinesSince $m 'Search succeeded').Count -gt 0) { Write-Step 'warning: the search ended before the Searching screenshot' }
}
$done = Wait-LogLine 'BiomeSearchWorker [0-9a-f]{8}: Search (succeeded|failed): ' 120 -Since $m
if ($done -notmatch 'succeeded') { throw "the $biome search failed: $done" }
Start-Sleep -Milliseconds 1500
Save-Screenshot 'hud-found' | Out-Null

if ($remote) {
    # The fallback HUD is a marked title; normal titles and the actionbar must still show.
    Invoke-Chat '/title @s title Hello'
    Start-Sleep -Milliseconds 1200
    Save-Screenshot 'title-while-hud' | Out-Null
    Invoke-Chat '/title @s actionbar Hi'
    Start-Sleep -Milliseconds 1200
    Save-Screenshot 'actionbar-while-hud' | Out-Null
    Start-Sleep -Milliseconds 6000   # both fade; the compass block stays
    Save-Screenshot 'hud-after-titles' | Out-Null
    $m = Get-LogMark
    Use-Compass -Sneak
    Wait-LogLine 'Compass reset by ' 15 -Since $m | Out-Null
    Start-Sleep -Milliseconds 1500
    Save-Screenshot 'hud-reset' | Out-Null
    Invoke-Chat '/title @s title After'
    Start-Sleep -Milliseconds 1200
    Save-Screenshot 'title-after-hud' | Out-Null
    Invoke-Chat '/title @s actionbar AfterBar'
    Start-Sleep -Milliseconds 1200
    Save-Screenshot 'actionbar-after-hud' | Out-Null
    # Chat: T opens it, ESCAPE closes it.
    Focus-Game; Press-Key T; Start-Sleep -Milliseconds 1200
    Save-Screenshot 'chat-open' | Out-Null
    Press-Key ESCAPE; Start-Sleep -Milliseconds 1200
    Save-Screenshot 'chat-closed' | Out-Null
    return
}

# ---- the needle: it turns as the player turns (frame -8 per +90 degrees of yaw) ----
function Get-NeedleFrame {
    $mark = Get-LogMark
    Invoke-Chat '/naturescompass needle frame'
    $line = Wait-LogLine 'needle: frame (\d+)' 20 -Since $mark
    if ($line -notmatch 'needle: frame (\d+)') { throw "no needle frame in '$line'" }
    return [int]$Matches[1]
}
$frames = @()
foreach ($yaw in 0, 90, 180, 270) {
    Invoke-Chat "/tp @s ~ ~ ~ $yaw 10"
    Start-Sleep -Milliseconds 2500
    Save-Screenshot "needle-yaw$yaw" | Out-Null
    $frames += Get-NeedleFrame
}
Write-Step "needle frames for yaw 0/90/180/270: $($frames -join ', ')"
for ($i = 1; $i -lt 4; $i++) {
    $step = (($frames[$i] - $frames[$i - 1]) % 32 + 32) % 32
    if ([Math]::Abs($step - 24) -gt 1) { throw "needle: yaw +90 changed the frame by $step (expected 24 = -8): $($frames -join ', ')" }
}
if ($script:FlowFrames) {
    # A smooth turn for the clip: raw mouse movement, one screenshot per step.
    Invoke-Chat '/tp @s ~ ~ ~ 0 10'
    Start-Sleep -Milliseconds 1500
    for ($i = 0; $i -lt 72; $i++) {
        [BsE2E.Native]::MoveRelative(40, 0)
        Start-Sleep -Milliseconds 60
        Save-Screenshot ('frame-needle-{0:D3}' -f $i) | Out-Null
    }
}

# ---- Search for Next from the screen (the found biome is pre-selected) ----
$opened = Open-SelectBiome 'gui-found'
if ($opened -notmatch "selected 'minecraft:$biome'") { throw "the found biome is not pre-selected: $opened" }
$m = Get-LogMark
Invoke-FormButton 'Search for Next' $ui.Next "Select Biome: .* chose Search for Next \(minecraft:$biome\)"
Wait-LogLine 'BiomeSearchWorker [0-9a-f]{8}: Starting search: .*, 1 previous locations' 15 -Since $m | Out-Null
$done = Wait-LogLine 'BiomeSearchWorker [0-9a-f]{8}: Search (succeeded|failed): ' 180 -Since $m
if ($done -notmatch 'succeeded') { throw "Search for Next failed: $done" }
Start-Sleep -Milliseconds 1500
Save-Screenshot 'hud-found-next' | Out-Null

# ---- Teleport (cheats are on in the test world) ----
Open-SelectBiome | Out-Null
$m = Get-LogMark
Invoke-FormButton 'Teleport' $ui.Teleport 'Select Biome: .* chose Teleport'
$tp = Wait-LogLine 'Player \S+ teleported to -?\d+ -?\d+ -?\d+' 30 -Since $m
Write-Step $tp
if ($script:FlowFrames) { Save-Burst 'teleport' $m '' -Max 20 -DelayMs 100 }
Start-Sleep -Milliseconds 2500
Save-Screenshot 'teleported' | Out-Null

# ---- sneak + right click resets the compass: the HUD block goes away ----
$m = Get-LogMark
Use-Compass -Sneak
Wait-LogLine 'Compass reset by ' 15 -Since $m | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'reset' | Out-Null
if ((Get-LinesSince $m 'Compass used by').Count -ne 0) { throw 'the sneaking use opened the screen' }

if ($script:FlowFrames) { return }   # the showcase only re-shoots pictures

# ---- the units' selftests on the integrated server of this DEFAULT world ----
# Back to NC's default config first (the selftests expect it), then reload.
Remove-Item -Force (Join-Path (Join-Path (Join-Path $script:ModsDir $script:ModName) 'config') 'naturescompass.json')
Invoke-Chat '/naturescompass reload'   # a missing file loads (and is saved with) the defaults
Start-Sleep -Seconds 2
foreach ($glob in 'showcase.*', 'search.*', 'actions.*', 'hud.*') {
    $r = Invoke-SelfTest $glob -TimeoutSeconds 1200
    if ($r.Fail -gt 0) { throw "$($r.Fail) $glob selftest(s) failed" }
}
if (-not (Get-ServerLog | Where-Object { $_ -match 'showcase\.flow_real_biomes: next desert at' })) {
    throw 'showcase.flow_real_biomes did not run the real-biome flow'
}
if (-not (Get-ServerLog | Where-Object { $_ -match 'search\.overworld_biomes: next minecraft:' })) {
    throw 'search.overworld_biomes did not search the overworld'
}
