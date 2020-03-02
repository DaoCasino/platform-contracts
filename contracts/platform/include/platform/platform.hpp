#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>

namespace platform {

struct [[eosio::table("version")]] version_row {
    std::string version;
};
using version_sigleton = eosio::singleton<"version"_n, version_row>;


using bytes = std::vector<char>;
using eosio::name;


class [[eosio::contract("platform")]] platform: public eosio::contract {
public:
    using eosio::contract::contract;

    platform(name receiver, name code, eosio::datastream<const char*> ds):
        contract(receiver, code, ds),
        version(_self, _self.value)
    {
        version.set(version_row {CONTRACT_VERSION}, _self);
    }

    [[eosio::action("addcas")]]
    void add_casino(name contract, uint16_t params_cnt, bytes meta);

    [[eosio::action("delcas")]]
    void del_casino(uint64_t id);

    [[eosio::action("pausecas")]]
    void pause_casino(uint64_t id, bool pause);

    [[eosio::action("setcontrcas")]]
    void set_contract_casino(uint64_t id, name contract);

    [[eosio::action("setmetacas")]]
    void set_meta_casino(uint64_t id, bytes meta);

private:
    version_sigleton version;
};


} // namespace platform
