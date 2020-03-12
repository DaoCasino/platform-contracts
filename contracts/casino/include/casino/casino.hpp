#pragma once

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>
#include <casino/version.hpp>
#include <platform/platform.hpp>

namespace casino {

using eosio::name;
using eosio::asset;

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

struct [[eosio::table("balance"), eosio::contract("casino")]] balance_row {
    uint64_t game_id;
    asset quantity;

    uint64_t primary_key() const { return game_id; }
};

using balance_table = eosio::multi_index<"balance"_n, balance_row>;

class [[eosio::contract("casino")]] casino: public eosio::contract {
public:
    using eosio::contract::contract;

    casino(name receiver, name code, eosio::datastream<const char*> ds):
        contract(receiver, code, ds),
        version(_self, _self.value),
        games(_self, _self.value),
        owner_account(_self, _self.value)
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
private:
    version_singleton version;
    game_table games;
    owner_singleton owner_account;

    name get_owner() {
        return owner_account.get().owner;
    }

    void add_balance(uint64_t game_id, asset quantity) {
        balance_table balances(_self, _self.value);
        auto itr = balances.find(game_id);
        if (itr == balances.end()) {
            balances.emplace(get_self(), [&](auto& row) {
                row.game_id = game_id;
                row.quantity = quantity;
            });
        } else {
            balances.modify(itr, get_self(), [&](auto& row) {
                row.quantity += quantity;
            });
        }
    }

    void sub_balance(uint64_t game_id, asset quantity) {
        balance_table balances(_self, _self.value);
        auto itr = balances.find(game_id);
        if (itr == balances.end()) {
            balances.emplace(get_self(), [&](auto& row) {
                row.game_id = game_id;
                row.quantity = -quantity;
            });
        } else {
            balances.modify(itr, get_self(), [&](auto& row) {
                row.quantity -= quantity;
            });
        }
    }

    asset get_balance(uint64_t game_id) const {
        balance_table balances(_self, _self.value);
        const auto itr = balances.find(game_id);
        return itr != balances.end() ? itr->quantity : asset();
    }

    bool is_active_game(uint64_t game_id) const {
        game_table games(_self, _self.value);
        return games.find(game_id) != games.end();
    }
};

} // namespace casino
