# U2 biomes: the biome catalog is built for the local world on the integrated server, and the
# biome / dimension names load in en_US and zh_CN (the client build ships the same lang files).
$r = Invoke-SelfTest 'biomes.*'
if ($r.Fail -gt 0) { throw "$($r.Fail) biomes selftest(s) failed" }
if ($r.Pass -lt 6) { throw "expected at least 6 biomes selftests to pass, got $($r.Pass)" }
$catalog = Get-LeviLog | Where-Object { $_ -match 'Biome catalog: (\d+) searchable biomes' } | Select-Object -Last 1
if (-not $catalog) { throw 'no "Biome catalog" log line' }
if ([int]([regex]::Match($catalog, 'Biome catalog: (\d+)').Groups[1].Value) -lt 50) { throw "catalog too small: $catalog" }
