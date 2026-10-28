# U1 compass: the item, its data on real stacks, held compasses, config, and the default use flow.
$r = Invoke-SelfTest 'compass.*'
if ($r.Fail -gt 0) { throw "$($r.Fail) compass selftest(s) failed" }
if ($r.Pass -lt 6) { throw "expected at least 6 compass selftests to pass, got $($r.Pass)" }
