#include "basic_tester.hpp"


namespace testing {

using bytes = std::vector<char>;


class platform_tester : public basic_tester {
public:
    const static account_name platform_name;

public:
    platform_tester() {
        create_accounts({
            platform_name,
        });

        produce_blocks(2);

        deploy_contract<contracts::platform>(platform_name);
    }

    fc::variant get_casino(uint64_t casino_id) {
        vector<char> data = get_row_by_account(platform_name, platform_name, N(casino), casino_id );
        return data.empty() ? fc::variant() : abi_ser[platform_name].binary_to_variant("casino_row", data, abi_serializer_max_time);
    }

    fc::variant get_game(uint64_t game_id) {
        vector<char> data = get_row_by_account(platform_name, platform_name, N(game), game_id );
        return data.empty() ? fc::variant() : abi_ser[platform_name].binary_to_variant("game_row", data, abi_serializer_max_time);
    }

public:
    abi_serializer plarform_abi_ser;
};

const account_name platform_tester::platform_name = N(platform);


BOOST_AUTO_TEST_SUITE(platform_tests)


BOOST_FIXTURE_TEST_CASE(add_casino, platform_tester) try {
    account_name casino_account = N(casino.1);

    create_account(casino_account);

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    auto casino = get_casino(0);
    BOOST_REQUIRE(!casino.is_null());

    BOOST_REQUIRE_EQUAL(casino["contract"].as<account_name>(), casino_account);
    BOOST_REQUIRE_EQUAL(casino["meta"].as<bytes>().size(), 0);
    BOOST_REQUIRE_EQUAL(casino["paused"].as<bool>(), false);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(add_many_casinos, platform_tester) try {
    std::vector<account_name> casinos = {
        N(casino.1),
        N(casino.2),
        N(casino.3),
        N(casino.4),
        N(casino.5),
    };

    for (const auto& casino: casinos) {
        create_account(casino);

        base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
            ("contract", casino)
            ("meta", bytes())
        );
    }

    for (auto i = 0; i < casinos.size(); ++i) {
        auto casino = get_casino(i);
        BOOST_REQUIRE(!casino.is_null());

        BOOST_REQUIRE_EQUAL(casino["contract"].as<account_name>(), casinos[i]);
        BOOST_REQUIRE_EQUAL(casino["meta"].as<bytes>().size(), 0);
        BOOST_REQUIRE_EQUAL(casino["paused"].as<bool>(), false);
    }

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_pause_casino, platform_tester) try {
    account_name casino_account = N(casino.1);

    create_account(casino_account);

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(pausecas), platform_name, mvo()
        ("id", 0)
        ("pause", true)
    );

    auto casino = get_casino(0);

    BOOST_REQUIRE_EQUAL(casino["paused"].as<bool>(), true);

    base_tester::push_action(platform_name, N(pausecas), platform_name, mvo()
        ("id", 0)
        ("pause", false)
    );

    casino = get_casino(0);

    BOOST_REQUIRE_EQUAL(casino["paused"].as<bool>(), false);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_contract_casino, platform_tester) try {
    account_name casino_account = N(casino.1);
    account_name casino_account_new = N(casino.2);

    create_account(casino_account);
    create_account(casino_account_new);

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(setcontrcas), platform_name, mvo()
        ("id", 0)
        ("contract", casino_account_new)
    );

    auto casino = get_casino(0);

    BOOST_REQUIRE_EQUAL(casino["contract"].as<account_name>(), casino_account_new);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_meta_casino, platform_tester) try {
    account_name casino_account = N(casino.1);

    create_account(casino_account);

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    bytes new_meta { 'n', 'e', 'w', ' ', 'm', 'e', 't', 'a' };

    base_tester::push_action(platform_name, N(setmetacas), platform_name, mvo()
        ("id", 0)
        ("meta", new_meta)
    );

    auto casino = get_casino(0);

    BOOST_TEST(casino["meta"].as<bytes>() == new_meta);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(casino_delete_test, platform_tester) try {
    account_name casino_account = N(casino.1);

    create_account(casino_account);

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(delcas), platform_name, mvo()
        ("id", 0)
    );

    auto casino = get_casino(0);

    BOOST_REQUIRE_EQUAL(casino.is_null(), true);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(auto_increment_test, platform_tester) try {
    std::vector<account_name> casinos = {
        N(casino.1),
        N(casino.2),
        N(casino.3),
        N(casino.4),
        N(casino.5),
    };

    for (auto i = 0; i < 3; ++i) {
        create_account(casinos[i]);

        base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
            ("contract", casinos[i])
            ("meta", bytes())
        );

        produce_blocks(2);

        auto casino = get_casino(i);
        BOOST_REQUIRE_EQUAL(casino["id"].as<uint64_t>(), i);
    }

    for (auto i = 0; i < 3; ++i) {
        base_tester::push_action(platform_name, N(delcas), platform_name, mvo()
            ("id", i)
        );
    }

    for (auto i = 3; i < 5; ++i) {
        create_account(casinos[i]);

        base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
            ("contract", casinos[i])
            ("meta", bytes())
        );

        produce_blocks(2);

        auto casino = get_casino(i);
        BOOST_REQUIRE_EQUAL(casino.is_null(), false);
        BOOST_REQUIRE_EQUAL(casino["id"].as<uint64_t>(), i);
    }

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
} // namespace testing
