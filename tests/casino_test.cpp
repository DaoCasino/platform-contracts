#include "basic_tester.hpp"

namespace testing {

using bytes = std::vector<char>;

static constexpr int64_t seconds_per_day = 24 * 3600;
static constexpr int64_t seconds_per_month = 30 * seconds_per_day;

class casino_tester : public basic_tester {
public:
    static const account_name casino_account;

    casino_tester() {
        create_accounts({
            platform_name,
            casino_account
        });

        produce_blocks(2);

        deploy_contract<contracts::platform>(platform_name);
        deploy_contract<contracts::casino>(casino_account);

        push_action(casino_account, N(setplatform), casino_account, mvo()
            ("platform_name", platform_name)
        );
    }

    fc::variant get_game(uint64_t game_id) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(game), game_id );
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("game_row", data, abi_serializer_max_time);
    }

    asset get_game_balance(uint64_t game_id, symbol balance_symbol = symbol{CORE_SYM}) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(gamestate), game_id );
        return data.empty() ? asset(0, balance_symbol) : abi_ser[casino_account].binary_to_variant("game_state_row", data, abi_serializer_max_time)["quantity"].as<asset>();
    }

    asset get_balance( const account_name& act, symbol balance_symbol = symbol{CORE_SYM} ) {
      vector<char> data = get_row_by_account( N(eosio.token), act, N(accounts), account_name(balance_symbol.to_symbol_code().value) );
      return data.empty() ? asset(0, balance_symbol) : abi_ser[N(eosio.token)].binary_to_variant("account", data, abi_serializer_max_time)["balance"].as<asset>();
   }

   fc::variant get_session_state(uint64_t ses_id) {
       vector<char> data = get_row_by_account(casino_account, casino_account, N(sessionstate), ses_id );
       return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("session_state_row", data, abi_serializer_max_time);
   }

   fc::variant get_global() {
       vector<char> data = get_row_by_account(casino_account, casino_account, N(global), N(global) );
       return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("global_state", data, abi_serializer_max_time);
   }
};

const account_name casino_tester::casino_account = N(dao.casino);

using game_params_type = std::vector<std::pair<uint16_t, uint32_t>>;

BOOST_AUTO_TEST_SUITE(casino_tests)

