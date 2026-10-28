// The Select Biome screen (src/feature/compass/Gui.h, docs/SPEC.md section 4.2).
//
// The state of an open screen is a `core::SelectBiomeModel` (filter, sort, selection, button
// rules) in a per-player `Session`. Two front-ends draw it:
//
// - the data-driven `ll::ui::CustomForm` (preferred): one form whose texts, visibility and
//   enabled flags are observables, so the filter (live, on every edit the client reports), the
//   sort button and the selection update in place without closing the screen;
// - the classic `ll::form::SimpleForm` (fallback when the data-driven form can't be shown, or
//   forced with `/naturescompass gui classic`): rebuilt and re-sent after every click; its title
//   and buttons carry invisible markers that the RP skin (`ui/nl_gui_screen.json`) uses to draw
//   NC's two-column layout.
//
// Buttons never keep a `Player*`: callbacks look the player up again by UUID.

#include "feature/compass/Gui.h"

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/form/CustomForm.h"
#include "ll/api/form/SimpleForm.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/ui/form/CustomForm.h"

#include "mc/platform/UUID.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include "core/SelectBiome.h"
#include "feature/compass/Actions.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "feature/gui/GuiMarkers.h"
#include "feature/gui/GuiText.h"
#include "game/WorldDb.h"
#include "mod/Commands.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace nl::feature::gui {

namespace {

using core::SelectBiomeModel;

struct DduiSlot {
    ll::ui::ObservableString  label{""};
    ll::ui::ObservableString  details{""};
    ll::ui::ObservableBoolean visible{false};
    ll::ui::ObservableBoolean disabled{false};
};

/// One open Select Biome screen.
struct Session {
    Session(Player const& player, std::string locale_, SelectBiomeModel model_)
    : uuid(player.getUuid()),
      playerName(player.getRealName()),
      locale(std::move(locale_)),
      model(std::move(model_)),
      maxNextSearches(compass::maxNextSearches()),
      canTeleport(actions::canTeleport(player)) {}

    mce::UUID        uuid;
    std::string      playerName;
    std::string      locale;
    SelectBiomeModel model;
    int              maxNextSearches;
    bool             canTeleport;
    bool             dataDriven = false;
    bool             closed     = false;

