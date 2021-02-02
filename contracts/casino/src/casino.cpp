#include <casino/casino.hpp>
#include <casino/version.hpp>

namespace casino {

casino::casino(name receiver, name code, eosio::datastream<const char*> ds):
    contract(receiver, code, ds),
    version(_self, _self.value),
    games(_self, _self.value),
    game_state(_self, _self.value),
    _gstate(_self, _self.value),
    _bstate(_self, _self.value),
    bonus_balance(_self, _self.value),
    player_stats(_self, _self.value),
    games_no_bonus(_self, _self.value),
    tokens(_self, _self.value),
    game_tokens(_self, _self.value),
    _gtokens(_self, _self.value),
    player_tokens(_self, _self.value),
    game_params(_self, _self.value),
    ban_list(_self, _self.value) {
    
    version.set(version_row {CONTRACT_VERSION}, _self);

    gstate = _gstate.get_or_create(_self, global_state{
        zero_asset,
        zero_asset,
        0,
        current_time_point(),
        name(),
        _self
    });

    bstate = _bstate.get_or_create(_self, bonus_pool_state{
        _self,
        zero_asset,
        zero_asset
    });

    const auto symbol_raw = core_symbol.raw();

    gtokens = _gtokens.get_or_create(_self, global_tokens_state{
        {{symbol_raw, gstate.game_active_sessions_sum.amount}},
        {{symbol_raw, gstate.game_profits_sum.amount}},
        {{symbol_raw, bstate.total_allocated.amount}},
        {{symbol_raw, bstate.greeting_bonus.amount}},
        {{symbol_raw, current_time_point()}}
    });

    // init game params for "BET"
    for (auto it = games.begin(); it != games.end(); ++it) {
        if (game_params.find(it->game_id) != game_params.end()) {
            continue;
        }
        game_params.emplace(get_self(), [&](auto& row) {
            row.game_id = it->game_id;
            row.params = {{core_symbol.code().raw(), it->params}};
        });
    }
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

    game_tokens.emplace(get_self(), [&](auto& row) {
        row.game_id = game_id;
        row.balance = {};
        row.active_sessions_sum = {};
    });
    game_params.emplace(get_self(), [&](auto& row) {
        row.game_id = game_id;
        row.params = {{core_symbol.code().raw(), params}};
    });
}

void casino::set_game_param(uint64_t game_id, game_params_type params) {
    require_auth(get_owner());
    const auto itr = games.require_find(game_id, "id not in the games list");
    const auto itr_token = game_params.require_find(game_id, "id not in the game params list");

    games.modify(itr, get_self(), [&](auto& row) {
        row.params = params;
    });
    game_params.modify(itr_token, get_self(), [&](auto& row) {
        row.params[core_symbol.code().raw()] = params;
    });
}

void casino::remove_game(uint64_t game_id) {
    require_auth(get_owner());
    const auto game_itr = games.require_find(game_id, "the game was not added");
    const auto game_state_itr = game_state.require_find(game_id, "game is not in game state");
    const auto game_tokens_itr = game_tokens.require_find(game_id, "game is not in game tokens");
    const auto game_params_itr = game_params.require_find(game_id, "game is not in game params");
    check(!game_state_itr->active_sessions_amount, "trying to remove a game with non-zero active sessions");
    reward_game_developer(game_id);
    game_params.erase(game_params_itr);
    game_tokens.erase(game_tokens_itr);
    game_state.erase(game_state_itr);
    games.erase(game_itr);
}

void casino::set_owner(name new_owner) {
    const auto old_owner = get_owner();
    require_auth(old_owner);
    check(is_account(new_owner), "new owner account does not exist");
    gstate.owner = new_owner;
}

void casino::on_transfer(name game_account, name casino_account, asset quantity, std::string memo) {    
    if (game_account == get_self() || casino_account != get_self()) {
        return;
    }
    platform::token_table tokens(get_platform(), get_platform().value);
    auto token_it = tokens.require_find(quantity.symbol.code().raw(), "token is not in the list");
    check(token_it->contract == get_first_receiver(), "transfer from incorrect contract");
    verify_asset(quantity);
    if (memo ==  "bonus") {
        if (quantity.symbol == core_symbol) {
            bstate.total_allocated += quantity;
        }
        gtokens.total_allocated_bonus[quantity.symbol.raw()] += quantity.amount;
        return;
    }
    platform::game_table platform_games(get_platform(), get_platform().value);
    auto games_idx = platform_games.get_index<"address"_n>();

    if (games_idx.find(game_account.value) != games_idx.end()) {
        const auto game_id = get_game_id(game_account);
        add_balance(game_id, quantity * get_profit_margin(game_id) / percent_100);
    }
}

void casino::on_loss(name game_account, name player_account, asset quantity) {
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
    reward_game_developer(game_id);
}

uint64_t casino::get_game_id(name game_account) {
    // get game throws if there's no game in the table
    return platform::read::get_game(get_platform(), game_account).id;
}

void casino::verify_game(uint64_t game_id) {
    check(platform::read::is_active_game(get_platform(), game_id), "the game was not verified by the platform");
    check(is_active_game(game_id), "the game is paused");
}

void casino::verify_from_game_account(name game_account) {
    require_auth(game_account);
    const auto game_id = get_game_id(game_account);
    verify_game(game_id);
}

void casino::greet_new_player(name player_account) {
    check_from_platform_game();
    create_or_update_bonus_balance(player_account, bstate.greeting_bonus);
};

void casino::greet_new_player_token(name player_account, const std::string& token) {
    check_from_platform_game();
    const auto symbol = token::get_symbol(get_platform(), token);
    const auto greeting_bonus = asset(gtokens.greeting_bonus[symbol.raw()], symbol);
    create_or_update_bonus_balance(player_account, greeting_bonus);
};

void casino::withdraw(name beneficiary_account, asset quantity) {
    require_auth(get_owner());
    verify_asset(quantity);
    const auto ct = current_time_point();
    const auto symbol = quantity.symbol;
    const auto account_balance = token::get_balance(get_platform(), _self, symbol) - asset(gtokens.total_allocated_bonus[symbol.raw()], symbol);
    // in case game developers screwed it up
    const auto game_profits_sum = asset(std::max(0LL, gtokens.game_profits_sum[symbol.raw()]), symbol);
    const auto game_active_sessions_sum = asset(gtokens.game_active_sessions_sum[symbol.raw()], symbol);
    if (account_balance > game_active_sessions_sum + game_profits_sum) {
        const asset max_transfer = account_balance - game_active_sessions_sum - game_profits_sum;
        check(quantity <= max_transfer, "quantity exceededs max transfer amount");
        transfer(beneficiary_account, quantity, "casino profits");
    } else {
        check(account_balance > game_profits_sum, "developer profits exceed account balance");
        const asset max_transfer = std::min(account_balance / 10, account_balance - game_profits_sum);
        check(quantity <= max_transfer, "quantity exceededs max transfer amount");
        check(ct - gtokens.last_withdraw_time[symbol.raw()] > microseconds(useconds_per_week), "already claimed within past week");
        transfer(beneficiary_account, quantity, "casino profits");
        gstate.last_withdraw_time = ct;
        gtokens.last_withdraw_time[symbol.raw()] = ct;
    }
}

void casino::session_update(name game_account, asset max_win_delta) {
    require_auth(game_account);
    session_update_volume(get_game_id(game_account), max_win_delta);
}

void casino::session_close(name game_account, asset quantity) {
    require_auth(game_account);
    session_close_internal(get_game_id(game_account), quantity);
}

void casino::on_new_session(name game_account) {
    require_auth(game_account);
    const auto game_id = get_game_id(game_account);
    verify_game(game_id);
    session_update_amount(game_id);
}

void casino::on_new_session_player(name game_account, name player_account) {
    // player stats
    const auto player_stat = get_or_create_player_stat(player_account);
    player_stats.modify(player_stat, _self, [&](auto& row) {
        row.sessions_created++;
    });
}

void casino::on_new_depo_legacy(name game_account, asset quantity) {
    verify_from_game_account(game_account);
}

void casino::on_new_depo(name game_account, name player_account, asset quantity) {
    verify_from_game_account(game_account);
    verify_asset(quantity);

    if (quantity.symbol == core_symbol) {
        const auto player_stat = get_or_create_player_stat(player_account);
        player_stats.modify(player_stat, _self, [&](auto& row) {
            row.volume_real += quantity;
            row.profit_real -= quantity;
        });
    }

    const auto symbol_raw = quantity.symbol.raw();
    const auto itr_tokens = get_or_create_player_tokens(player_account);
    player_tokens.modify(itr_tokens, _self, [&](auto& row) {
        row.volume_real[symbol_raw] += quantity.amount;
        row.profit_real[symbol_raw] -= quantity.amount;
    });
}

void casino::on_ses_payout(name game_account, name player_account, asset quantity) {
    verify_from_game_account(game_account);
    verify_asset(quantity);

    if (quantity.symbol == core_symbol) {
        const auto player_stat = get_or_create_player_stat(player_account);
        player_stats.modify(player_stat, _self, [&](auto& row) {
            row.profit_real += quantity;
        });
    }

    const auto symbol_raw = quantity.symbol.raw();
    const auto itr_tokens = get_or_create_player_tokens(player_account);
    player_tokens.modify(itr_tokens, _self, [&](auto& row) {
        row.profit_real[symbol_raw] += quantity.amount;
    });
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

void casino::set_greeting_bonus(asset amount) {
    require_auth(bstate.admin);
    verify_asset(amount);
    if (amount.symbol == core_symbol) {
        bstate.greeting_bonus = amount;
    }
    gtokens.greeting_bonus[amount.symbol.raw()] = amount.amount;    
}

void casino::withdraw_bonus(name to, asset quantity, const std::string& memo) {
    require_auth(get_owner());
    verify_asset(quantity);
    check(memo.size() <= 256, "memo has more than 256 bytes");
    const auto symbol_raw = quantity.symbol.raw();
    check(quantity.amount <= gtokens.total_allocated_bonus[symbol_raw], "withdraw quantity cannot exceed total bonus");

    if (quantity.symbol == core_symbol) {
        check(quantity <= bstate.total_allocated, "withdraw quantity cannot exceed total bonus");
        bstate.total_allocated -= quantity;
    }

    gtokens.total_allocated_bonus[symbol_raw] -= quantity.amount;

    transfer(to, quantity, memo);
}

void casino::send_bonus(name to, asset amount) {
    require_auth(bstate.admin);
    create_or_update_bonus_balance(to, amount);
}

void casino::subtract_bonus(name from, asset amount) {
    require_auth(bstate.admin);
    verify_asset(amount);

    if (amount.symbol == core_symbol) {
        const auto itr = bonus_balance.require_find(from.value, "player has no bonus");
        check(amount <= itr->balance, "subtract amount cannot exceed player's bonus balance");
        bonus_balance.modify(itr, _self, [&](auto& row) {
            row.balance -= amount;
        });
    }
  
    const auto itr_tokens = get_or_create_player_tokens(from);
    const auto symbol_raw = amount.symbol.raw();
    check(amount.amount <= itr_tokens->bonus_balance.at(symbol_raw), "subtract amount cannot exceed player's bonus balance");
    player_tokens.modify(itr_tokens, _self, [&](auto& row) {
        row.bonus_balance[symbol_raw] -= amount.amount;
    });
}

// only "BET" bonus token
void casino::convert_bonus(name account, const std::string& memo) {
    require_auth(bstate.admin);
    check(memo.size() <= 256, "memo has more than 256 bytes");
    const auto row = bonus_balance.require_find(account.value, "player has no bonus");
    const auto itr_tokens = get_or_create_player_tokens(account);
    const auto symbol_raw = core_symbol.raw();
    check(row->balance <= bstate.total_allocated, "convert quantity cannot exceed total allocated");
    check(itr_tokens->bonus_balance.at(symbol_raw) <= gtokens.total_allocated_bonus[symbol_raw],
        "convert quantity cannot exceed total allocated");
    bstate.total_allocated -= row->balance;
    gtokens.total_allocated_bonus[symbol_raw] -= itr_tokens->bonus_balance.at(symbol_raw);
    transfer(account, asset(itr_tokens->bonus_balance.at(symbol_raw), core_symbol), memo);
    bonus_balance.erase(row);
    player_tokens.modify(itr_tokens, _self, [&](auto& row) {
        row.bonus_balance[symbol_raw] = 0;
    });
}

void casino::convert_bonus_token(name account, symbol symbol, const std::string& memo) {
    require_auth(bstate.admin);
    check(memo.size() <= 256, "memo has more than 256 bytes");

    if (symbol == core_symbol) {
        const auto row = bonus_balance.require_find(account.value, "player has no bonus");
        check(row->balance <= bstate.total_allocated, "convert quantity cannot exceed total allocated");
        bstate.total_allocated -= row->balance;
        bonus_balance.erase(row);
    }

    const auto itr_tokens = get_or_create_player_tokens(account);
    const auto symbol_raw = symbol.raw();    
    check(itr_tokens->bonus_balance.at(symbol_raw) <= gtokens.total_allocated_bonus[symbol_raw],
        "convert quantity cannot exceed total allocated");
    gtokens.total_allocated_bonus[symbol_raw] -= itr_tokens->bonus_balance.at(symbol_raw);
    transfer(account, asset(itr_tokens->bonus_balance.at(symbol_raw), symbol), memo);
    player_tokens.modify(itr_tokens, _self, [&](auto& row) {
        row.bonus_balance[symbol_raw] = 0;
    });
}

void casino::session_lock_bonus(name game_account, name player_account, asset amount) {
    verify_from_game_account(game_account);
    check(games_no_bonus.find(get_game_id(game_account)) == games_no_bonus.end(), "game is restricted to bonus");
    verify_asset(amount);

    if (amount.symbol == core_symbol) {
        const auto row = bonus_balance.require_find(player_account.value, "player has no bonus");
        check(amount <= row->balance, "lock amount cannot exceed player's bonus balance");
        bonus_balance.modify(row, _self, [&](auto& row) {
            row.balance -= amount;
        });
        if (row->balance == zero_asset) {
            bonus_balance.erase(row);
        }

        // when player makes a bet using bonuses (volume increases)
        // his tokens are locked (assume he loses)
        const auto player_stat = get_or_create_player_stat(player_account);
        player_stats.modify(player_stat, _self, [&](auto& row) {
            row.volume_bonus += amount;
            row.profit_bonus -= amount;
        });
    }

    const auto symbol_raw = amount.symbol.raw();
    const auto itr = get_or_create_player_tokens(player_account);
    check(amount.amount <= itr->bonus_balance.at(symbol_raw), "lock amount cannot exceed player's bonus balance");
    player_tokens.modify(itr, _self, [&](auto& row) {
        row.bonus_balance[symbol_raw] -= amount.amount;
        row.volume_bonus[symbol_raw] += amount.amount;
        row.profit_bonus[symbol_raw] -= amount.amount;
    });
}

void casino::session_add_bonus(name game_account, name account, asset amount) {
    verify_from_game_account(game_account);
    verify_asset(amount);
    create_or_update_bonus_balance(account, amount);
    if (amount.symbol == core_symbol) {
        const auto player_stat = get_or_create_player_stat(account);
        player_stats.modify(player_stat, _self, [&](auto& row) {
            row.profit_bonus += amount;
        });
    }
    
    const auto itr = get_or_create_player_tokens(account);
    player_tokens.modify(itr, _self, [&](auto& row) {
        row.profit_bonus[amount.symbol.raw()] += amount.amount;
    });
}

void casino::add_game_no_bonus(name game_account) {
    require_auth(bstate.admin);

    const auto game_id = get_game_id(game_account);
    const auto it = games_no_bonus.find(game_id);
    check(it == games_no_bonus.end(), "game is already restricted");

    games_no_bonus.emplace(_self, [&](auto& row) {
        row.game_id = game_id;
    });
}

void casino::remove_game_no_bonus(name game_account) {
    require_auth(bstate.admin);

    const auto game_id = get_game_id(game_account);
    const auto it = games_no_bonus.require_find(game_id, "game is not restricted");

    games_no_bonus.erase(it);
}

void casino::add_token(std::string token_name) {
    require_auth(get_self());
    platform::read::verify_token(get_platform(), token_name);
    tokens.emplace(get_self(), [&](auto& row) {
        row.token_name = token_name;
        row.paused = false;
    });

    const auto symbol = token::get_symbol(get_platform(), token_name);
    gtokens.last_withdraw_time[symbol.raw()] = current_time_point();
}

void casino::remove_token(std::string token_name) {
    require_auth(get_self());
    tokens.erase(get_token_itr(token_name));
    const auto token_raw = platform::get_token_pk(token_name);
    for (auto it = game_params.begin(); it != game_params.end(); ++it) {
        game_params.modify(it, get_self(), [&](auto& row) {
            row.params.erase(token_raw);
        });
    }
}

void casino::pause_token(std::string token_name, bool pause) {
    require_auth(get_self());
    tokens.modify(get_token_itr(token_name), get_self(), [&](auto& row) {
        row.paused = pause;
    });
}

void casino::migrate_token() {  
    require_auth(get_owner());
    const auto symbol_raw = core_symbol.raw();

    // game state
    for (auto it = game_state.begin(); it != game_state.end(); ++it) {
        if (game_tokens.find(it->game_id) != game_tokens.end()) {
            continue;
        }
        game_tokens.emplace(get_self(), [&](auto& row) {
            row.game_id = it->game_id;
            row.balance[symbol_raw] = it->balance.amount;
            row.active_sessions_sum[symbol_raw] = it->active_sessions_sum.amount;
        });
    }

    // bonus balances
    for (auto it = bonus_balance.begin(); it != bonus_balance.end(); ++it) {
        if (player_tokens.find(it->player.value) != player_tokens.end()) {
            continue;
        }
        const auto it_stats = player_stats.find(it->player.value);
        player_tokens.emplace(get_self(), [&](auto& row) {
            row.player = it->player;
            row.bonus_balance[symbol_raw] = it->balance.amount;
            if (it_stats != player_stats.end()) {
                row.volume_real[symbol_raw] = it_stats->volume_real.amount;
                row.volume_bonus[symbol_raw] = it_stats->volume_bonus.amount;
                row.profit_real[symbol_raw] = it_stats->profit_real.amount;
                row.profit_bonus[symbol_raw] = it_stats->profit_bonus.amount;
            }
        });
    }

    // player stats, some player has stats and not bonus
    for (auto it = player_stats.begin(); it != player_stats.end(); ++it) {
        if (player_tokens.find(it->player.value) != player_tokens.end()) {
            continue;
        }
        player_tokens.emplace(get_self(), [&](auto& row) {
            row.player = it->player;
            row.volume_real[symbol_raw] = it->volume_real.amount;
            row.volume_bonus[symbol_raw] = it->volume_bonus.amount;
            row.profit_real[symbol_raw] = it->profit_real.amount;
            row.profit_bonus[symbol_raw] = it->profit_bonus.amount;
        });
    }
}

void casino::set_game_param_token(uint64_t game_id, std::string token, game_params_type params) {
    verify_token(token);
    if (token == "BET") {
        set_game_param(game_id, params);
        return;
    }
    require_auth(get_owner());

    const auto itr_token = game_params.require_find(game_id, "id is not found in game params");
    game_params.modify(itr_token, get_self(), [&](auto& row) {
        row.params[platform::get_token_pk(token)] = params;
    }); 
}

void casino::ban_player(name player) {
    require_auth(get_owner());
    const auto it = ban_list.find(player.value);
    eosio::check(it == ban_list.end(), "player is already banned");
    ban_list.emplace(get_self(), [&](auto& row) {
        row.player = player;
    });
}

void casino::unban_player(name player) {
    require_auth(get_owner());
    const auto it = ban_list.find(player.value);
    eosio::check(it != ban_list.end(), "player is not banned");
    ban_list.erase(it);
}

} // namespace casino
