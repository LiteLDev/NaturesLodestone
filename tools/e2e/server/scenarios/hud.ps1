# U6 hud: the HUD line model on real compasses, the fallback string (marker, side, offset) and
# its change-only sending (nothing for Inactive).
$r = Invoke-SelfTest 'hud.*'
if ($r.Fail -gt 0) { throw "$($r.Fail) hud selftest(s) failed" }
if ($r.Pass -lt 7) { throw "expected at least 7 hud selftests to pass, got $($r.Pass)" }
