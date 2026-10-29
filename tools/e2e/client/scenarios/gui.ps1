# U5 gui: the Select Biome screen on a real client. First the data-driven form (forced with
# `/naturescompass gui ddui`), then the default for a player with our resource pack: the classic
# form drawn in NC's layout by the RP skin. Opens it with the compass (right click), filters,
# cycles the sort, selects a biome and presses Search; every step waits for the server's log line.
# Screenshots: out/gui/*.png
Invoke-Chat '/clear @s'
Invoke-Chat '/xp 100L @s'
Invoke-Chat '/give @s natureslodestone:naturescompass'
Start-Sleep -Milliseconds 500

function Open-Screen([string]$Expect, [string]$Name) {
    $m = Get-LogMark
    Click 0.5 0.5 -Relative -Right
    Wait-LogLine "Select Biome opened for .*$Expect" 30 -Since $m | Out-Null
    Start-Sleep -Milliseconds 1500
    Save-Screenshot $Name | Out-Null
}
# ESC until the server logs the close (the first ESC may only leave the text field).
function Close-Screen-Logged {
    $m = Get-LogMark
    for ($i = 0; $i -lt 3; $i++) {
        Press-Key ESC
        Start-Sleep -Milliseconds 1200
        if (@(Get-ServerLog -Since $m | Where-Object { $_ -match 'Select Biome: .* closed the screen' }).Count) { return }
    }
    throw 'the screen did not close with ESC'
}
function Press-At([double]$X, [double]$Y, [string]$Expect, [string]$Name) {
    $m = Get-LogMark
    Click $X $Y -Relative
    Wait-LogLine $Expect 15 -Since $m | Out-Null
    Start-Sleep -Milliseconds 1000
    if ($Name) { Save-Screenshot $Name | Out-Null }
}

# ---- data-driven form ----
Invoke-Chat '/naturescompass gui ddui'
Open-Screen 'data-driven' 'ddui-open'
Press-At 0.5 0.555 'Select Biome: .* sort by Source' 'ddui-sort'

$m = Get-LogMark
Click 0.5 0.285 -Relative
Start-Sleep -Milliseconds 400
Type-Text 'plains'
Wait-LogLine "Select Biome: .* filter 'plains'" 15 -Since $m | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'ddui-filter' | Out-Null

Move-Mouse 0.5 0.6 -Relative
Scroll -5
Start-Sleep -Milliseconds 800
Save-Screenshot 'ddui-scrolled' | Out-Null
Close-Screen-Logged

# ---- the default: classic form in NC's layout ----
Invoke-Chat '/naturescompass gui auto'
Open-Screen 'classic form' 'classic-open'
Press-At 0.121 0.365 'Select Biome: .* sort by Source' 'classic-sort'
Click 0.375 0.065 -Relative   # the search box opens the filter input
Start-Sleep -Milliseconds 1200
Type-Text 'plains'
Start-Sleep -Milliseconds 400
Save-Screenshot 'classic-filter-input' | Out-Null
Press-At 0.5 0.448 "Select Biome: .* filter 'plains' \(3 listed\)" 'classic-filter'
Press-At 0.6 0.208 'Select Biome: .* selected minecraft:plains' 'classic-selected'
$m = Get-LogMark
Press-At 0.121 0.165 'Select Biome: .* chose Search \(minecraft:plains\)' ''
Wait-LogLine 'BiomeSearchWorker [0-9a-f]{8}: Starting search: ' 15 -Since $m | Out-Null
$done = Wait-LogLine 'BiomeSearchWorker [0-9a-f]{8}: Search (succeeded|failed): ' 120 -Since $m
Write-Step "search: $done"

# Found: the found biome is pre-selected; Search for Next and Teleport are active.
Open-Screen "classic form, selected 'minecraft:plains'" 'classic-found'
Close-Screen-Logged