BOOST_FIXTURE_TEST_CASE(add_game, casino_tester) try {

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", casino_account)
            ("params_cnt", 1)
            ("meta", bytes())
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );
    auto game = get_game(0);
    BOOST_REQUIRE(!game.is_null());
    BOOST_REQUIRE_EQUAL(game["game_id"].as<uint64_t>(), 0);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(add_game_verify_failure, casino_tester) try {
    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("the game was not verified by the platform"),
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(revmove_game, casino_tester) try {

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", casino_account)
            ("params_cnt", 1)
            ("meta", bytes())
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(rmgame), casino_account, mvo()
            ("game_id", 0)
        )
    );

    auto game = get_game(0);
    BOOST_REQUIRE(game.is_null());
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(remove_game_not_added_failure, casino_tester) try {
    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("the game was not added"),
        push_action(casino_account, N(rmgame), casino_account, mvo()
            ("game_id", 0)
        )
    );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_owner, casino_tester) try {
    name casino = N(casino.1);
    create_account(casino);
    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(setowner), casino_account, mvo()
            ("new_owner", casino)
        )
    );
    vector<char> data = get_row_by_account(casino_account, casino_account, N(global), N(global) );
    BOOST_REQUIRE_EQUAL(data.empty(), false);

    auto owner_row = abi_ser[casino_account].binary_to_variant("global_state", data, abi_serializer_max_time);
    BOOST_REQUIRE_EQUAL(owner_row["owner"].as<name>(), casino);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_owner_failure, casino_tester) try {
    name casino = N(casino.1);
    create_account(casino);
    BOOST_REQUIRE_EQUAL(
        error(std::string("missing authority of ") + casino_account.to_string()),
        push_action(casino_account, N(setowner), casino, mvo()
            ("new_owner", casino)
        )
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(add_game_auth_failure, casino_tester) try {
    name casino = N(casino.1);
    create_account(casino);
    BOOST_REQUIRE_EQUAL(
        error(std::string("missing authority of ") + casino_account.to_string()),
        push_action(casino_account, N(addgame), casino, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(remove_game_auth_failure, casino_tester) try {
    name casino = N(casino.1);
    create_account(casino);
    BOOST_REQUIRE_EQUAL(
        error(std::string("missing authority of ") + casino_account.to_string()),
        push_action(casino_account, N(rmgame), casino, mvo()
            ("game_id", 0)
        )
    );
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(simple_transfer_to_casino, casino_tester) try {
    name game_account = N(game.boy);
    create_accounts({
        game_account
    });
    transfer(config::system_account_name, game_account, STRSYM("3.0000"));
    transfer(game_account, casino_account, STRSYM("3.0000"), game_account);
    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("3.0000"));
    BOOST_REQUIRE_EQUAL(get_game_balance(game_account), STRSYM("0.0000"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(on_transfer_update_game_balance, casino_tester) try {
    name game_account = N(game.boy);
    create_accounts({
        game_account
    });
    transfer(config::system_account_name, game_account, STRSYM("3.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", game_account)
            ("params_cnt", 1)
            ("meta", bytes())
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );

    transfer(game_account, casino_account, STRSYM("3.0000"), game_account);
    BOOST_REQUIRE_EQUAL(get_game_balance(0), STRSYM("3.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("303.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(game_account), STRSYM("0.0000"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(on_transfer_from_inactive_casino_game, casino_tester) try {
    name game_account = N(game.boy);
    create_accounts({
        game_account
    });

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", game_account)
            ("params_cnt", 1)
            ("meta", bytes())
        )
    );

    transfer(config::system_account_name, game_account, STRSYM("3.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));

    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("300.0000"));
    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("the game is not run by the casino"),
        transfer(game_account, casino_account, STRSYM("3.0000"), game_account)
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(on_transfer_from_inactive_platform_game, casino_tester) try {
    name game_account = N(game.boy);
    create_accounts({
        game_account
    });

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", game_account)
            ("params_cnt", 1)
            ("meta", bytes())
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(pausegame), platform_name, mvo()
            ("id", 0)
            ("pause", true)
        )
    );

    transfer(config::system_account_name, game_account, STRSYM("3.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("the game was not verified by the platform"),
        transfer(game_account, casino_account, STRSYM("3.0000"), game_account)
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(on_loss_update_game_balance, casino_tester) try {
    name game_account = N(game.boy);
    name player_account = N(din.don);

    create_accounts({
        game_account,
        player_account
    });
    transfer(config::system_account_name, casino_account, STRSYM("3.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", game_account)
            ("params_cnt", 1)
            ("meta", bytes())
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(onloss), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", STRSYM("3.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(get_balance(player_account), STRSYM("3.0000"));
    BOOST_REQUIRE_EQUAL(get_game_balance(0), STRSYM("-3.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("0.0000"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(on_loss_from_inactive_casino_game, casino_tester) try {
    name game_account = N(game.boy);
    name player_account = N(din.don);

    create_accounts({
        game_account,
        player_account
    });
    transfer(config::system_account_name, game_account, STRSYM("3.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", game_account)
            ("params_cnt", 1)
            ("meta", bytes())
        )
    );

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("the game is not run by the casino"),
        push_action(casino_account, N(onloss), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", STRSYM("3.0000"))
        )
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(on_loss_from_nonexistent_platform_game, casino_tester) try {
    name game_account = N(game.boy);
    name player_account = N(din.don);

    create_accounts({
        game_account,
        player_account
    });
    transfer(config::system_account_name, game_account, STRSYM("3.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("no game found for a given account"),
        push_action(casino_account, N(onloss), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", STRSYM("3.0000"))
        )
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(on_loss_from_inactive_platform_game, casino_tester) try {
    name game_account = N(game.boy);
    name player_account = N(din.don);

    create_accounts({
        game_account,
        player_account
    });
    transfer(config::system_account_name, game_account, STRSYM("3.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", game_account)
            ("params_cnt", 1)
            ("meta", bytes())
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(pausegame), platform_name, mvo()
            ("id", 0)
            ("pause", true)
        )
    );

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("the game was not verified by the platform"),
        push_action(casino_account, N(onloss), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", STRSYM("3.0000"))
        )
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(claim_profit, casino_tester) try {
    name game_account = N(game.boy);
    name game_beneficiary_account = N(din.don);

    create_accounts({
        game_account,
        game_beneficiary_account
    });
    transfer(config::system_account_name, game_account, STRSYM("3.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", game_account)
            ("params_cnt", 1)
            ("meta", bytes())
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(setmargin), platform_name, mvo()
            ("id", 0)
            ("profit_margin", 50)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(setbenefic), platform_name, mvo()
            ("id", 0)
            ("beneficiary", game_beneficiary_account)
        )
    );


    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        transfer(game_account, casino_account, STRSYM("3.0000"), game_account)
    );

    produce_block(fc::seconds(seconds_per_month + 1));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(claimprofit), game_account, mvo()
            ("game_account", game_account)
        )
    );

    BOOST_REQUIRE_EQUAL(get_game_balance(0), STRSYM("1.5000"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("301.5000"));
    BOOST_REQUIRE_EQUAL(get_balance(game_beneficiary_account), STRSYM("1.5000"));
    BOOST_REQUIRE_EQUAL(get_balance(game_account), STRSYM("0.0000"));

    produce_block(fc::seconds(3));
    // second claim should fail
    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("already claimed within past month"),
        push_action(casino_account, N(claimprofit), game_account, mvo()
            ("game_account", game_account)
        )
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(withdraw, casino_tester) try {
    name game_account = N(game.boy);
    name game_beneficiary_account = N(din.don);
    name casino_beneficiary_account = N(don.din);

    create_accounts({
        game_account,
        game_beneficiary_account,
        casino_beneficiary_account
    });

    transfer(config::system_account_name, game_account, STRSYM("100.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("290.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", game_account)
            ("params_cnt", 1)
            ("meta", bytes())
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(setmargin), platform_name, mvo()
            ("id", 0)
            ("profit_margin", 50)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(setbenefic), platform_name, mvo()
            ("id", 0)
            ("beneficiary", game_beneficiary_account)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sesupdate), game_account, mvo()
            ("game_account", game_account)
            ("max_win_delta", STRSYM("10.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        transfer(game_account, casino_account, STRSYM("10.0000"), game_account)
    );

    BOOST_REQUIRE_EQUAL(get_global()["game_active_sessions_sum"].as<asset>(), STRSYM("10.0000"));
    BOOST_REQUIRE_EQUAL(get_global()["game_profits_sum"].as<asset>(), STRSYM("5.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", STRSYM("30.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("270.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_beneficiary_account), STRSYM("30.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sesupdate), game_account, mvo()
            ("game_account", game_account)
            ("max_win_delta", STRSYM("300.0000"))
        )
    );

    // max transfer is 27 now

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("quantity exceededs max transfer amount"),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", STRSYM("30.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("already claimed within past week"),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", STRSYM("20.0000"))
        )
    );

    produce_block(fc::seconds(seconds_per_month + 1));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sesclose), game_account, mvo()
            ("game_account", game_account)
            ("quantity", STRSYM("310.0000"))
        )
    );

    // claim all except game profits

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", STRSYM("265.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(get_balance(casino_beneficiary_account), STRSYM("295.0000"));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()

} // namespace testing
