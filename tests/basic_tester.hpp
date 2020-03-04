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
    basic_tester() {
        produce_blocks( 2 );

        create_accounts({
            N(eosio.token)
        });

        produce_blocks( 100 );
        set_code( N(eosio.token), contracts::system::token::wasm());
        set_abi( N(eosio.token), contracts::system::token::abi().data() );

        symbol core_symbol = symbol{CORE_SYM};
        create_currency( N(eosio.token), config::system_account_name, asset(100000000000000, core_symbol) );
        issue(config::system_account_name, asset(1672708210000, core_symbol) );
    }

    template <typename Contract>
    void deploy_contract(account_name account) {
        set_code(account, Contract::wasm() );
        set_abi(account, Contract::abi().data() );
    }

    void create_currency( const name& contract, const name& manager, const asset& maxsupply ) {
        auto act = mutable_variant_object()
            ("issuer",         manager)
            ("maximum_supply", maxsupply);

        base_tester::push_action(contract, N(create), contract, act);
    }

    void issue( const name& to, const asset& amount, const name& manager = config::system_account_name ) {
        base_tester::push_action( N(eosio.token), N(issue), manager, mutable_variant_object()
                                ("to",       to)
                                ("quantity", amount)
                                ("memo",     "") );
    }

    void transfer( const name& from, const name& to, const asset& amount, const name& manager = config::system_account_name ) {
        base_tester::push_action( N(eosio.token), N(transfer), manager, mutable_variant_object()
                                ("from",     from)
                                ("to",       to)
                                ("quantity", amount)
                                ("memo",     "") );
    }
};

} // namespace testing
