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

struct [[eosio::table("global"), eosio::contract("events")]] global_row {
    name platform;
};
using global_singleton = eosio::singleton<"global"_n, global_row>;


class [[eosio::contract("events")]] events: public eosio::contract {
public:
    using eosio::contract::contract;

    events(name receiver, name code, eosio::datastream<const char*> ds);

    [[eosio::action("setplatform")]]
    void set_platform(name platform_name);

    [[eosio::action("send")]]
    void send(name sender, uint64_t casino_id, uint64_t game_id, uint64_t req_id, uint32_t event_type, bytes data);

private:
    version_singleton version;
    global_singleton global;

private:
    name get_platform() {
        const auto gl = global.get();
        eosio::check(gl.platform != name(), "platform name isn't setted");
        return gl.platform;
    }
};


} // namespace events
