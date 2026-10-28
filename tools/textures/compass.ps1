# Nature's Compass item icon and its 32 needle frames (original pixel art, unit U7 `needle`).
#
#   pwsh tools/textures/compass.ps1                       # writes assets/pack/rp/textures/items/naturescompass*.png
#   pwsh tools/textures/compass.ps1 -Preview sheet.png    # also writes an enlarged contact sheet of every frame
#
# The compass: a round rim of bark with a mossy face, two leaf sprigs growing over the rim, a needle
# with a red tip (the target) and a pale birch tail around a wooden pin.
#
# Frame k shows the needle turned (k - 16) / 32 of a turn clockwise from straight up: frame 16 points
# up (the holder faces the target), 00 down, 08 left, 24 right (docs/SPEC.md 3.8). The plain icon
# `naturescompass.png` (what vanilla clients see, index 0 of the `naturescompass` texture array)
# is frame 16.

param([string]$Preview)

. "$PSScriptRoot/lib.ps1"

$root   = Resolve-Path "$PSScriptRoot/../.."
$outDir = Join-Path $root 'assets/pack/rp/textures/items'

$size = 16
$cx   = 7.5 # the pin is pixel (7, 7), so a 1 px needle stays crisp at every angle
$cy   = 7.5

# Palette. o outline, b bark, h bark highlight, d bark shadow, n bark grain, g moss face,
# i face shadow (under the rim), m light moss, v leaf vein / stem, l leaf, e leaf highlight,
# r needle tip, s needle tip shade, w needle tail, c pin.
$bark = '#6e4a2a'
$moss = '#3f7a34'
$pal = @{
    o = '#24170c'
    b = $bark
    h = (Shade-Color $bark 1.35)
    d = (Shade-Color $bark 0.72)
    n = (Shade-Color $bark 0.85)
    g = $moss
    i = (Shade-Color $moss 0.72)
    m = (Shade-Color $moss 1.22)
    v = '#2c5a1e'
    l = '#6fae3e'
    e = '#a6d36a'
    r = '#d23a2c'
    s = '#8f2119'
    w = '#ece6d2'
    c = '#c99a3c'
}

function Get-Base {
    $grid = New-Object 'char[,]' $size, $size
    for ($y = 0; $y -lt $size; $y++) {
        for ($x = 0; $x -lt $size; $x++) {
            $px = $x + 0.5 - $cx; $py = $y + 0.5 - $cy
            $r  = [Math]::Sqrt($px * $px + $py * $py)
            $light = (-$px - $py) / [Math]::Max($r, 0.001) / [Math]::Sqrt(2) # 1 = upper left
            $ch = '.'
            if ($r -le 7.4 -and $r -gt 6.45) { $ch = 'o' }
            elseif ($r -le 6.45 -and $r -gt 5.3) {
                if ($light -gt 0.45) { $ch = 'h' }
                elseif ($light -lt -0.45) { $ch = 'd' }
                elseif ((($x * 3 + $y * 5) % 7) -eq 0) { $ch = 'n' }
                else { $ch = 'b' }
            }
            elseif ($r -le 5.3) {
                if ($r -gt 4.4 -and $light -gt -0.2) { $ch = 'i' }
                elseif ((($x * 5 + $y * 3) % 11) -eq 0 -or ($light -lt -0.5 -and $r -gt 4.0)) { $ch = 'm' }
                else { $ch = 'g' }
            }
            $grid[$y, $x] = $ch
        }
    }
    # Leaf sprigs growing over the rim (top right, bottom left).
    $leaves = @(
        @(12, 0, 'l'), @(13, 0, 'e'), @(11, 1, 'v'), @(12, 1, 'l'), @(13, 1, 'l'), @(14, 1, 'e'),
        @(13, 2, 'v'), @(14, 2, 'l'), @(15, 2, 'l'), @(14, 3, 'l'),
        @(1, 12, 'l'), @(0, 13, 'l'), @(1, 13, 'e'), @(2, 13, 'v'), @(1, 14, 'l'), @(2, 14, 'l'),
        @(3, 14, 'v'), @(2, 15, 'l'), @(3, 15, 'e'), @(4, 15, 'l')
    )
    foreach ($p in $leaves) { $grid[$p[1], $p[0]] = [char]$p[2] }
    return , $grid
}

function Get-Frame([char[,]]$Base, [int]$Frame) {
    $grid  = $Base.Clone()
    $theta = ($Frame - 16) / 32.0 * 2 * [Math]::PI   # clockwise from up
    $dx = [Math]::Sin($theta); $dy = -[Math]::Cos($theta)
    for ($y = 0; $y -lt $size; $y++) {
        for ($x = 0; $x -lt $size; $x++) {
            $px = $x + 0.5 - $cx; $py = $y + 0.5 - $cy
            $along = $px * $dx + $py * $dy
            $cross = $px * $dy - $py * $dx  # > 0: left of the needle
            $perp  = [Math]::Abs($cross)
            if ([Math]::Sqrt($px * $px + $py * $py) -lt 0.5) { $grid[$y, $x] = 'c'; continue }
            # A 1 px band along the needle: one pixel per row / column at any angle.
            if ($perp -gt 0.5) { continue }
            if ($along -gt 0 -and $along -le 4.75) {
                $grid[$y, $x] = if ($along -le 1.6) { 's' } else { 'r' }
            } elseif ($along -lt 0 -and $along -ge -3.75) {
                $grid[$y, $x] = 'w'
            }
        }
    }
    return , $grid
}

function ConvertTo-Rows([char[,]]$Grid) {
    $h = $Grid.GetLength(0); $w = $Grid.GetLength(1)
    $rows = for ($y = 0; $y -lt $h; $y++) { -join (0..($w - 1) | ForEach-Object { $Grid[$y, $_] }) }
    return , $rows
}

$base   = Get-Base
$frames = @{}
for ($k = 0; $k -lt 32; $k++) {
    $frames[$k] = ConvertTo-Rows (Get-Frame $base $k)
    $path = Join-Path $outDir ('naturescompass_{0:d2}.png' -f $k)
    Save-PixelArt -Rows $frames[$k] -Palette $pal -Path $path
}
Save-PixelArt -Rows $frames[16] -Palette $pal -Path (Join-Path $outDir 'naturescompass.png')
Write-Host "wrote $outDir/naturescompass.png and naturescompass_00..31.png"

if ($Preview) {
    # 8 x 4 frames with a 1 px gap, enlarged 8x.
    $sheet = for ($row = 0; $row -lt 4; $row++) {
        for ($y = 0; $y -lt $size; $y++) {
            -join (0..7 | ForEach-Object { $frames[$row * 8 + $_][$y] + '.' })
        }
        ('.' * (8 * ($size + 1)))
    }
    Save-PixelArt -Rows $sheet -Palette $pal -Path $Preview -Scale 8
    Write-Host "wrote $Preview"
}