    // Data-driven form.
    std::optional<ll::ui::CustomForm> form;
    ll::ui::ObservableString          filter{"", ll::ui::ObservableOptions{true}};
    ll::ui::ObservableString          sortLabel{""};
    ll::ui::ObservableBoolean         searchDisabled{true};
    ll::ui::ObservableBoolean         nextDisabled{true};
    ll::ui::ObservableBoolean         teleportDisabled{true};
    std::vector<DduiSlot>             slots;
};

using SessionPtr = std::shared_ptr<Session>;

std::unordered_map<std::string, SessionPtr> gSessions;

std::string keyOf(mce::UUID const& uuid) { return uuid.asString(); }

Player* findPlayer(mce::UUID const& uuid) {
    auto level = ll::service::getLevel();
    return level ? level->getPlayer(uuid) : nullptr;
}

SessionPtr sessionOf(Player const& player) {
    auto it = gSessions.find(keyOf(player.getUuid()));
    return it == gSessions.end() || it->second->closed ? nullptr : it->second;
}

std::string tx(Session const& s, std::string_view key, std::string_view fallback) {
    return text(s.locale, key, fallback);
}

/// Drops the session (later, never inside one of its own callbacks).
void endSession(SessionPtr const& session) {
    if (session->closed) return;
    session->closed = true;
    ll::thread::ServerThreadExecutor::getDefault().execute([session] {
        auto it = gSessions.find(keyOf(session->uuid));
        if (it != gSessions.end() && it->second == session) gSessions.erase(it);
    });
}

void logFilter(Session const& s) {
    auto const& f = s.model.filter();
    log().info(
        "Select Biome: {} filter '{}' ({} listed)",
        s.playerName,
        f.size() > 64 ? f.substr(0, 64) + "..." : f,
        s.model.listed().size()
    );
}

void logSort(Session const& s) {
    log().info("Select Biome: {} sort by {}", s.playerName, core::sortModeEnglish(s.model.sortMode()));
}

void logSelected(Session const& s) {
    log().info("Select Biome: {} selected {}", s.playerName, s.model.selected().value_or(""));
}

// ---- data-driven form: drawing ---------------------------------------------------------------

/// The row button: the name (line 1), the XP cost, and the selection mark.
std::string dduiRowButton(Session const& s, core::BiomeRow const& row, std::array<std::string, 4> const& lines) {
    std::string out = lines[0];
    if (row.xpLevels > 0) out += " [" + std::to_string(row.xpLevels) + "]";
    if (s.model.isSelected(row)) out = "> " + out + " <";
    return out;
}

/// The label under the row button: lines 2-4.
std::string dduiRowDetails(std::array<std::string, 4> const& lines) {
    return "§7" + lines[1] + "\n" + lines[2] + "\n" + lines[3];
}

void refreshDdui(Session& s) {
    auto b = s.model.buttons(s.maxNextSearches, s.canTeleport);
    s.searchDisabled.setData(!b.searchActive);
    s.nextDisabled.setData(!b.nextActive);
    s.teleportDisabled.setData(!b.teleportActive);
    s.sortLabel.setData(sortButtonText(s.model.sortMode(), s.locale));
    for (std::size_t i = 0; i < s.slots.size(); ++i) {
        auto& slot = s.slots[i];
        if (auto const* row = s.model.listedRow(i)) {
            auto lines = rowLines(*row, s.model.sortMode(), s.locale);
            slot.label.setData(dduiRowButton(s, *row, lines));
            slot.details.setData(dduiRowDetails(lines));
            slot.disabled.setData(!row->affordable);
            slot.visible.setData(true);
        } else {
            slot.visible.setData(false);
        }
    }
}

void showClassic(Player& player, SessionPtr const& s);

/// After a change that keeps the screen open: update it in place, or re-send the classic form.
void redraw(Player& player, SessionPtr const& s) {
    if (s->closed) return;
    if (s->dataDriven) refreshDdui(*s);
    else showClassic(player, s);
}

void closeScreen(SessionPtr const& s) {
    bool wasOpen = !s->closed;
    endSession(s); // marks it closed first: close() may run the show callback right away
    if (wasOpen && s->dataDriven && s->form) (void)s->form->close();
}

// ---- handlers shared by both forms (and the selftest hooks) ----------------------------------

enum class Choice { Search, SearchForNext, Teleport };

std::string_view choiceName(Choice c) {
    switch (c) {
    case Choice::Search:
        return "Search";
    case Choice::SearchForNext:
        return "Search for Next";
    default:
        return "Teleport";
    }
}

bool choiceActive(Session const& s, Choice choice) {
    auto b = s.model.buttons(s.maxNextSearches, s.canTeleport);
    switch (choice) {
    case Choice::Search:
        return b.searchActive;
    case Choice::SearchForNext:
        return b.nextActive;
    default:
        return b.teleportActive;
    }
}

/// Search / Search for Next / Teleport: the action, then the screen closes (NC). An inactive
/// button does nothing.
void handleChoice(Player& player, SessionPtr const& s, Choice choice) {
    if (!choiceActive(*s, choice)) return redraw(player, s);
    auto biome = s->model.selected().value_or("");
    closeScreen(s);
    log().info("Select Biome: {} chose {} ({})", s->playerName, choiceName(choice), biome);
    switch (choice) {
    case Choice::Search:
        actions::search(player, biome);
        break;
    case Choice::SearchForNext:
        actions::searchForNext(player);
        break;
    case Choice::Teleport:
        actions::teleport(player);
        break;
    }
}

void handleCancel(SessionPtr const& s) {
    log().info("Select Biome: {} chose Cancel", s->playerName);
    closeScreen(s);
}

void handleSort(Player& player, SessionPtr const& s) {
    s->model.cycleSort();
    logSort(*s);
    redraw(player, s);
}

/// A click on a listed row: select it; a second click on the selected row searches (NC double
/// click); rows the player can't afford do nothing.
void handleRow(Player& player, SessionPtr const& s, std::size_t position) {
    switch (s->model.click(position)) {
    case SelectBiomeModel::Click::Search:
        return handleChoice(player, s, Choice::Search);
    case SelectBiomeModel::Click::Selected:
        logSelected(*s);
        break;
    default:
        break;
    }
    redraw(player, s);
}

/// The filter changed. Returns true when the list was rebuilt.
bool applyFilter(Session& s, std::string const& value) {
    if (!s.model.setFilter(value)) return false;
    if (s.dataDriven) refreshDdui(s);
    logFilter(s);
    return true;
}

// ---- data-driven form ------------------------------------------------------------------------

/// Wraps a button handler: finds the session and its player again.
template <class F>
ll::ui::CustomForm::ButtonCallback onDdui(std::weak_ptr<Session> weak, F fn) {
    return [weak = std::move(weak), fn = std::move(fn)]() mutable {
        auto s = weak.lock();
        if (!s || s->closed) return;
        auto* player = findPlayer(s->uuid);
        if (!player) return;
        fn(*player, s);
    };
}

bool showDdui(Player& player, SessionPtr const& s) {
    std::weak_ptr<Session> weak = s;
    auto                   b    = s->model.buttons(s->maxNextSearches, s->canTeleport);
    s->dataDriven               = true;

    ll::ui::CustomForm form(player, tx(*s, "string.naturescompass.selectBiome", "Select Biome"));
    form.textField(tx(*s, "string.naturescompass.search", "Search"), s->filter);
    form.button(
        tx(*s, "string.naturescompass.search", "Search"),
        onDdui(weak, [](Player& p, SessionPtr const& s) { handleChoice(p, s, Choice::Search); }),
        ll::ui::ButtonOptions{.disabled = s->searchDisabled}
    );
    form.button(
        tx(*s, "string.naturescompass.searchForNext", "Search for Next"),
        onDdui(weak, [](Player& p, SessionPtr const& s) { handleChoice(p, s, Choice::SearchForNext); }),
        ll::ui::ButtonOptions{.disabled = s->nextDisabled, .visible = b.nextVisible}
    );
    form.button(s->sortLabel, onDdui(weak, [](Player& p, SessionPtr const& s) { handleSort(p, s); }));
    form.button(
        tx(*s, "string.naturescompass.teleport", "Teleport"),
        onDdui(weak, [](Player& p, SessionPtr const& s) { handleChoice(p, s, Choice::Teleport); }),
        ll::ui::ButtonOptions{.disabled = s->teleportDisabled, .visible = b.teleportVisible}
    );
    form.button(
        tx(*s, "string.naturescompass.cancel", "Cancel"),
        onDdui(weak, [](Player&, SessionPtr const& s) { handleCancel(s); })
    );
    form.divider();

    s->slots.resize(s->model.rows().size());
    for (std::size_t i = 0; i < s->slots.size(); ++i) {
        auto& slot = s->slots[i];
        form.button(
            slot.label,
            onDdui(weak, [i](Player& p, SessionPtr const& s) { handleRow(p, s, i); }),
            ll::ui::ButtonOptions{.disabled = slot.disabled, .visible = slot.visible}
        );
        form.label(slot.details, ll::ui::TextOptions{.visible = slot.visible});
    }
    refreshDdui(*s);

    s->filter.subscribe([weak](std::string const& value) {
        if (auto s = weak.lock(); s && !s->closed) applyFilter(*s, value);
    });

    auto shown = form.show([weak](ll::ui::CustomForm::Result) {
        if (auto s = weak.lock()) {
            if (!s->closed) log().info("Select Biome: {} closed the screen", s->playerName);
            endSession(s);
        }
    });
    if (!shown) {
        s->dataDriven = false;
        log().warn("Select Biome: the data-driven form can't be shown to {}", s->playerName);
        return false;
    }
    s->form.emplace(std::move(form));
    return true;
}

// ---- classic form ----------------------------------------------------------------------------

std::string classicRowLabel(Session const& s, core::BiomeRow const& row) {
    auto        lines    = rowLines(row, s.model.sortMode(), s.locale);
    bool        selected = s.model.isSelected(row);
    std::string out{!row.affordable ? kMarkRowLocked : selected ? kMarkRowSelected : kMarkRow};
    std::string sub  = row.affordable ? "§7" : "§8";
    out             += (row.affordable ? "§f" : "§7") + lines[0];
    for (int i = 1; i < 4; ++i) out += "\n" + sub + lines[i];
    return out;
}

/// The enchanting table's level icon (Bedrock's lapis "dust" icons), as NC draws level_N.
std::string levelIcon(core::BiomeRow const& row) {
    if (row.xpLevels <= 0) return {};
    auto n = std::to_string(std::min(row.xpLevels, 3));
    return std::string(row.affordable ? "textures/ui/dust_selectable_" : "textures/ui/dust_unselectable_") + n;
}

void showClassicFilter(Player& player, SessionPtr const& s) {
    std::weak_ptr<Session> weak = s;
    ll::form::CustomForm form(tx(*s, "string.naturescompass.selectBiome", "Select Biome"));
    form.appendInput("filter", "", tx(*s, "string.naturescompass.search", "Search"), s->model.filter());
    form.setSubmitButton(tx(*s, "string.naturescompass.search", "Search"));
    form.sendTo(player, [weak](Player& p, ll::form::CustomFormResult const& result, ll::form::FormCancelReason) {
        auto s = weak.lock();
        if (!s || s->closed) return;
        if (result) {
            if (auto it = result->find("filter"); it != result->end()) {
                if (auto const* value = std::get_if<std::string>(&it->second)) applyFilter(*s, *value);
            }
        }
        showClassic(p, s);
    });
}

void showClassic(Player& player, SessionPtr const& s) {
    using Handler = std::function<void(Player&, SessionPtr const&)>;
    std::vector<Handler> handlers;
    auto                 b = s->model.buttons(s->maxNextSearches, s->canTeleport);

    ll::form::SimpleForm form(std::string(kMarkTitle) + tx(*s, "string.naturescompass.selectBiome", "Select Biome"));
    auto add = [&](std::string const& label, std::string const& image, Handler handler) {
        if (image.empty()) form.appendButton(label);
        else form.appendButton(label, image, "path");
        handlers.push_back(std::move(handler));
    };
    auto control = [](bool active, std::string_view onMark, std::string_view offMark, std::string const& label) {
        return std::string(active ? onMark : offMark) + (active ? "§f" : "§7") + label;
    };
    auto choose = [](Choice choice) { return [choice](Player& p, SessionPtr const& s) { handleChoice(p, s, choice); }; };

    // The search box, then NC's buttons; the skin moves each one to its place.
    std::string box{kMarkSearchBox};
    // Shown without '§', so typed text can't fake the skin's markers.
    std::string shown = s->model.filter();
    for (auto pos = shown.find("§"); pos != std::string::npos; pos = shown.find("§")) shown.erase(pos, 2);
    box += s->model.filter().empty() ? "§7" + tx(*s, "string.naturescompass.search", "Search") : "§f" + shown;
    add(box, {}, [](Player& p, SessionPtr const& s) { showClassicFilter(p, s); });
    add(control(b.searchActive, kMarkButton, kMarkButtonOff, tx(*s, "string.naturescompass.search", "Search")),
        {},
        choose(Choice::Search));
    if (b.nextVisible) {
        add(control(
                b.nextActive,
                kMarkButton,
                kMarkButtonOff,
                tx(*s, "string.naturescompass.searchForNext", "Search for Next")
            ),
            {},
            choose(Choice::SearchForNext));
    }
    add(control(true, b.nextVisible ? kMarkSortPadded : kMarkButton, kMarkButtonOff, sortButtonText(s->model.sortMode(), s->locale)),
        {},
        [](Player& p, SessionPtr const& s) { handleSort(p, s); });
    if (b.teleportVisible) {
        add(control(b.teleportActive, kMarkTeleport, kMarkTeleportOff, tx(*s, "string.naturescompass.teleport", "Teleport")),
            {},
            choose(Choice::Teleport));
    }
    add(control(true, kMarkCancel, kMarkCancel, tx(*s, "string.naturescompass.cancel", "Cancel")),
        {},
        [](Player&, SessionPtr const& s) { handleCancel(s); });
    for (std::size_t i = 0; i < s->model.listed().size(); ++i) {
        auto const& row = *s->model.listedRow(i);
        add(classicRowLabel(*s, row), levelIcon(row), [i](Player& p, SessionPtr const& s) { handleRow(p, s, i); });
    }

    std::weak_ptr<Session> weak = s;
    form.sendTo(player, [weak, handlers = std::move(handlers)](Player& p, int index, ll::form::FormCancelReason) {
        auto s = weak.lock();
        if (!s || s->closed) return;
        if (index < 0 || static_cast<std::size_t>(index) >= handlers.size()) {
            log().info("Select Biome: {} closed the screen", s->playerName);
            endSession(s);
            return;
        }
        handlers[static_cast<std::size_t>(index)](p, s);
    });
}

// ---- opening ---------------------------------------------------------------------------------

void closeExisting(Player const& player) {
    auto it = gSessions.find(keyOf(player.getUuid()));
    if (it == gSessions.end()) return;
    auto old = it->second;
    gSessions.erase(it);
    bool wasOpen = !old->closed;
    old->closed  = true;
    if (wasOpen && old->dataDriven && old->form) (void)old->form->close();
}

} // namespace

