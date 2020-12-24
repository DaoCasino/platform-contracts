#pragma once

#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/resource_limits.hpp>
#include "contracts.hpp"
#include "test_symbol.hpp"

#include <fc/variant_object.hpp>
#include <fstream>

using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;

#ifndef TESTER
# ifdef NON_VALIDATING_TEST
#  define TESTER tester
# else
#  define TESTER validating_tester
# endif
#endif


// just sugar
#define STRSYM(str) core_sym::from_string(str)



namespace testing {

class basic_tester : public TESTER {
public:
    basic_tester() {}

    template <typename Contract>
    void deploy_contract(account_name account) {
        set_code(account, Contract::wasm() );
        set_abi(account, Contract::abi().data() );

        abi_def abi;
        abi_serializer abi_s;
        const auto& accnt = control->db().get<account_object,by_name>(account);
        abi_serializer::to_abi(accnt.abi, abi);
        abi_s.set_abi(abi, abi_serializer_max_time);
        abi_ser.insert({ account, abi_s });
    }

    action_result create_currency( const name& contract, const name& manager, const asset& maxsupply ) {
        auto act = mutable_variant_object()
            ("issuer",         manager)
            ("maximum_supply", maxsupply);

        return push_action(contract, N(create), contract, act);
    }

    action_result issue( const name& to, const asset& amount, const name& manager = config::system_account_name, 
        const name& contract = N(eosio.token)) {
        return push_action( contract, N(issue), manager, mutable_variant_object()
                                ("to",       to)
                                ("quantity", amount)
                                ("memo",     "") );
    }

    action_result transfer( const name& from, const name& to, const asset& amount, const std::string& memo = "") {
        return push_action( N(eosio.token), N(transfer), from, mutable_variant_object()
                                ("from",     from)
                                ("to",       to)
                                ("quantity", amount)
                                ("memo",     memo) );
    }

    action_result push_action( const action_name& contract, const action_name &name, const action_name &actor, const variant_object& data) {
        string action_type_name = abi_ser[contract].get_action_type(name);

        action act;
        act.account = contract;
        act.name = name;
        act.data = abi_ser[contract].variant_to_binary( action_type_name, data, abi_serializer_max_time );

        return base_tester::push_action( std::move(act), actor);
   }

    asset asset_from_string(const std::string& s) {
        return sym::from_string(s);
    }
public:
    std::map<account_name, abi_serializer> abi_ser;
};

} // namespace testing
