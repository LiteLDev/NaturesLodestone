# U8 showcase: re-shoots docs/showcase/ (the LeviMC Mods gallery, showcase.json) in a local
# world. Plays the flow scenario with frame bursts on, then encodes with ffmpeg (on PATH):
# pictures as webp, clips as mp4 (+ a webp poster) and as animated webp (for the README).
#   pwsh -NoProfile -File tools/e2e/client/run.ps1 -Scenario showcase
# Review the files (and their captions in scenarios/showcase.json, copied next to them) before
# committing them, then add `<img src="docs/showcase/search.webp">` to the README.
$script:FlowFrames = $true
. (Join-Path $PSScriptRoot 'flow.ps1')

$ffmpeg = (Get-Command ffmpeg -ErrorAction SilentlyContinue).Source
if (-not $ffmpeg) { throw 'ffmpeg is not on PATH (winget install Gyan.FFmpeg)' }
$src  = Join-Path $script:OutRoot $script:Unit
$docs = Join-Path $script:RepoRoot 'docs/showcase'
New-Item -ItemType Directory -Force $docs | Out-Null

function Invoke-Ffmpeg([string[]]$ArgList) {
    & $ffmpeg -hide_banner -loglevel error -y @ArgList
    if ($LASTEXITCODE) { throw "ffmpeg failed: $($ArgList -join ' ')" }
}

# A still picture.
function Export-Picture([string]$Png, [string]$Name) {
    Invoke-Ffmpeg @('-i', (Join-Path $src "$Png.png"), '-c:v', 'libwebp', '-quality', '85', (Join-Path $docs "$Name.webp"))
    Write-Step "showcase: $Name.webp"
}

# A clip from (png, seconds) pairs: <name>.mp4, <name>-poster.webp (its last frame) and <name>.webp.
function Export-Clip([string]$Name, [object[]]$Frames, [int]$WebpWidth = 960) {
    $list = Join-Path $src "$Name.ffconcat"
    $lines = @('ffconcat version 1.0')
    foreach ($f in $Frames) { $lines += "file '$($f[0] -replace '\\', '/')'"; $lines += "duration $($f[1])" }
    $lines += "file '$($Frames[-1][0] -replace '\\', '/')'"   # the concat demuxer drops the last duration otherwise
    Set-Content -Path $list -Value $lines -Encoding ascii
    Invoke-Ffmpeg @('-f', 'concat', '-safe', '0', '-i', $list, '-vf', 'fps=30,format=yuv420p', '-c:v', 'libx264',
        '-crf', '23', '-preset', 'slow', '-movflags', '+faststart', (Join-Path $docs "$Name.mp4"))
    Invoke-Ffmpeg @('-f', 'concat', '-safe', '0', '-i', $list, '-vf', "fps=10,scale=$($WebpWidth):-2:flags=lanczos",
        '-c:v', 'libwebp_anim', '-loop', '0', '-quality', '70', (Join-Path $docs "$Name.webp"))
    Invoke-Ffmpeg @('-i', $Frames[-1][0], '-c:v', 'libwebp', '-quality', '85', (Join-Path $docs "$Name-poster.webp"))
    Write-Step "showcase: $Name.mp4 / $Name.webp ($($Frames.Count) frames)"
}

function Get-Frames([string]$Clip) { @(Get-ChildItem $src -Filter "frame-$Clip-*.png" | Sort-Object Name | ForEach-Object FullName) }
function Get-Png([string]$Name) { Join-Path $src "$Name.png" }

Export-Picture 'gui-open' 'select-biome'
Export-Picture 'gui-selected' 'select-biome-desert'
Export-Picture 'hud-found' 'hud-found'
Export-Picture 'hud-found-next' 'hud-found-next'
Export-Picture 'teleported' 'teleported'

# Search: the screen (open, filter, sort, select), then the HUD while searching until Found.
$frames = @(@((Get-Png 'gui-open'), 1.2), @((Get-Png 'gui-filter'), 1.0), @((Get-Png 'gui-sort'), 0.8), @((Get-Png 'gui-selected'), 1.2))
foreach ($f in Get-Frames 'search') { $frames += , @($f, 0.25) }
$frames += , @((Get-Png 'hud-found'), 2.5)
Export-Clip 'search' $frames

# Needle: the player turns a full circle; the needle keeps pointing at the desert.
$frames = @()
foreach ($f in Get-Frames 'needle') { $frames += , @($f, 0.1) }
Export-Clip 'needle' $frames

# Teleport: Select Biome -> Teleport -> standing in the desert, distance 0.
$frames = @(, @((Get-Png 'gui-found'), 1.2))
foreach ($f in Get-Frames 'teleport') { $frames += , @($f, 0.15) }
$frames += , @((Get-Png 'teleported'), 2.0)
Export-Clip 'teleport' $frames

# The gallery index (titles and captions in en and zh-CN) goes with the media.
Copy-Item -Force (Join-Path $PSScriptRoot 'showcase.json') (Join-Path $docs 'showcase.json')
Write-Step "showcase: written to $docs"
