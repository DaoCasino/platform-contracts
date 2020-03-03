#pragma once

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <platform/version.hpp>

namespace casino {

using eosio::name;

struct [[eosio::table("game"), eosio::contract("casino")]] game_row {
    uint64_t game_id;
    std::vector<std::pair<uint16_t, uint32_t>> params; // pair {param_type, param_value}

    uint64_t primary_key() const { return game_id; }
};

using game_table = eosio::multi_index<"game"_n, game_row>;

struct [[eosio::table("version"), eosio::contract("casino")]] version_row {
    std::string version;
};
using version_singleton = eosio::singleton<"version"_n, version_row>;

class [[eosio::contract("casino")]] casino: public eosio::contract {
public:
    using eosio::contract::contract;

    casino(name receiver, name code, eosio::datastream<const char*> ds):
        contract(receiver, code, ds),
        version(_self, _self.value),
        games(_self, _self.value)
    {
        version.set(version_row {::platform::CONTRACT_VERSION}, _self);
    }
private:
    version_singleton version;
    game_table games;
};

} // namespace casino