void openSelectBiome(Player& player) {
    auto slot = compass::heldCompass(player);
    if (!slot) return;
    closeExisting(player);

    auto locale = localeOf(player);
    auto session =
        std::make_shared<Session>(player, locale, SelectBiomeModel(buildRows(player, locale), compassView(player)));
    gSessions[keyOf(session->uuid)] = session;

    auto mode  = frontend();
    bool shown = false;
    if (mode != Frontend::Classic) shown = showDdui(player, session);
    if (!shown && mode != Frontend::Ddui) {
        showClassic(player, session);
        shown = true;
    }
    if (!shown) {
        endSession(session);
        return;
    }
    log().info(
        "Select Biome opened for {} ({} biomes, {} form, selected '{}')",
        session->playerName,
        session->model.rows().size(),
        session->dataDriven ? "data-driven" : "classic",
        session->model.selected().value_or("")
    );
}

// ---- selftest hooks ----------------------------------------------------------------------------

namespace testing {

std::optional<ScreenInfo> screen(Player const& player) {
    auto s = sessionOf(player);
    if (!s) return std::nullopt;
    ScreenInfo info;
    info.dataDriven = s->dataDriven;
    info.selected   = s->model.selected();
    info.rows       = s->model.rows().size();
    info.listed     = s->model.listed().size();
    info.sort       = s->model.sortMode();
    if (s->dataDriven) {
        info.sortLabel = s->sortLabel.getData();
        for (auto const& slot : s->slots) {
            if (slot.visible.getData()) info.rowLabels.push_back(slot.label.getData());
        }
    }
    return info;
}

bool setFilter(Player& player, std::string const& filter) {
    auto s = sessionOf(player);
    if (!s) return false;
    if (s->dataDriven) {
        s->filter.setData(filter); // what an edit from the client does
    } else if (applyFilter(*s, filter)) {
        showClassic(player, s);
    }
    return true;
}

bool cycleSort(Player& player) {
    auto s = sessionOf(player);
    if (!s) return false;
    handleSort(player, s);
    return true;
}

bool clickRow(Player& player, std::size_t position) {
    auto s = sessionOf(player);
    if (!s) return false;
    handleRow(player, s, position);
    return true;
}

void close(Player& player) {
    if (auto s = sessionOf(player)) handleCancel(s);
}

} // namespace testing

