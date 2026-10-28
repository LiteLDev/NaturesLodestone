# U6 hud: NC's HUD block for the held compass, in every state, on both sides, with chat open and
# hidden behind other screens. Local / remote-mod: the client build's native overlay (the server's
# marked fallback title is dropped). remote-vanilla: the server fallback rendered by the RP.
# States are set with the debug sub-command `/naturescompass hud <state>`.
# Screenshots: out/hud/<state>-<side>.png, chat-open.png, chat-hidden.png, inventory.png, offset3.png
$modded = $script:Mode -ne 'remote-vanilla'

function Set-HudState([string]$state) {
    Invoke-Chat "/naturescompass hud $state"
    Wait-LogLine "HUD debug state $state for" 15 | Out-Null
    Start-Sleep -Milliseconds 1500
}

Invoke-Chat '/gamerule sendcommandfeedback false'
Invoke-Chat '/clear @s'
Invoke-Chat '/give @s natureslodestone:naturescompass'
Start-Sleep -Milliseconds 4000           # the "given" chat line and item name popup fade
Save-Screenshot 'inactive' | Out-Null    # nothing drawn for an Inactive compass

Set-HudState 'searching'
if ($modded) {
    Wait-LogLine 'HUD overlay drawn' 15 | Out-Null
}
Start-Sleep -Milliseconds 6000           # chat lines of the typed commands fade (they sit top-left)
Save-Screenshot 'searching-left' | Out-Null
Set-HudState 'found'
Start-Sleep -Milliseconds 6000
Save-Screenshot 'found-left' | Out-Null
Set-HudState 'notfound'
Start-Sleep -Milliseconds 6000
Save-Screenshot 'notfound-left' | Out-Null

# Chat open: still drawn (displayWithChatOpen = true by default).
Focus-Game; Press-Key T; Start-Sleep -Milliseconds 1200
Save-Screenshot 'chat-open' | Out-Null
Press-Key ESC; Start-Sleep -Milliseconds 800

# Any other screen hides it (native overlay).
Open-Inventory
Save-Screenshot 'inventory' | Out-Null
Close-Screen

Invoke-Chat '/naturescompass hud side RIGHT'
Wait-LogLine 'HUD debug overlaySide RIGHT' 15 | Out-Null
Start-Sleep -Milliseconds 6000
Save-Screenshot 'notfound-right' | Out-Null
Set-HudState 'found'
Start-Sleep -Milliseconds 6000
Save-Screenshot 'found-right' | Out-Null
Set-HudState 'searching'
Start-Sleep -Milliseconds 6000
Save-Screenshot 'searching-right' | Out-Null

Invoke-Chat '/naturescompass hud offset 3'
Wait-LogLine 'HUD debug overlayLineOffset 3' 15 | Out-Null
Set-HudState 'foundnocoords'
Start-Sleep -Milliseconds 6000
Save-Screenshot 'offset3' | Out-Null

Invoke-Chat '/naturescompass hud chat off'
Wait-LogLine 'HUD debug displayWithChatOpen false' 15 | Out-Null
Start-Sleep -Milliseconds 6000
Focus-Game; Press-Key T; Start-Sleep -Milliseconds 1200
Save-Screenshot 'chat-hidden' | Out-Null   # native overlay: hidden with chat open
Press-Key ESC; Start-Sleep -Milliseconds 800

Set-HudState 'inactive'
Start-Sleep -Milliseconds 6000
Save-Screenshot 'cleared' | Out-Null

if ($modded) {
    # The server sent marked titles; the modded client dropped them (no double block).
    Wait-LogLine 'HUD fallback title dropped' 5 | Out-Null
}

$r = Invoke-SelfTest 'hud.*'
if ($r.Fail -gt 0) { throw "$($r.Fail) hud selftest(s) failed" }
if ($r.Pass -lt 7) { throw "expected at least 7 hud selftests to pass, got $($r.Pass)" }
