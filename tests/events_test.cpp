#include "basic_tester.hpp"


namespace testing {

using bytes = std::vector<char>;


class events_tester : public basic_tester {
public:
    const static account_name platform_name;
    const static account_name events_name;

public:
    events_tester() {
        create_accounts({
            platform_name,
            events_name,
        });

        produce_blocks(2);

        deploy_contract<contracts::platform>(platform_name);
        deploy_contract<contracts::events>(events_name);
    }

    fc::variant get_casino(uint64_t casino_id) {
        vector<char> data = get_row_by_account(platform_name, platform_name, N(casino), casino_id );
        return data.empty() ? fc::variant() : abi_ser[platform_name].binary_to_variant("casino_row", data, abi_serializer_max_time);
    }

    fc::variant get_game(uint64_t game_id) {
        vector<char> data = get_row_by_account(platform_name, platform_name, N(game), game_id );
        return data.empty() ? fc::variant() : abi_ser[platform_name].binary_to_variant("game_row", data, abi_serializer_max_time);
    }
};

const account_name events_tester::platform_name = N(platform);
const account_name events_tester::events_name = N(events);


BOOST_AUTO_TEST_SUITE(events_tests)

BOOST_FIXTURE_TEST_CASE(send_event_ok, events_tester) try {
    account_name casino_account = N(casino.1);
    account_name game_account = N(game.1);

    create_account(casino_account);
    create_account(game_account);

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
        ("contract", game_account)
        ("params_cnt", 0)
        ("meta", bytes())
    );

    produce_blocks(2);

    base_tester::push_action(events_name, N(send), game_account, mvo()
        ("sender", game_account)
        ("casino_id", 0)
        ("game_id", 0)
        ("req_id", 0)
        ("event_type", 0)
        ("data", bytes())
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(vesion_test, events_tester) try {
    account_name casino_account = N(casino.1);
    account_name game_account = N(game.1);

    create_account(casino_account);
    create_account(game_account);

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
        ("contract", game_account)
        ("params_cnt", 0)
        ("meta", bytes())
    );

    produce_blocks(2);

    base_tester::push_action(events_name, N(send), game_account, mvo()
        ("sender", game_account)
        ("casino_id", 0)
        ("game_id", 0)
        ("req_id", 0)
        ("event_type", 0)
        ("data", bytes())
    );

    vector<char> data = get_row_by_account(events_name, events_name, N(version), N(version) );
    BOOST_REQUIRE_EQUAL(data.empty(), false);

    auto version = abi_ser[events_name].binary_to_variant("version_row", data, abi_serializer_max_time);
    BOOST_REQUIRE_EQUAL(version["version"], contracts::version());

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(send_event_invalid_sender, events_tester) try {
    account_name casino_account = N(casino.1);
    account_name game_account = N(game.1);

    create_account(casino_account);
    create_account(game_account);

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
        ("contract", game_account)
        ("params_cnt", 0)
        ("meta", bytes())
    );

    produce_blocks(2);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("incorrect sender(sender should be game's contract)"),
        push_action(events_name, N(send), casino_account, mvo()
            ("sender", casino_account)
            ("casino_id", 0)
            ("game_id", 0)
            ("req_id", 0)
            ("event_type", 0)
            ("data", bytes())
        )
    );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(send_event_unauth_sender, events_tester) try {
    account_name casino_account = N(casino.1);
    account_name game_account = N(game.1);

    create_account(casino_account);
    create_account(game_account);

    produce_blocks(2);

    BOOST_REQUIRE_EQUAL(std::string("missing authority of game.1"),
        push_action(events_name, N(send), casino_account, mvo()
            ("sender", game_account)
            ("casino_id", 0)
            ("game_id", 0)
            ("req_id", 0)
            ("event_type", 0)
            ("data", bytes())
        )
    );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(send_event_inactive_game, events_tester) try {
    account_name casino_account = N(casino.1);
    account_name game_account = N(game.1);

    create_account(casino_account);
    create_account(game_account);

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    produce_blocks(2);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("game isn't active"),
        push_action(events_name, N(send), game_account, mvo()
            ("sender", game_account)
            ("casino_id", 0)
            ("game_id", 0)
            ("req_id", 0)
            ("event_type", 0)
            ("data", bytes())
        )
    );

    base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
        ("contract", game_account)
        ("params_cnt", 0)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(pausegame), platform_name, mvo()
        ("id", 0)
        ("pause", true)
    );

    produce_blocks(2);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("game isn't active"),
        push_action(events_name, N(send), game_account, mvo()
            ("sender", game_account)
            ("casino_id", 0)
            ("game_id", 0)
            ("req_id", 0)
            ("event_type", 0)
            ("data", bytes())
        )
    );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(send_event_inactive_casino, events_tester) try {
    account_name casino_account = N(casino.1);
    account_name game_account = N(game.1);

    create_account(casino_account);
    create_account(game_account);


    base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
        ("contract", game_account)
        ("params_cnt", 0)
        ("meta", bytes())
    );

    produce_blocks(2);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("casino isn't active"),
        push_action(events_name, N(send), game_account, mvo()
            ("sender", game_account)
            ("casino_id", 0)
            ("game_id", 0)
            ("req_id", 0)
            ("event_type", 0)
            ("data", bytes())
        )
    );

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(pausecas), platform_name, mvo()
        ("id", 0)
        ("pause", true)
    );

    produce_blocks(2);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("casino isn't active"),
        push_action(events_name, N(send), game_account, mvo()
            ("sender", game_account)
            ("casino_id", 0)
            ("game_id", 0)
            ("req_id", 0)
            ("event_type", 0)
            ("data", bytes())
        )
    );

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
} // namespace testing
