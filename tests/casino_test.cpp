#include "basic_tester.hpp"

namespace testing {

using bytes = std::vector<char>;


class casino_tester : public basic_tester {
public:
    static const account_name platform_account;
    static const account_name casino_account;

    casino_tester() {
        create_accounts({
            platform_account,
            casino_account
        });

        produce_blocks(2);

        deploy_contract<contracts::platform>(platform_account);
        deploy_contract<contracts::casino>(casino_account);
    }

    fc::variant get_game(uint64_t game_id) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(game), game_id );
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("game_row", data, abi_serializer_max_time);
    }
};

const account_name casino_tester::platform_account = N(dao.platform);
const account_name casino_tester::casino_account = N(dao.casino);

using game_params_type = std::vector<std::pair<uint16_t, uint32_t>>;

BOOST_AUTO_TEST_SUITE(casino_tests)

BOOST_FIXTURE_TEST_CASE(add_game, casino_tester) try {

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_account, N(addgame), platform_account, mvo()
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
        push_action(platform_account, N(addgame), platform_account, mvo()
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
    vector<char> data = get_row_by_account(casino_account, casino_account, N(owner), N(owner) );
    BOOST_REQUIRE_EQUAL(data.empty(), false);

    auto owner_row = abi_ser[casino_account].binary_to_variant("owner_row", data, abi_serializer_max_time);
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

BOOST_AUTO_TEST_SUITE_END()

} // namespace testing