#include <casino/casino.hpp>

using eosio::check;

namespace casino {

void casino::add_game(uint64_t game_id, game_params_type params) {
    check(platform::read::is_active_game(platform_contract, game_id), "the game was not verified by the platform");
    games.emplace(get_self(), [&](auto& row) {
        row.game_id = game_id;
        row.params = params;
    });
}

void casino::remove_game(uint64_t game_id) {
    const auto itr = games.find(game_id);
    check(itr != games.end(), "the game was not added");
    games.erase(itr);
}

} // namespace casino
