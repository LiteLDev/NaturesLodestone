// Debug sub-command of unit U4 (tooling, not gameplay; docs/SPEC.md section 6):
//   /naturescompass damage <damage>     (op / cheats) sets the damage of the held compass
// Used by the e2e scenarios to show the durability bar without running searches.

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"

#include "feature/actions/Actions.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "mod/Commands.h"
#include "mod/I18n.h"
#include "mod/Log.h"

namespace nl::feature::actions {

// Command parameter types need external linkage (reflection).
struct DamageParams {
    int damage = 0;
};

} // namespace nl::feature::actions

NL_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    using nl::feature::actions::DamageParams;
    namespace compass = nl::feature::compass;
    cmd.overload<DamageParams>().text("damage").required("damage").execute(
        [](CommandOrigin const& origin, CommandOutput& output, DamageParams const& params) {
            auto* player = nl::commands::playerOf(origin);
            if (!nl::commands::isAdmin(origin)) {
                return output.error(
                    player ? nl::trFor(*player, "nl.actions.needOperator", "You need operator permission.")
                           : nl::tr("nl.actions.needOperator", "You need operator permission.")
                );
            }
            if (!player) return output.error(nl::tr("nl.actions.noCompass", "Hold a Nature's Compass."));
            auto slot = compass::heldCompass(*player);
            auto data = slot ? compass::dataAt(*player, *slot) : std::nullopt;
            if (!data) return output.error(nl::trFor(*player, "nl.actions.noCompass", "Hold a Nature's Compass."));
            data->damage = std::max(params.damage, 0);
            compass::storeAt(*player, *slot, *data);
            nl::feature::actions::refreshDurability(*player);
            nl::log().info("Compass damage of {} set to {}", player->getRealName(), data->damage);
            output.success(
                nl::trFor(
                    *player,
                    "nl.actions.damageSet",
                    "Compass damage set to {} (durability {}).",
                    data->damage,
                    compass::compassDurability()
                )
            );
        }
    );
});
