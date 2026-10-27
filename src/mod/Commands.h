#pragma once

#include <functional>

#include "mod/Registry.h"

namespace ll::command {
class CommandHandle;
}
class CommandOrigin;
class Player;

namespace nl::commands {

/// Adds overloads to the `/naturescompass` command. The function runs every time the command is
/// (re)registered (BDS start, every client local world):
///
///     NL_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
///         cmd.overload().text("count").execute([](CommandOrigin const& o, CommandOutput& out) { ... });
///     });
using SubcommandFn       = std::function<void(ll::command::CommandHandle&)>;
using SubcommandRegistry = StaticRegistry<SubcommandFn>;

/// The player who ran the command (nullptr for the console / command blocks).
[[nodiscard]] Player* playerOf(CommandOrigin const& origin);
/// Permission level 2+ (admin commands need it).
[[nodiscard]] bool isAdmin(CommandOrigin const& origin);

namespace detail {
struct SubcommandRegistrar {
    explicit SubcommandRegistrar(SubcommandFn fn) { SubcommandRegistry::add(std::move(fn)); }
};
} // namespace detail

} // namespace nl::commands

#define NL_REGISTER_SUBCOMMAND(...)                                                                                   \
    static ::nl::commands::detail::SubcommandRegistrar NL_UNIQUE(nl_subcommand_)(__VA_ARGS__)