namespace {

/// Closes every screen when the world goes away (the client loads many worlds).
class GuiModule {
public:
    void install() {
        if (mHooked) return;
        mHooked = true;
        game::db::onWorldClosed([] { gSessions.clear(); });
    }
    void uninstall() {
        auto sessions = std::move(gSessions);
        gSessions.clear();
        for (auto& [_, s] : sessions) {
            bool wasOpen = !s->closed;
            s->closed    = true;
            if (wasOpen && s->dataDriven && s->form) (void)s->form->close(); // no callbacks into an unloaded mod
        }
    }

private:
    bool mHooked = false;
};

} // namespace

} // namespace nl::feature::gui

NL_REGISTER_MODULE(nl::feature::gui::GuiModule);

// Debug: `/naturescompass gui <auto|ddui|classic>` picks the form front-end (process-wide), and
// `/naturescompass gui open` opens the screen for the executing player.
NL_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    using nl::feature::gui::Frontend;
    for (auto value : {Frontend::Auto, Frontend::Ddui, Frontend::Classic}) {
        cmd.overload()
            .text("gui")
            .text(std::string(nl::feature::gui::frontendName(value)))
            .execute([value](CommandOrigin const& origin, CommandOutput& output) {
                if (!nl::commands::isAdmin(origin)) return output.error("You need operator permission.");
                nl::feature::gui::setFrontend(value);
                nl::log().info("Select Biome front-end: {}", nl::feature::gui::frontendName(value));
                output.success("Select Biome front-end: " + std::string(nl::feature::gui::frontendName(value)));
            });
    }
    cmd.overload().text("gui").text("open").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (!nl::commands::isAdmin(origin)) return output.error("You need operator permission.");
        auto* player = nl::commands::playerOf(origin);
        if (!player) return output.error("Players only.");
        if (!nl::feature::compass::heldCompass(*player)) return output.error("Hold a Nature's Compass.");
        nl::feature::gui::openSelectBiome(*player);
        output.success("Select Biome opened.");
    });
});
