#pragma once

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>
#include <casino/version.hpp>
#include <platform/platform.hpp>

namespace casino {

using eosio::name;
using eosio::asset;
using eosio::time_point;
using eosio::current_time_point;
using eosio::microseconds;
using eosio::check;

using game_params_type = std::vector<std::pair<uint16_t, uint32_t>>;

struct [[eosio::table("game"), eosio::contract("casino")]] game_row {
    uint64_t game_id;
    game_params_type params;

    uint64_t primary_key() const { return game_id; }
};

using game_table = eosio::multi_index<"game"_n, game_row>;

struct [[eosio::table("version"), eosio::contract("casino")]] version_row {
    std::string version;
};
using version_singleton = eosio::singleton<"version"_n, version_row>;

struct [[eosio::table("owner"), eosio::contract("casino")]] owner_row {
    name owner;
};
using owner_singleton = eosio::singleton<"owner"_n, owner_row>;

struct [[eosio::table("gamestate"), eosio::contract("casino")]] game_state_row {
    uint64_t game_id;
    asset quantity;
    eosio::time_point last_claim_time;

    uint64_t primary_key() const { return game_id; }
};

using game_state_table = eosio::multi_index<"gamestate"_n, game_state_row>;

class [[eosio::contract("casino")]] casino: public eosio::contract {
public:
    using eosio::contract::contract;

    casino(name receiver, name code, eosio::datastream<const char*> ds):
        contract(receiver, code, ds),
        version(_self, _self.value),
        games(_self, _self.value),
        owner_account(_self, _self.value),
        game_state(_self, _self.value)
    {
        owner_account.set(owner_row{_self}, _self);
        version.set(version_row {CONTRACT_VERSION}, _self);
    }

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

    static constexpr int64_t seconds_per_day = 24 * 3600;
    static constexpr int64_t useconds_per_day = seconds_per_day * 1000'000ll;
    static constexpr int64_t useconds_per_month = 30 * useconds_per_day;
private:
    version_singleton version;
    game_table games;
    owner_singleton owner_account;
    game_state_table game_state;

    name get_owner() {
        return owner_account.get().owner;
    }

    void add_balance(uint64_t game_id, asset quantity) {
        game_state_table game_state(_self, _self.value);
        auto itr = game_state.find(game_id);
        if (itr == game_state.end()) {
            game_state.emplace(get_self(), [&](auto& row) {
                row.game_id = game_id;
                row.quantity = quantity;
                row.last_claim_time = current_time_point();
            });
        } else {
            game_state.modify(itr, get_self(), [&](auto& row) {
                row.quantity += quantity;
            });
        }
    }

    void sub_balance(uint64_t game_id, asset quantity) {
        game_state_table game_state(_self, _self.value);
        auto itr = game_state.find(game_id);
        if (itr == game_state.end()) {
            game_state.emplace(get_self(), [&](auto& row) {
                row.game_id = game_id;
                row.quantity = -quantity;
                row.last_claim_time = current_time_point();
            });
        } else {
            game_state.modify(itr, get_self(), [&](auto& row) {
                row.quantity -= quantity;
            });
        }
    }

    asset get_balance(uint64_t game_id) const {
        game_state_table game_state(_self, _self.value);
        const auto itr = game_state.find(game_id);
        return itr != game_state.end() ? itr->quantity : asset();
    }

    bool is_active_game(uint64_t game_id) const {
        game_table games(_self, _self.value);
        return games.find(game_id) != games.end();
    }

    uint64_t get_game_id(name game_account) {
        // get game throws if there's no game in the table
        return platform::read::get_game(platform_contract, game_account).id;
    }

    time_point get_last_claim_time(uint64_t game_id) const {
        game_state_table game_state(_self, _self.value);
        const auto itr = game_state.find(game_id);
        return itr == game_state.end() ? time_point() : itr->last_claim_time;
    }

    void update_last_claim_time(uint64_t game_id) {
        game_state_table game_state(_self, _self.value);
        const auto itr = game_state.require_find(game_id, "game is not in the casino");
        game_state.modify(itr, _self, [&](auto& row) {
            row.last_claim_time = current_time_point();
        });
    }

    void transfer(name to, asset quantity) {
        eosio::action(
            eosio::permission_level{_self, "active"_n},
            "eosio.token"_n,
            "transfer"_n,
            std::make_tuple(_self, to, quantity, std::string("player winnings"))
        ).send();
    }

    void verify_game(uint64_t game_id) {
        check(platform::read::is_active_game(platform_contract, game_id), "the game was not verified by the platform");
        check(is_active_game(game_id), "the game is not run by the casino");
    }
};

} // namespace casino
