#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>

namespace platform {


using bytes = std::vector<char>;
using eosio::name;


struct [[eosio::table("version"), eosio::contract("platform")]] version_row {
    std::string version;
};
using version_sigleton = eosio::singleton<"version"_n, version_row>;

struct [[eosio::table("casino"), eosio::contract("platform")]] casino_row {
    uint64_t id;
    name contract;
    bool paused;
    bytes meta;

    uint64_t primary_key() const { return id; }
};
using casino_table = eosio::multi_index<"casino"_n, casino_row>;

struct [[eosio::table("game"), eosio::contract("platform")]] game_row {
    uint64_t id;
    name contract;
    uint16_t params_cnt;
    bool paused;
    bytes meta;

    uint64_t primary_key() const { return id; }
};
using game_table = eosio::multi_index<"game"_n, game_row>;


class [[eosio::contract("platform")]] platform: public eosio::contract {
public:
    using eosio::contract::contract;

    platform(name receiver, name code, eosio::datastream<const char*> ds):
        contract(receiver, code, ds),
        version(_self, _self.value),
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
    version_sigleton version;
    casino_table casinos;
    game_table games;
};


// external non-writing functions
namespace read {

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
