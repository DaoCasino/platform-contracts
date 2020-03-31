#pragma once

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>
#include <platform/platform.hpp>

namespace casino {

using eosio::name;
using eosio::asset;
using eosio::time_point;
using eosio::current_time_point;
using eosio::microseconds;
using eosio::check;
using eosio::symbol;

using game_params_type = std::vector<std::pair<uint16_t, uint32_t>>;

struct [[eosio::table("game"), eosio::contract("casino")]] game_row {
    uint64_t game_id; // unique id of the game - global to casino and platform contracts
    game_params_type params; // game params is simply a vector of integer pairs

    uint64_t primary_key() const { return game_id; }
};

using game_table = eosio::multi_index<"game"_n, game_row>;

struct [[eosio::table("version"), eosio::contract("casino")]] version_row {
    std::string version;
};
using version_singleton = eosio::singleton<"version"_n, version_row>;

struct [[eosio::table("gamestate"), eosio::contract("casino")]] game_state_row {
    uint64_t game_id;
    asset balance; // game's balance aka not clamed profits
    eosio::time_point last_claim_time; // last time of claim
    uint64_t active_sessions_amount; //
    asset active_sessions_sum; // sum of tokens between currently active sessions

    uint64_t primary_key() const { return game_id; }
};

using game_state_table = eosio::multi_index<"gamestate"_n, game_state_row>;

struct [[eosio::table("global"), eosio::contract("casino")]] global_state {
    asset game_active_sessions_sum; // total sum of currently active sessions between all games
    asset game_profits_sum; // total sum of game developer profits
    uint64_t active_sessions_amount; // amount of active sessions
    time_point last_withdraw_time; // casino last withdraw time
    name platform; // platfrom account name
    name owner; // owner has the rights to withdraw and update the contract state
};

using global_state_singleton = eosio::singleton<"global"_n, global_state>;


class [[eosio::contract("casino")]] casino: public eosio::contract {
public:
    using eosio::contract::contract;

    casino(name receiver, name code, eosio::datastream<const char*> ds);

    ~casino() {
        _gstate.set(gstate, _self);
    }

    [[eosio::action("setplatform")]]
    void set_platform(name platform_name);
    [[eosio::action("addgame")]]
    void add_game(uint64_t game_id, game_params_type params);
    [[eosio::action("rmgame")]]
    void remove_game(uint64_t game_id);
    [[eosio::action("setowner")]]
    void set_owner(name new_owner);
    [[eosio::on_notify("eosio.token::transfer")]]
    void on_transfer(name from, name to, eosio::asset quantity, std::string memo);
    [[eosio::action("onloss")]]
    void on_loss(name game_account, name player_account, eosio::asset quantity);
    [[eosio::action("claimprofit")]]
    void claim_profit(name game_account);
    [[eosio::action("withdraw")]]
    void withdraw(name beneficiary_account, asset quantity);
    [[eosio::action("sesupdate")]]
    void session_update(name game_account, asset max_win_delta);
    [[eosio::action("sesclose")]]
    void session_close(name game_account, asset quantity);
    [[eosio::action("newsession")]]
    void on_new_session(name game_account);

    static constexpr int64_t seconds_per_day = 24 * 3600;
    static constexpr int64_t useconds_per_day = seconds_per_day * 1000'000ll;
    static constexpr int64_t useconds_per_week = 7 * useconds_per_day;
    static constexpr int64_t useconds_per_month = 30 * useconds_per_day;
    static constexpr symbol core_symbol = symbol(eosio::symbol_code("BET"), 4);
    static const asset zero_asset;
    static const int percent_100 = 100;
private:
    version_singleton version;
    game_table games;
    game_state_table game_state;
    global_state_singleton _gstate;
    global_state gstate;

    name get_owner() const {
        return gstate.owner;
    }

    uint32_t get_profit_margin(uint64_t game_id) const;

    asset get_balance(uint64_t game_id) const {
        const auto itr = game_state.require_find(game_id, "game not found");
        return itr->balance;
    }

    void add_balance(uint64_t game_id, asset quantity) {
        const auto itr = game_state.require_find(game_id, "game not found");
        game_state.modify(itr, get_self(), [&](auto& row) {
            row.balance += quantity;
        });
        gstate.game_profits_sum += quantity;
    }

    void sub_balance(uint64_t game_id, asset quantity) {
        const auto itr = game_state.require_find(game_id, "game not found");
        game_state.modify(itr, get_self(), [&](auto& row) {
            row.balance -= quantity;
        });
        gstate.game_profits_sum -= quantity;
    }

    bool is_active_game(uint64_t game_id) const {
        return games.find(game_id) != games.end();
    }

    time_point get_last_claim_time(uint64_t game_id) const {
        const auto itr = game_state.require_find(game_id, "game not found");
        return itr->last_claim_time;
    }

    void update_last_claim_time(uint64_t game_id) {
        const auto itr = game_state.require_find(game_id, "game is not listed in the casino");
        game_state.modify(itr, _self, [&](auto& row) {
            row.last_claim_time = current_time_point();
        });
    }

    void transfer(name to, asset quantity, std::string memo) {
        eosio::action(
            eosio::permission_level{_self, "active"_n},
            "eosio.token"_n,
            "transfer"_n,
            std::make_tuple(_self, to, quantity, memo)
        ).send();
    }

    void verify_game(uint64_t game_id);
    void verify_account(name game_account);
    uint64_t get_game_id(name game_account);

    void session_update(uint64_t game_id, asset quantity) {
        const auto itr = game_state.require_find(game_id, "game not found");
        game_state.modify(itr, _self, [&](auto& row) {
            row.active_sessions_sum += quantity;
        });
        gstate.game_active_sessions_sum += quantity;
    }

    void session_close(uint64_t game_id, asset quantity) {
        const auto itr = game_state.require_find(game_id, "game not found");
        check(quantity <= itr->active_sessions_sum, "invalid quantity in session close");
        check(quantity <= gstate.game_active_sessions_sum, "invalid quantity in session close");
        check(itr->active_sessions_amount, "no active sessions");
        game_state.modify(itr, _self, [&](auto& row) {
            row.active_sessions_sum -= quantity;
            row.active_sessions_amount--;
        });
        gstate.game_active_sessions_sum -= quantity;
        gstate.active_sessions_amount--;
    }

    void on_new_session(uint64_t game_id) {
        const auto itr = game_state.require_find(game_id);
        game_state.modify(itr, _self, [&](auto& row) {
            row.active_sessions_amount++;
        });
        gstate.active_sessions_amount++;
    }

    name get_platform() const {
        check(gstate.platform != name(), "platform name wasn't set");
        return gstate.platform;
    }
}; // casino contract

const asset casino::zero_asset = asset(0, casino::core_symbol);

namespace read {
    static uint64_t get_active_sessions_amount(name casino_contract, uint64_t game_id) {
        game_state_table game_state(casino_contract, casino_contract.value);
        return game_state.require_find(game_id)->active_sessions_amount;
    }

    static uint64_t get_total_active_sessions_amount(name casino_contract) {
        global_state_singleton global_state(casino_contract, casino_contract.value);
        return global_state.get_or_default().active_sessions_amount;
    }
} // ns read

} // namespace casino

namespace token {
    struct account {
        eosio::asset balance;
        uint64_t primary_key() const { return balance.symbol.raw(); }
    };

    typedef eosio::multi_index<"accounts"_n, account> accounts;

    eosio::asset get_balance(eosio::name account, eosio::symbol s) {
        accounts accountstable("eosio.token"_n, account.value);
        return accountstable.get(s.code().raw()).balance;
    }
}