# Nature's Compass item icon (original pixel art).
#
# PLACEHOLDER (unit U1): one static 16x16 icon so the item is visible. Unit U7 `needle` replaces
# this script with the 32 animated needle frames and owns it from then on, together with
# assets/pack/rp/item_texture.d/needle.json (texture key `naturescompass`).
#
#   pwsh tools/textures/compass.ps1

. "$PSScriptRoot/lib.ps1"

$root = Resolve-Path "$PSScriptRoot/../.."
$out  = Join-Path $root 'assets/pack/rp/textures/items/naturescompass.png'

# o outline (dark bark), b bark rim, h rim highlight, g moss face, m light moss, l leaf,
# r needle tip (north), w needle tail, c centre pin
$rows = @(
    '................',
    '.....oooooo.....',
    '...oobbhhbboo...',
    '..obhggmmggbbo..',
    '..obgmgllgmgbo..',
    '.obgmggrrggmgbo.',
    '.obggggrrggggbo.',
    '.obglggrrgglgbo.',
    '.obglggccgglgbo.',
    '.obggggwwggggbo.',
    '.obgmggwwggmgbo.',
    '..obgmgllgmgbo..',
    '..obbgmmmmgbbo..',
    '...oobbbbbboo...',
    '.....oooooo.....',
    '................'
)
$pal = @{
    o = '#2b1d10'
    b = '#6b4a2b'
    h = '#9a7148'
    g = '#3f7a34'
    m = '#5f9e47'
    l = '#8cc263'
    r = '#c8312b'
    w = '#e8e4d8'
    c = '#1c1c1c'
}
Save-PixelArt -Rows $rows -Palette $pal -Path $out
Write-Host "wrote $out"
