#include "core/HudLines.h"

#include <cmath>

namespace nl::core::hud {

namespace {

constexpr std::string_view kLabelCode = "§f";
constexpr std::string_view kValueCode = "§7";

} // namespace

std::vector<Line> buildLines(Input const& in, Labels const& labels) {
    std::vector<Line> out;
    auto              label = [&](int i, std::string const& text) { out.push_back({i, text, false}); };
    auto              value = [&](int i, std::string text) { out.push_back({i, std::move(text), true}); };
    switch (in.state) {
    case State::Searching:
        label(0, labels.status);
        value(1, labels.searching);
        label(3, labels.biome);
        value(4, in.biomeName);
        label(6, labels.radius);
        value(7, std::to_string(in.searchRadius));
        break;
    case State::Found:
        label(0, labels.status);
        value(1, labels.found);
        label(3, labels.biome);
        value(4, in.biomeName);
        if (in.displayCoords) {
            label(6, labels.coordinates);
            value(7, std::to_string(in.foundX) + ", " + std::to_string(in.foundZ));
            label(9, labels.distance);
            value(10, std::to_string(horizontalDistance(in.playerX, in.playerZ, in.foundX, in.foundZ)));
        }
        break;
    case State::NotFound:
        label(0, labels.status);
        value(1, labels.notFound);
        label(3, labels.biome);
        value(4, in.biomeName);
        label(6, labels.radius);
        value(7, std::to_string(in.searchRadius));
        label(9, labels.samples);
        value(10, std::to_string(in.samples));
        break;
    case State::Inactive:
    default:
        break;
    }
    return out;
}

int horizontalDistance(int x1, int z1, int x2, int z2) {
    double dx = static_cast<double>(x2) - x1;
    double dz = static_cast<double>(z2) - z1;
    return static_cast<int>(std::sqrt(dx * dx + dz * dz));
}

std::string encodeFallback(std::vector<Line> const& lines, Side side, int offset) {
    std::string out(side == Side::Right ? kMarkerRight : kMarkerLeft);
    if (lines.empty()) return out;
    std::vector<Line const*> rows;
    for (auto const& l : lines) {
        if (l.index < 0) continue;
        if (static_cast<size_t>(l.index) >= rows.size()) rows.resize(static_cast<size_t>(l.index) + 1, nullptr);
        rows[static_cast<size_t>(l.index)] = &l;
    }
    out.append(static_cast<size_t>(offset < 0 ? 0 : offset), '\n');
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i > 0) out += '\n';
        if (rows[i]) {
            out += rows[i]->value ? kValueCode : kLabelCode;
            out += rows[i]->text;
        }
    }
    return out;
}

std::optional<Decoded> decodeFallback(std::string_view text) {
    Decoded d;
    if (text.starts_with(kMarkerLeft)) {
        d.side = Side::Left;
        text.remove_prefix(kMarkerLeft.size());
    } else if (text.starts_with(kMarkerRight)) {
        d.side = Side::Right;
        text.remove_prefix(kMarkerRight.size());
    } else {
        return std::nullopt;
    }
    if (text.empty()) return d;
    std::vector<std::string_view> rows;
    for (size_t start = 0;;) {
        size_t nl = text.find('\n', start);
        rows.push_back(text.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start));
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    size_t first = 0;
    while (first < rows.size() && rows[first].empty()) ++first;
    d.offset = static_cast<int>(first);
    for (size_t i = first; i < rows.size(); ++i) {
        auto row = rows[i];
        if (row.empty()) continue;
        Line l;
        l.index = static_cast<int>(i - first);
        if (row.starts_with(kValueCode)) {
            l.value = true;
            row.remove_prefix(kValueCode.size());
        } else if (row.starts_with(kLabelCode)) {
            row.remove_prefix(kLabelCode.size());
        }
        l.text = std::string(row);
        d.lines.push_back(std::move(l));
    }
    return d;
}

bool isFallback(std::string_view text) {
    return text.find(kMarkerLeft) != std::string_view::npos || text.find(kMarkerRight) != std::string_view::npos;
}

} // namespace nl::core::hud
