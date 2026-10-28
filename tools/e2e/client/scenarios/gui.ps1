# U5 gui: the Select Biome screen on a real client (data-driven form first, then the classic
# form with the RP skin). Opens it with the compass (right click), filters, cycles the sort,
# selects a biome and presses Search; every step waits for the server's log line.
# Screenshots: out/gui/*.png
Invoke-Chat '/clear @s'
Invoke-Chat '/xp 100L @s'
Invoke-Chat '/give @s natureslodestone:naturescompass'
Start-Sleep -Milliseconds 500

# ---- data-driven form ----
$m = Get-LogMark
Click 0.5 0.5 -Relative -Right
Wait-LogLine 'Select Biome opened for .*data-driven' 30 -Since $m | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'ddui-open' | Out-Null

$m = Get-LogMark
Click 0.5 0.555 -Relative
Wait-LogLine 'Select Biome: .* sort by Source' 15 -Since $m | Out-Null
Start-Sleep -Milliseconds 600
Save-Screenshot 'ddui-sort' | Out-Null

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
Press-Key ESCAPE; Start-Sleep -Milliseconds 800
Press-Key ESCAPE; Start-Sleep -Milliseconds 800

# ---- classic form + RP skin ----
$m = Get-LogMark
Invoke-Chat '/naturescompass gui classic'
Click 0.5 0.5 -Relative -Right
Wait-LogLine 'Select Biome opened for .*classic' 30 -Since $m | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'classic-open' | Out-Null
Press-Key ESCAPE; Start-Sleep -Milliseconds 800
Invoke-Chat '/naturescompass gui auto'
