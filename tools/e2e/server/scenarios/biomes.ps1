# U2 biomes: the biome catalog (dimensions, tags, sources, XP costs, blacklist) and the biome /
# dimension names in en_US and zh_CN.
$r = Invoke-SelfTest 'biomes.*'
if ($r.Fail -gt 0) { throw "$($r.Fail) biomes selftest(s) failed" }
if ($r.Pass -lt 6) { throw "expected at least 6 biomes selftests to pass, got $($r.Pass)" }
