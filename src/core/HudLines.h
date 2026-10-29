#pragma once

// The Nature's Compass HUD block (docs/SPEC.md section 4.3) as plain data: which text goes on
// which line for a compass state, and the marked string the server sends to players without the
// native overlay (decoded by the RP's hud_screen.json). Game-free, unit-tested in tests/.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nl::core::hud {

/// NC compass states (same numbers as `compass::CompassState`).
enum class State : int {
    Inactive  = 0,
    Searching = 1,
    Found     = 2,
    NotFound  = 3,
};

/// What the HUD shows, already resolved for the viewer: the compass data, the translated biome
/// name (raw, no fixBiomeNames) and the viewer's block position.
struct Input {
    State       state = State::Inactive;
    std::string biomeName;
    int         foundX        = 0;
    int         foundZ        = 0;
    int         searchRadius  = 0;
    int         samples       = 0;
    bool        displayCoords = false;
    int         playerX       = 0; ///< block x of the viewer
    int         playerZ       = 0; ///< block z of the viewer
};

/// The translated labels (`string.naturescompass.<key>`).
struct Labels {
    std::string status      = "Status";
    std::string searching   = "Searching";
    std::string found       = "Found";
    std::string notFound    = "Not Found";
    std::string biome       = "Biome";
    std::string radius      = "Radius";
    std::string coordinates = "Coordinates";
    std::string distance    = "Distance";
    std::string samples     = "Samples";
};

/// Label colour (white) and value colour (grey) of NC's HUD, 0xRRGGBB.
inline constexpr unsigned kLabelColor = 0xFFFFFF;
inline constexpr unsigned kValueColor = 0xAAAAAA;

/// One HUD line: drawn at NC line `index` (before the overlayLineOffset).
struct Line {
    int         index = 0;
    std::string text;
    bool        value = false; ///< grey value (else white label)

    friend bool operator==(Line const&, Line const&) = default;
};

/// NC's HUD lines for `in` (empty for Inactive), in index order.
[[nodiscard]] std::vector<Line> buildLines(Input const& in, Labels const& labels);

/// NC's live distance: (int) sqrt(dx^2 + dz^2) between two block positions (x/z only).
[[nodiscard]] int horizontalDistance(int x1, int z1, int x2, int z2);

/// y (GUI pixels) of the top of line `index`: 5 + (index + offset) * 9 + 2.
[[nodiscard]] constexpr int lineY(int index, int offset) { return 5 + (index + offset) * 9 + 2; }
/// Distance of the block from the screen edge (left x, or right edge - x for right-aligned text).
inline constexpr int kEdgeMargin = 7;

// ---- server fallback (marked title string) -------------------------------------------------------

enum class Side { Left, Right };

/// Markers that start every fallback string. They select the side; the rest of the string is the
/// line block (offset blank rows, then one row per NC line, `§f` labels / `§7` values, '\n'
/// separated). The RP hides marked titles from the vanilla title and shows the block instead.
inline constexpr std::string_view kMarkerLeft  = "§r§N§C§L";
inline constexpr std::string_view kMarkerRight = "§r§N§C§R";

/// The fallback string for `lines` (an empty `lines` gives the bare marker, which clears the block).
[[nodiscard]] std::string encodeFallback(std::vector<Line> const& lines, Side side, int offset);

/// A decoded fallback string (what the RP shows).
struct Decoded {
    Side              side   = Side::Left;
    int               offset = 0; ///< leading blank rows
    std::vector<Line> lines;      ///< non-empty rows, index relative to the offset

    friend bool operator==(Decoded const&, Decoded const&) = default;
};

/// Decodes a fallback string; nullopt if `text` does not start with a marker.
[[nodiscard]] std::optional<Decoded> decodeFallback(std::string_view text);

/// True if `text` contains a fallback marker anywhere (what the RP and the client hook test).
[[nodiscard]] bool isFallback(std::string_view text);

} // namespace nl::core::hud
