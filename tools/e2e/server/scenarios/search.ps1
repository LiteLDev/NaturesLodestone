# U3 search: biome sampling in all three dimensions, the spiral limits, stop, FIFO, and real
# overworld searches (plains, desert, ocean, forest, Search for Next) in a DEFAULT world
# (search.prelaunch.ps1). Every search logs NC's BiomeSearchWorker lines.
$mark = Get-LogMark
$r = Invoke-SelfTest 'search.*' -TimeoutSeconds 1200
if ($r.Fail -gt 0) { throw "$($r.Fail) search selftest(s) failed" }
if ($r.Pass -lt 10) { throw "expected at least 10 search selftests to pass, got $($r.Pass)" }
$log = Get-ServerLog -Since $mark
foreach ($what in 'Starting search: \d+ sample space, \d+ max samples, \d+ max radius, \d+ previous locations',
                  'Search succeeded: \d+ radius, \d+ samples',
                  'Search failed: \d+ radius, \d+ samples',
                  'Search stopped: \d+ radius, \d+ samples') {
    if (-not ($log | Where-Object { $_ -match "BiomeSearchWorker [0-9a-f]{8}: $what" })) { throw "no BiomeSearchWorker line '$what'" }
}
# The overworld searches really ran (not skipped as on a flat world).
if (-not ($log | Where-Object { $_ -match 'search.overworld_biomes.*next minecraft:' })) {
    throw 'search.overworld_biomes did not search the overworld'
}
