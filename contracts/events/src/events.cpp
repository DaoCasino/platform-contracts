#include <events/events.hpp>
#include <events/version.hpp>
#include <platform/platform.hpp>

namespace events {

events::events(name receiver, name code, eosio::datastream<const char*> ds):
    contract(receiver, code, ds),
    version(_self, _self.value)
{
    version.set(version_row {CONTRACT_VERSION}, _self);
}

void events::send(name sender, uint64_t casino_id, uint64_t game_id, uint64_t req_id, uint32_t event_type, bytes data) {
    require_auth(sender);
    eosio::check(platform::read::is_active_game(platform_contract, game_id), "game isn't active");
    eosio::check(platform::read::is_active_casino(platform_contract, casino_id), "casino isn't active");

    auto game = platform::read::get_game(platform_contract, game_id);
    eosio::check(game.contract == sender, "incorrect sender(sender should be game's contract)");
}

} // namespace events
