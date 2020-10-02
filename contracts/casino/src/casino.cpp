#include <casino/casino.hpp>
#include <casino/version.hpp>

using eosio::check;

namespace casino {

casino::casino(name receiver, name code, eosio::datastream<const char*> ds):
    contract(receiver, code, ds),
    version(_self, _self.value),
    games(_self, _self.value),
    game_state(_self, _self.value),
    _gstate(_self, _self.value),
    _bstate(_self, _self.value),
    bonus_balance(_self, _self.value) {

    version.set(version_row {CONTRACT_VERSION}, _self);

    gstate = _gstate.get_or_create(_self, global_state{
                zero_asset,
                zero_asset,
                0,
                current_time_point(),
                name(),
                _self
            });

    bstate = _bstate.get_or_create(_self, bonus_state{
            _self,
            zero_asset
        });
}

void casino::set_platform(name platform_name) {
    require_auth(get_owner());
    check(is_account(platform_name), "platform_name account doesn't exists");
    gstate.platform = platform_name;
}

void casino::add_game(uint64_t game_id, game_params_type params) {
    require_auth(get_owner());
    check(platform::read::is_active_game(get_platform(), game_id), "the game was not verified by the platform");
    games.emplace(get_self(), [&](auto& row) {
        row.game_id = game_id;
        row.params = params;
        row.paused = false;
    });
    game_state.emplace(get_self(), [&](auto& row) {
        row.game_id = game_id;
        row.balance = zero_asset;
        row.last_claim_time = current_time_point();
        row.active_sessions_amount = 0;
        row.active_sessions_sum = zero_asset;
    });
}

void casino::remove_game(uint64_t game_id) {
    require_auth(get_owner());
    const auto game_itr = games.require_find(game_id, "the game was not added");
    const auto game_state_itr = game_state.require_find(game_id, "game is not in game state");
    check(!game_state_itr->active_sessions_amount, "trying to remove a game with non-zero active sessions");
    game_state.erase(game_state_itr);
    games.erase(game_itr);
}

void casino::set_owner(name new_owner) {
    const auto old_owner = get_owner();
    require_auth(old_owner);
    check(is_account(new_owner), "new owner account does not exist");
    gstate.owner = new_owner;
}

void casino::on_transfer(name game_account, name casino_account, eosio::asset quantity, std::string memo) {
    if (game_account == get_self() || casino_account != get_self()) {
        return;
    }
    platform::game_table platform_games(get_platform(), get_platform().value);
    auto games_idx = platform_games.get_index<"address"_n>();

    if (games_idx.find(game_account.value) != games_idx.end()) {
        const auto game_id = get_game_id(game_account);
        add_balance(game_id, quantity * get_profit_margin(game_id) / percent_100);
    }
}

void casino::on_loss(name game_account, name player_account, eosio::asset quantity) {
    require_auth(game_account);
    check(is_account(player_account), "to account does not exist");
    const auto game_id = get_game_id(game_account);
    transfer(player_account, quantity, "player winnings");
    sub_balance(game_id, quantity * get_profit_margin(game_id) / percent_100);
}

void casino::claim_profit(name game_account) {
    const auto ct = current_time_point();
    const auto game_id = get_game_id(game_account);
    check(ct - get_last_claim_time(game_id) > microseconds(useconds_per_month), "already claimed within past month");
    const auto beneficiary = platform::read::get_game(get_platform(), game_id).beneficiary;
    const auto to_transfer = get_balance(game_id);
    check(to_transfer > zero_asset, "cannot claim a negative profit");
    transfer(beneficiary, to_transfer, "game developer profits");
    sub_balance(game_id, to_transfer);
    update_last_claim_time(game_id);
}

uint64_t casino::get_game_id(name game_account) {
    // get game throws if there's no game in the table
    return platform::read::get_game(get_platform(), game_account).id;
}

void casino::verify_game(uint64_t game_id) {
    check(platform::read::is_active_game(get_platform(), game_id), "the game was not verified by the platform");
    check(is_active_game(game_id), "the game is paused");
}

void casino::withdraw(name beneficiary_account, asset quantity) {
    require_auth(get_owner());
    const auto ct = current_time_point();
    const auto account_balance = token::get_balance(_self, core_symbol);
    // in case game developers screwed it up
    const auto game_profits_sum = std::max(zero_asset, gstate.game_profits_sum);

    if (account_balance > gstate.game_active_sessions_sum + game_profits_sum) {
        const asset max_transfer = account_balance - gstate.game_active_sessions_sum - game_profits_sum;
        check(quantity <= max_transfer, "quantity exceededs max transfer amount");
        transfer(beneficiary_account, quantity, "casino profits");
    } else {
        check(account_balance > game_profits_sum, "developer profits exceed account balance");
        const asset max_transfer = std::min(account_balance / 10, account_balance - game_profits_sum);
        check(quantity <= max_transfer, "quantity exceededs max transfer amount");
        check(ct - gstate.last_withdraw_time > microseconds(useconds_per_week), "already claimed within past week");
        transfer(beneficiary_account, quantity, "casino profits");
        gstate.last_withdraw_time = ct;
    }
}

void casino::session_update(name game_account, asset max_win_delta) {
    require_auth(game_account);
    session_update(get_game_id(game_account), max_win_delta);
}

void casino::session_close(name game_account, asset quantity) {
    require_auth(game_account);
    session_close(get_game_id(game_account), quantity);
}

void casino::on_new_session(name game_account) {
    require_auth(game_account);
    const auto game_id = get_game_id(game_account);
    verify_game(game_id);
    on_new_session(game_id);
}

void casino::pause_game(uint64_t game_id, bool pause) {
    require_auth(get_self());

    const auto game_itr = games.require_find(game_id, "game not found");
    const auto game_state_itr = game_state.require_find(game_id, "game not found");

    games.modify(game_itr, get_self(), [&](auto& row) {
        row.paused = pause;
    });
}

uint32_t casino::get_profit_margin(uint64_t game_id) const {
    return platform::read::get_game(get_platform(), game_id).profit_margin;
}

void casino::set_bonus_admin(name new_admin) {
    require_auth(get_owner());
    check(is_account(new_admin), "new bonus admin account does not exist");
    bstate.admin = new_admin;
}

void casino::deposit_bonus(asset quantity, const std::string& memo) {
    require_auth(bstate.admin);
    check(memo.size() <= 256, "memo has more than 256 bytes");
    bstate.total_allocated += quantity;
    check(bstate.total_allocated <= token::get_balance(get_self(), core_symbol), "total bonus cannot exceed casino balance");
}

void casino::withdraw_bonus(name to, asset quantity, const std::string& memo) {
    require_auth(bstate.admin);
    check(memo.size() <= 256, "memo has more than 256 bytes");
    check(quantity <= bstate.total_allocated, "withdraw quantity cannot exceed total bonus");
    bstate.total_allocated -= quantity;
    transfer(to, quantity, memo);
}

void casino::send_bonus(name to, uint64_t amount) {
    require_auth(bstate.admin);
    const auto itr = bonus_balance.find(to.value);
    if (itr == bonus_balance.end()) {
        bonus_balance.emplace(_self, [&](auto& row) {
            row.player = to;
            row.balance = amount;
        });
    } else {
        bonus_balance.modify(itr, _self, [&](auto& row) {
            row.balance += amount;
        });
    }
}

void casino::subtract_bonus(name from, uint64_t amount) {
    require_auth(bstate.admin);
    const auto itr = bonus_balance.require_find(from.value, "player has no bonus");
    check(amount <= itr->balance, "subtract amount cannot exceed player's bonus balance");
    bonus_balance.modify(itr, _self, [&](auto& row) {
        row.balance -= amount;
    });
}

void casino::convert_bonus(name account, uint64_t amount, const std::string& memo) {
    require_auth(bstate.admin);
    check(memo.size() <= 256, "memo has more than 256 bytes");
    const auto row = bonus_balance.require_find(account.value, "player has no bonus");
    check(amount <= row->balance, "convert amount cannot exceed player's bonus balance");
    const auto quantity = convert_bonus_to_bet(amount);
    check(quantity <= bstate.total_allocated, "convert quantity cannot exceed total allocated");
    bstate.total_allocated -= quantity;
    bonus_balance.modify(row, _self, [&](auto& row) {
        row.balance -= amount;
    });
    transfer(account, quantity, memo);
}

} // namespace casino
