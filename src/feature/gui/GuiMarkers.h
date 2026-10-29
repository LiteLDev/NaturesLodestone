#pragma once

// Invisible markers in the classic Select Biome form (formatting codes render as nothing). The
// RP skin (assets/pack/rp/ui/nl_gui_screen.json) finds our form by the title marker and sorts
// the buttons into NC's layout by the button markers; keep both in sync.

#include <string_view>

namespace nl::feature::gui {

inline constexpr std::string_view kMarkTitle       = "§n§c§g§u§i§r"; ///< the Select Biome form
inline constexpr std::string_view kMarkButton      = "§n§c§b§r";     ///< left column, active
inline constexpr std::string_view kMarkButtonOff   = "§n§c§2§r";     ///< left column, inactive
inline constexpr std::string_view kMarkSortPadded  = "§n§c§a§r";     ///< left column, 10 px lower (Sort By under Next)
inline constexpr std::string_view kMarkTeleport    = "§n§c§3§r";     ///< top right, active
inline constexpr std::string_view kMarkTeleportOff = "§n§c§4§r";     ///< top right, inactive
inline constexpr std::string_view kMarkSearchBox   = "§n§c§5§r";     ///< the search box
inline constexpr std::string_view kMarkRow         = "§n§c§6§r";     ///< list row
inline constexpr std::string_view kMarkRowLocked   = "§n§c§7§r";     ///< list row, can't afford
inline constexpr std::string_view kMarkRowSelected = "§n§c§8§r";     ///< list row, selected
inline constexpr std::string_view kMarkCancel      = "§n§c§9§r";     ///< bottom left (Cancel)

} // namespace nl::feature::gui
