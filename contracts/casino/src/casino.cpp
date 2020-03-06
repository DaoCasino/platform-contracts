#include <casino/casino.hpp>

using eosio::check;

namespace casino {

void casino::add_game(uint64_t game_id, game_params_type params) {
    require_auth(get_owner());
    check(platform::read::is_active_game(platform_account, game_id), "the game was not verified by the platform");
    games.emplace(get_self(), [&](auto& row) {
        row.game_id = game_id;
        row.params = params;
    });
}

void casino::remove_game(uint64_t game_id) {
    require_auth(get_owner());
    const auto itr = games.find(game_id);
    check(itr != games.end(), "the game was not added");
    games.erase(itr);
}

void casino::set_owner(name new_owner) {
    const auto old_owner = get_owner();
    eosio::require_auth(old_owner);
    check(is_account(new_owner), "new owner account does not exist");
    owner_account.set(owner_row{new_owner}, old_owner);
}

} // namespace casino
