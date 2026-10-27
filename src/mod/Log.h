#pragma once

#include "ll/api/io/Logger.h"

namespace nl {

/// The mod's logger (LeviLamina console + log file, prefixed "NaturesLodestone").
/// Every message is flushed immediately so e2e harnesses can tail the log.
[[nodiscard]] ll::io::Logger& log();

} // namespace nl
