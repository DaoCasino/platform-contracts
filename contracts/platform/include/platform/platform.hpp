#pragma once

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>

#include <platform/version.hpp>

namespace platform {


using bytes = std::vector<char>;
using eosio::name;


struct [[eosio::table("version"), eosio::contract("platform")]] version_row {
    std::string version;
};
using version_singleton = eosio::singleton<"version"_n, version_row>;

struct [[eosio::table("version"), eosio::contract("platform")]] global_row {
    uint64_t casinos_seq { 0u }; // <-- used for casino id auto increment
    uint64_t games_seq { 0u }; // <-- used for game id auto increment
};
using global_singleton = eosio::singleton<"global"_n, global_row>;

struct [[eosio::table("casino"), eosio::contract("platform")]] casino_row {
    uint64_t id;
    name contract;
    bool paused;
    bytes meta;

    uint64_t primary_key() const { return id; }
    uint64_t by_address() const { return contract.value; }
};

using casino_table = eosio::multi_index<
                        "casino"_n,
                        casino_row,
                        eosio::indexed_by<"address"_n, eosio::const_mem_fun<casino_row, uint64_t, &casino_row::by_address>>
                    >;

struct [[eosio::table("game"), eosio::contract("platform")]] game_row {
    uint64_t id;
    name contract;
    uint16_t params_cnt;
    bool paused;
    bytes meta;

    uint64_t primary_key() const { return id; }
    uint64_t by_address() const { return contract.value; }
};
using game_table = eosio::multi_index<
                    "game"_n,
                    game_row,
                    eosio::indexed_by<"address"_n, eosio::const_mem_fun<game_row, uint64_t, &game_row::by_address>>
                   >;


class [[eosio::contract("platform")]] platform: public eosio::contract {
public:
    using eosio::contract::contract;

    platform(name receiver, name code, eosio::datastream<const char*> ds):
        contract(receiver, code, ds),
        version(_self, _self.value),
        global(_self, _self.value),
        casinos(_self, _self.value),
        games(_self, _self.value)
    {
        version.set(version_row {CONTRACT_VERSION}, _self);
    }

    [[eosio::action("addcas")]]
    void add_casino(name contract, bytes meta);

    [[eosio::action("delcas")]]
    void del_casino(uint64_t id);

    [[eosio::action("pausecas")]]
    void pause_casino(uint64_t id, bool pause);

    [[eosio::action("setcontrcas")]]
    void set_contract_casino(uint64_t id, name contract);

    [[eosio::action("setmetacas")]]
    void set_meta_casino(uint64_t id, bytes meta);


    [[eosio::action("addgame")]]
    void add_game(name contract, uint16_t params_cnt, bytes meta);

    [[eosio::action("delgame")]]
    void del_game(uint64_t id);

    [[eosio::action("pausegame")]]
    void pause_game(uint64_t id, bool pause);

    [[eosio::action("setcontrgame")]]
    void set_contract_game(uint64_t id, name contract);

    [[eosio::action("setmetagame")]]
    void set_meta_game(uint64_t id, bytes meta);

private:
    version_singleton version;
    global_singleton global;
    casino_table casinos;
    game_table games;
};


// external non-writing functions
namespace read {

static casino_row get_casino(name platform_contract, uint64_t casino_id) {
    casino_table casinos(platform_contract, platform_contract.value);
    return casinos.get(casino_id, "casino not found");
}

static casino_row get_casino(name platform_contract, name casino_address) {
    casino_table casinos(platform_contract, platform_contract.value);
    auto casino_idx = casinos.get_index<"address"_n>();
    return casino_idx.get(casino_address.value, "casino not found");
}

static game_row get_game(name platform_contract, uint64_t game_id) {
    game_table games(platform_contract, platform_contract.value);
    return games.get(game_id, "game not found");
}

static game_row get_game(name platform_contract, name game_address) {
    game_table games(platform_contract, platform_contract.value);
    auto games_idx = games.get_index<"address"_n>();
    return games_idx.get(game_address.value, "no game found for a given account");
}

static bool is_active_casino(name platform_contract, uint64_t casino_id) {
    casino_table casinos(platform_contract, platform_contract.value);
    const auto casino_itr = casinos.find(casino_id);
    if (casino_itr == casinos.end()) {
        return false;
    }
    return !(casino_itr->paused);
}

static bool is_active_game(name platform_contract, uint64_t game_id) {
    game_table games(platform_contract, platform_contract.value);
    const auto game_itr = games.find(game_id);
    if (game_itr == games.end()) {
        return false;
    }
    return !(game_itr->paused);
}

} // namespace read


} // namespace platform
