# U4 actions in the real client (local world, compassDurability 4 from actions.prelaunch.ps1):
# right-click opens Select Biome once per click, sneak + right-click resets, the durability bar,
# a broken compass does nothing; then the actions selftests on the integrated server.
# Screenshots: out/actions/{select-biome,durability-1of4,durability-3of4,durability-broken}.png
function Get-LinesSince([int]$Mark, [string]$Pattern) { @(Get-ServerLog -Since $Mark | Where-Object { $_ -match $Pattern }) }

function Use-Compass([switch]$Sneak) {
    if ($Sneak) { [BsE2E.Native]::Key([uint16]0x10, $false); Start-Sleep -Milliseconds 400 }
    Click 0.5 0.5 -Relative -Right
    if ($Sneak) { Start-Sleep -Milliseconds 300; [BsE2E.Native]::Key([uint16]0x10, $true) }
}

Invoke-Chat '/clear @s'
Invoke-Chat '/give @s natureslodestone:naturescompass'
Start-Sleep -Milliseconds 800
Save-Screenshot 'given' | Out-Null

# right click: Select Biome, once
$mark = Get-LogMark
Use-Compass
Wait-LogLine 'Compass used by .*: Select Biome' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'select-biome' | Out-Null
$opens = (Get-LinesSince $mark 'Compass used by .*: Select Biome').Count
if ($opens -ne 1) { throw "one right click opened the screen $opens times" }
Close-Screen
Start-Sleep -Milliseconds 500

# sneak + right click: reset
$mark = Get-LogMark
Use-Compass -Sneak
Wait-LogLine 'Compass reset by ' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 500
if ((Get-LinesSince $mark 'Compass used by').Count -ne 0) { throw 'a sneaking use opened the screen' }

# durability bar: 1 of 4, 3 of 4, broken
foreach ($d in 1, 3, 4) {
    $mark = Get-LogMark
    Invoke-Chat "/naturescompass damage $d"
    Wait-LogLine "Compass damage of .* set to $d" 15 -Since $mark | Out-Null
    Start-Sleep -Milliseconds 800
    $name = if ($d -ge 4) { 'durability-broken' } else { "durability-$($d)of4" }
    Save-Screenshot $name | Out-Null
}

# a broken compass does not open the screen; sneak-use still resets it
$mark = Get-LogMark
Use-Compass
Start-Sleep -Milliseconds 1500
if ((Get-LinesSince $mark 'Compass used by').Count -ne 0) { throw 'a broken compass opened the screen' }
$mark = Get-LogMark
Use-Compass -Sneak
Wait-LogLine 'Compass reset by ' 15 -Since $mark | Out-Null

$r = Invoke-SelfTest 'actions.*'
if ($r.Fail -gt 0) { throw "$($r.Fail) actions selftest(s) failed" }
if ($r.Pass -lt 9) { throw "expected at least 9 actions selftests to pass, got $($r.Pass)" }
