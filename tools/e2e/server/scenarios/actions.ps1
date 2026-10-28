# U4 actions: use / sneak reset, broken compasses, XP costs, the durability bar, teleport permission,
# NC's teleport height and teleports into loaded and unloaded columns.
$r = Invoke-SelfTest 'actions.*'
if ($r.Fail -gt 0) { throw "$($r.Fail) actions selftest(s) failed" }
if ($r.Pass -lt 9) { throw "expected at least 9 actions selftests to pass, got $($r.Pass)" }
# The log lines scenarios wait for.
foreach ($pattern in 'Compass reset by ', 'Player \S+ teleported to -?\d+ -?\d+ -?\d+', 'tried to teleport but does not have permission\.') {
    if (-not (Get-ServerLog | Where-Object { $_ -match $pattern })) { throw "missing log line /$pattern/" }
}
