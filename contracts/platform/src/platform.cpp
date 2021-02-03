#include <platform/platform.hpp>
#include <platform/version.hpp>

namespace platform {

platform::platform(name receiver, name code, eosio::datastream<const char*> ds):
    contract(receiver, code, ds),
    version(_self, _self.value),
    global(_self, _self.value),
    casinos(_self, _self.value),
    games(_self, _self.value),
    tokens(_self, _self.value),
    ban_list(_self, _self.value)
{
    version.set(version_row {CONTRACT_VERSION}, _self);
}

void platform::set_rsa_pubkey(const std::string& rsa_pubkey) {
    require_auth(get_self());

    auto gs = global.get_or_default();
    gs.rsa_pubkey = rsa_pubkey;
    global.set(gs, get_self());
}

void platform::add_casino(name contract, bytes meta) {
    require_auth(get_self());

    auto gs = global.get_or_default();

    casinos.emplace(get_self(), [&](auto& row) {
        row.id = gs.casinos_seq++; // <-- auto-increment id
        row.paused = false; // enabled by default
        row.contract = contract;
        row.meta = std::move(meta);
    });

    global.set(gs, get_self());
}

void platform::del_casino(uint64_t id) {
    require_auth(get_self());

    const auto casino_itr = casinos.require_find(id, "casino not found");
    casinos.erase(casino_itr);
}

void platform::pause_casino(uint64_t id, bool pause) {
    require_auth(get_self());

    const auto casino_itr = casinos.require_find(id, "casino not found");
    casinos.modify(casino_itr, get_self(), [&](auto& row) {
        row.paused = pause;
    });
}

void platform::set_contract_casino(uint64_t id, name contract) {
    require_auth(get_self());

    const auto casino_itr = casinos.require_find(id, "casino not found");
    casinos.modify(casino_itr, get_self(), [&](auto& row) {
        row.contract = contract;
    });
}

void platform::set_meta_casino(uint64_t id, bytes meta) {
    require_auth(get_self());

    const auto casino_itr = casinos.require_find(id, "casino not found");
    casinos.modify(casino_itr, get_self(), [&](auto& row) {
        row.meta = meta;
    });
}

void platform::set_rsa_pubkey_casino(uint64_t id, const std::string& rsa_pubkey) {
    require_auth(get_self());

    const auto casino_itr = casinos.require_find(id, "casino not found");
    casinos.modify(casino_itr, get_self(), [&](auto& row) {
        row.rsa_pubkey = rsa_pubkey;
    });
}


void platform::add_game(name contract, uint16_t params_cnt, bytes meta) {
    require_auth(get_self());

    auto gs = global.get_or_default();

    games.emplace(get_self(), [&](auto& row) {
        row.id = gs.games_seq++; // <-- auto-increment id
        row.paused = false; // <-- enabled by default
        row.params_cnt = params_cnt;
        row.contract = contract;
        row.meta = std::move(meta);
    });

    global.set(gs, get_self());
}

void platform::del_game(uint64_t id) {
    require_auth(get_self());

    const auto game_itr = games.require_find(id, "game not found");
    games.erase(game_itr);
}

void platform::pause_game(uint64_t id, bool pause) {
    require_auth(get_self());

    const auto game_itr = games.require_find(id, "game not found");
    games.modify(game_itr, get_self(), [&](auto& row) {
        row.paused = pause;
    });
}

void platform::set_contract_game(uint64_t id, name contract) {
    require_auth(get_self());

    const auto game_itr = games.require_find(id, "game not found");
    games.modify(game_itr, get_self(), [&](auto& row) {
        row.contract = contract;
    });
}

void platform::set_meta_game(uint64_t id, bytes meta) {
    require_auth(get_self());

    const auto game_itr = games.require_find(id, "game not found");
    games.modify(game_itr, get_self(), [&](auto& row) {
        row.meta = meta;
    });
}

void platform::set_profit_margin_game(uint64_t id, uint32_t profit_margin) {
    require_auth(get_self());

    const auto game_itr = games.require_find(id, "game not found");
    games.modify(game_itr, get_self(), [&](auto& row) {
        row.profit_margin = profit_margin;
    });
}

void platform::set_beneficiary_game(uint64_t id, name beneficiary) {
    require_auth(get_self());

    const auto game_itr = games.require_find(id, "game not found");
    games.modify(game_itr, get_self(), [&](auto& row) {
        row.beneficiary = beneficiary;
    });
}

void platform::add_token(std::string token_name, name contract) {
    require_auth(get_self());
    tokens.emplace(get_self(), [&](auto& row) {
        row.token_name = token_name;
        row.contract = contract;
    });
}

void platform::del_token(std::string token_name) {
    require_auth(get_self());
    tokens.erase(tokens.require_find(get_token_pk(token_name), "del token: no token found"));
}

void platform::ban_player(name player) {
    require_auth(get_self());
    const auto it = ban_list.find(player.value);
    eosio::check(it == ban_list.end(), "player is already banned");
    ban_list.emplace(get_self(), [&](auto& row) {
        row.player = player;
    });
}

void platform::unban_player(name player) {
    require_auth(get_self());
    const auto it = ban_list.find(player.value);
    eosio::check(it != ban_list.end(), "player is not banned");
    ban_list.erase(it);
}

} // namespace platform
