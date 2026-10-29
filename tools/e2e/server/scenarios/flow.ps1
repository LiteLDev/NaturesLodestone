# U8 flow: the units working together on one compass in a DEFAULT world (flow.prelaunch.ps1):
# Search for a desert (XP), Found on the HUD, Search for Next (a separate desert), durability
# breaking after two successes, the safe teleport, sneak reset, XP in survival vs creative and the
# repair recipe. The selftests drive the contracts; this checks the NC log lines they produce.
$mark = Get-LogMark
$r = Invoke-SelfTest 'showcase.*' -TimeoutSeconds 1200
if ($r.Fail -gt 0) { throw "$($r.Fail) showcase selftest(s) failed" }
if ($r.Pass -lt 3) { throw "expected at least 3 showcase selftests to pass, got $($r.Pass)" }
$log = Get-ServerLog -Since $mark
if (-not ($log | Where-Object { $_ -match 'showcase\.flow_real_biomes: next desert at -?\d+, -?\d+' })) {
    throw 'showcase.flow_real_biomes did not run the real-biome flow'
}
foreach ($what in 'BiomeSearchWorker [0-9a-f]{8}: Starting search: 64 sample space, 50000 max samples, 10000 max radius, 0 previous locations',
                  'BiomeSearchWorker [0-9a-f]{8}: Starting search: 64 sample space, 50000 max samples, 10000 max radius, 1 previous locations',
                  'BiomeSearchWorker [0-9a-f]{8}: Search succeeded: \d+ radius, \d+ samples',
                  'Player \S+ teleported to -?\d+ -?\d+ -?\d+',
                  'Compass reset by ') {
    if (-not ($log | Where-Object { $_ -match $what })) { throw "missing log line /$what/" }
}
