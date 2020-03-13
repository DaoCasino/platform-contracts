#pragma once

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>

namespace events {


using bytes = std::vector<char>;
using eosio::name;


struct [[eosio::table("version"), eosio::contract("events")]] version_row {
    std::string version;
};
using version_singleton = eosio::singleton<"version"_n, version_row>;


class [[eosio::contract("events")]] events: public eosio::contract {
public:
    using eosio::contract::contract;

    events(name receiver, name code, eosio::datastream<const char*> ds);

    [[eosio::action("send")]]
    void send(name sender, uint64_t casino_id, uint64_t game_id, uint64_t req_id, uint32_t event_type, bytes data);

private:
    version_singleton version;
};


} // namespace events
