#include <casino/casino.hpp>
#include <casino/version.hpp>

using eosio::check;

namespace casino {

casino::casino(name receiver, name code, eosio::datastream<const char*> ds):
    contract(receiver, code, ds),
    version(_self, _self.value),
    games(_self, _self.value),
    owner_account(_self, _self.value),
    game_state(_self, _self.value),
    _gstate(_self, _self.value)
{
    owner_account.set(owner_row{_self}, _self);
    version.set(version_row {CONTRACT_VERSION}, _self);

    if (!_gstate.exists()) {
        // on contract first call
        _gstate.set(global_state{
            zero_asset,
            zero_asset,
            current_time_point()
        }, _self);
    }
    gstate = _gstate.get();
}

void casino::add_game(uint64_t game_id, game_params_type params) {
    require_auth(get_owner());
    check(platform::read::is_active_game(platform_contract, game_id), "the game was not verified by the platform");
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
    require_auth(old_owner);
    check(is_account(new_owner), "new owner account does not exist");
    owner_account.set(owner_row{new_owner}, old_owner);
}

void casino::on_transfer(name game_account, name casino_account, eosio::asset quantity, std::string memo) {
    if (game_account == get_self() || casino_account != get_self()) {
        return;
    }
    platform::game_table platform_games(platform_contract, platform_contract.value);
    auto games_idx = platform_games.get_index<"address"_n>();

    if (games_idx.find(game_account.value) != games_idx.end()) {
        // get game throws if there's no game in the table
        const auto game_id = get_game_id(game_account);
        verify_game(game_id);
        add_balance(game_id, quantity);
    }
}

void casino::on_loss(name game_account, name player_account, eosio::asset quantity) {
    require_auth(game_account);
    check(is_account(player_account), "to account does not exist");
    const auto game_id = get_game_id(game_account);
    verify_game(game_id);
    transfer(player_account, quantity);
    sub_balance(game_id, quantity);
}

void casino::claim_profit(name game_account) {
    const auto ct = current_time_point();
    const auto game_id = get_game_id(game_account);
    verify_game(game_id);
    check(ct - get_last_claim_time(game_id) > microseconds(useconds_per_month), "already claimed within past month");
    const auto beneficiary = platform::read::get_game(platform_contract, game_id).beneficiary;
    const auto to_transfer = get_game_profits(game_id);
    transfer(beneficiary, to_transfer);
    sub_balance(game_id, to_transfer);
    update_last_claim_time(game_id);
}

uint64_t casino::get_game_id(name game_account) {
    // get game throws if there's no game in the table
    return platform::read::get_game(platform_contract, game_account).id;
}

void casino::verify_game(uint64_t game_id) {
    check(platform::read::is_active_game(platform_contract, game_id), "the game was not verified by the platform");
    check(is_active_game(game_id), "the game is not run by the casino");
}

void casino::withdraw(name beneficiary_account, asset quantity) {
    require_auth(get_owner());
    const auto ct = current_time_point();
    const auto account_balance = token::get_balance(_self, core_symbol);
    const auto max_transfer = account_balance / 10;
    check(gstate.game_active_sessions_sum + gstate.game_profits_sum <= account_balance, "cannot withdraw: not enough account balance");
    check(quantity <= max_transfer, "cannot withdraw more than 10% of account balance");
    transfer(beneficiary_account, quantity);
}

void casino::session_update(name game_account, uint64_t ses_id, asset max_win_delta) {
    require_auth(game_account);
    const auto game_id = get_game_id(game_account);
    verify_game(game_id);
    session_update(ses_id, game_id, max_win_delta);
}

void casino::session_close(name game_account, uint64_t ses_id) {
    require_auth(game_account);
    const auto game_id = get_game_id(game_account);
    verify_game(game_id);
    session_close(ses_id, game_id);
}

uint32_t casino::get_profit_margin(uint64_t game_id) {
    return platform::read::get_game(platform_contract, game_id).profit_margin;
}

asset casino::get_game_profits(uint64_t game_id) {
    const auto game_row = platform::read::get_game(platform_contract, game_id);
    return get_balance(game_id) * get_profit_margin(game_id) / 100;
}

} // namespace casino
