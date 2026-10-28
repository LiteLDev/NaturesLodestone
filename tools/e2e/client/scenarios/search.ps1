# U3 search: the search selftests on the integrated server of a local world (the client build
# runs the same workers after the server level's tick).
$mark = Get-LogMark
$r = Invoke-SelfTest 'search.*' -TimeoutSeconds 1200
if ($r.Fail -gt 0) { throw "$($r.Fail) search selftest(s) failed" }
if ($r.Pass -lt 10) { throw "expected at least 10 search selftests to pass, got $($r.Pass)" }
if (-not (Get-ServerLog -Since $mark | Where-Object { $_ -match 'BiomeSearchWorker [0-9a-f]{8}: Search succeeded' })) {
    throw 'no BiomeSearchWorker success line'
}
