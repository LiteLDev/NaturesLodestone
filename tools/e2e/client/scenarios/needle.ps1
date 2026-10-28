# U7 needle: the held Nature's Compass needle turns with the player's yaw and points at the found
# position (state Found) or the world spawn (other states); a dropped compass spins.
# Screenshots: out/needle/{found-north,found-south,found-west,found-east,spawn-east,dropped-1,dropped-2}.png
#
# Frame k of the needle is (k - 16) / 32 of a turn clockwise from straight up: facing the target
# shows frame 16 (up), target behind 0 (down), on the left 8, on the right 24. Yaw: 0 south,
# 90 west, 180 north, 270 east.

$r = Invoke-SelfTest 'needle.*'
if ($r.Fail -gt 0) { throw "$($r.Fail) needle selftest(s) failed" }
if ($r.Pass -lt 4) { throw "expected at least 4 needle selftests to pass, got $($r.Pass)" }

Invoke-Chat '/gamemode creative'
Invoke-Chat '/time set noon'
Invoke-Chat '/weather clear'
Invoke-Chat '/clear @s'
Invoke-Chat '/give @s natureslodestone:naturescompass'
Press-Key 1
Start-Sleep -Milliseconds 300

# Reads the needle frame the client computes for the held compass (after the wobble settled).
function Get-NeedleFrame {
    $mark = Get-LogMark
    Invoke-Chat '/naturescompass needle frame'
    $line = Wait-LogLine 'needle: frame (\d+)' 20 -Since $mark
    if ($line -notmatch 'needle: frame (\d+)') { throw "no needle frame in '$line'" }
    return [int]$Matches[1]
}

function Test-Needle([string]$name, [int]$yaw, [int]$expected) {
    Invoke-Chat "/tp @s 0.5 ~ 0.5 $yaw 20"
    Start-Sleep -Milliseconds 3000   # let the wobble settle
    Save-Screenshot $name | Out-Null
    $frame = Get-NeedleFrame
    Write-Step "needle $name (yaw $yaw): frame $frame, expected $expected"
    # The wobble settles within a frame of the exact angle.
    $diff = [Math]::Abs((($frame - $expected + 48) % 32) - 16)
    if ($diff -gt 1) { throw "needle $name (yaw $yaw): frame $frame, expected $expected" }
}

# Found far to the north of x/z 0.5.
$mark = Get-LogMark
Invoke-Chat '/naturescompass needle found 0 -2000'
Wait-LogLine 'needle: .* compass Found at 0, -2000' 20 -Since $mark | Out-Null
Test-Needle 'found-north' 180 16
Test-Needle 'found-south' 0 0
Test-Needle 'found-west' 90 24
Test-Needle 'found-east' 270 8

# A fresh compass (Inactive) points at the world spawn, here far to the east.
Invoke-Chat '/setworldspawn 2000 64 0'
Invoke-Chat '/clear @s'
Invoke-Chat '/give @s natureslodestone:naturescompass'
Press-Key 1
Test-Needle 'spawn-east' 180 24

# Dropped on the ground: no living holder, the needle spins.
Invoke-Chat "/tp @s 0.5 ~ 0.5 180 60"
Press-Key Q
Start-Sleep -Milliseconds 1500
Save-Screenshot 'dropped-1' | Out-Null
Start-Sleep -Milliseconds 700
Save-Screenshot 'dropped-2' | Out-Null
