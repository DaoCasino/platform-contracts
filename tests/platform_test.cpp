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


BOOST_AUTO_TEST_SUITE_END()

} // namespace testing
