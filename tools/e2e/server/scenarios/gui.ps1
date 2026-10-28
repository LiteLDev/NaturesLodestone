# U5 gui: the Select Biome screen's rows, texts, filter, sort, selection and buttons, driven
# through the form handlers on a SimulatedPlayer (data-driven and classic forms).
$r = Invoke-SelfTest 'gui.*'
if ($r.Fail -gt 0) { throw "$($r.Fail) gui selftest(s) failed" }
if ($r.Pass -lt 7) { throw "expected at least 7 gui selftests to pass, got $($r.Pass)" }
