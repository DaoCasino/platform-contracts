#include <events/events.hpp>
#include <platform/platform.hpp>

namespace events {

void events::send(name sender, uint64_t casino_id, uint64_t game_id, uint64_t req_id, uint32_t event_type, bytes data) {
    require_auth(sender);
    eosio::check(platform::read::is_active_game(platform_contract, game_id), "game isn't active");
    eosio::check(platform::read::is_active_casino(platform_contract, casino_id), "casino isn't active");

    auto game = platform::read::get_game(platform_contract, game_id);
    eosio::check(game.contract == sender, "incorrect sender(sender should be game's contract)");
}

} // namespace events
