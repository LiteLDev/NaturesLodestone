# U1 compass: the item exists on the client with its icon and name, and the compass selftests
# pass on the integrated server.
# Screenshots: out/compass/{given,inventory}.png
Invoke-Chat '/clear @s'
Invoke-Chat '/give @s natureslodestone:naturescompass'
Start-Sleep -Milliseconds 300
Save-Screenshot 'given' | Out-Null       # hotbar + the item name popup above it
Press-Key E; Start-Sleep -Milliseconds 1200
Save-Screenshot 'inventory' | Out-Null
Press-Key ESCAPE; Start-Sleep -Milliseconds 600
$r = Invoke-SelfTest 'compass.*'
if ($r.Fail -gt 0) { throw "$($r.Fail) compass selftest(s) failed" }
if ($r.Pass -lt 6) { throw "expected at least 6 compass selftests to pass, got $($r.Pass)" }
