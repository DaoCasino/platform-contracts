#include <platform/version.hpp>
#include <platform/platform.hpp>

namespace platform {

void platform::add_casino(name contract, bytes meta) {
    require_auth(get_self());

    casinos.emplace(get_self(), [&](auto& row) {
        row.id = casinos.available_primary_key(); // <-- auto-increment id
        row.paused = false; // enabled by default
        row.contract = contract;
        row.meta = std::move(meta);
    });
}

void platform::del_casino(uint64_t id) {
    require_auth(get_self());

    auto casino_itr = casinos.find(id);
    eosio::check(casino_itr != casinos.end(), "casino not found");
    casinos.erase(casino_itr);
}

void platform::pause_casino(uint64_t id, bool pause) {
    require_auth(get_self());

    auto casino_itr = casinos.find(id);
    eosio::check(casino_itr != casinos.end(), "casino not found");
    casinos.modify(casino_itr, get_self(), [&](auto& row) {
        row.paused = pause;
    });
}

void platform::set_contract_casino(uint64_t id, name contract) {
    require_auth(get_self());

    auto casino_itr = casinos.find(id);
    eosio::check(casino_itr != casinos.end(), "casino not found");
    casinos.modify(casino_itr, get_self(), [&](auto& row) {
        row.contract = contract;
    });
}

void platform::set_meta_casino(uint64_t id, bytes meta) {
    require_auth(get_self());

    auto casino_itr = casinos.find(id);
    eosio::check(casino_itr != casinos.end(), "casino not found");
    casinos.modify(casino_itr, get_self(), [&](auto& row) {
        row.meta = meta;
    });
}


void platform::add_game(name contract, uint16_t params_cnt, bytes meta) {

}

void platform::del_game(uint64_t id) {

}

void platform::pause_game(uint64_t id, bool pause) {

}

void platform::set_contract_game(uint64_t id, name contract) {

}

void platform::set_meta_game(uint64_t id, bytes meta) {

}

} // namespace platform
