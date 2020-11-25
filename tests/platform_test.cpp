#include "basic_tester.hpp"

namespace testing {

using bytes = std::vector<char>;

static uint64_t get_token_pk(const std::string& token_name) {
    // https://github.com/EOSIO/eosio.cdt/blob/1ba675ef4fe6dedc9f57a9982d1227a098bcaba9/libraries/eosiolib/core/eosio/symbol.hpp
    uint64_t value = 0;
    for( auto itr = token_name.rbegin(); itr != token_name.rend(); ++itr ) {
        if( *itr < 'A' || *itr > 'Z') {
            throw std::logic_error("only uppercase letters allowed in symbol_code string");
        }
        value <<= 8;
        value |= *itr;
    }
    return value;
}

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

    fc::variant get_token(std::string token_name) {
        const uint64_t pk = get_token_pk(token_name);
        vector<char> data = get_row_by_account(platform_name, platform_name, N(token), pk);
        return data.empty() ? fc::variant() : abi_ser[platform_name].binary_to_variant("token_row", data, abi_serializer_max_time);
    }
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

BOOST_FIXTURE_TEST_CASE(vesion_test, platform_tester) try {
    account_name casino_account = N(casino.1);

    create_account(casino_account);

    base_tester::push_action(platform_name, N(addcas), platform_name, mvo()
        ("contract", casino_account)
        ("meta", bytes())
    );

    vector<char> data = get_row_by_account(platform_name, platform_name, N(version), N(version) );
    BOOST_REQUIRE_EQUAL(data.empty(), false);

    auto version = abi_ser[platform_name].binary_to_variant("version_row", data, abi_serializer_max_time);
    BOOST_REQUIRE_EQUAL(version["version"], contracts::version());

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

BOOST_FIXTURE_TEST_CASE(casino_id_auto_increment_test, platform_tester) try {
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


BOOST_FIXTURE_TEST_CASE(add_game, platform_tester) try {
    account_name game_account = N(game.1);

    create_account(game_account);

    base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
        ("contract", game_account)
        ("params_cnt", 0)
        ("meta", bytes())
    );

    auto game = get_game(0);
    BOOST_REQUIRE(!game.is_null());

    BOOST_REQUIRE_EQUAL(game["contract"].as<account_name>(), game_account);
    BOOST_REQUIRE_EQUAL(game["meta"].as<bytes>().size(), 0);
    BOOST_REQUIRE_EQUAL(game["params_cnt"].as<uint64_t>(), 0);
    BOOST_REQUIRE_EQUAL(game["paused"].as<bool>(), false);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(add_many_games, platform_tester) try {
    std::vector<account_name> games = {
        N(game.1),
        N(game.2),
        N(game.3),
        N(game.4),
        N(game.5),
    };

    for (const auto& game: games) {
        create_account(game);

        base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", game)
            ("params_cnt", 0)
            ("meta", bytes())
        );
    }

    for (auto i = 0; i < games.size(); ++i) {
        auto game = get_game(i);
        BOOST_REQUIRE(!game.is_null());

        BOOST_REQUIRE_EQUAL(game["contract"].as<account_name>(), games[i]);
        BOOST_REQUIRE_EQUAL(game["meta"].as<bytes>().size(), 0);
        BOOST_REQUIRE_EQUAL(game["paused"].as<bool>(), false);
    }

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_pause_game, platform_tester) try {
    account_name game_account = N(game.1);

    create_account(game_account);

    base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
        ("contract", game_account)
        ("params_cnt", 0)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(pausegame), platform_name, mvo()
        ("id", 0)
        ("pause", true)
    );

    auto game = get_game(0);

    BOOST_REQUIRE_EQUAL(game["paused"].as<bool>(), true);

    base_tester::push_action(platform_name, N(pausegame), platform_name, mvo()
        ("id", 0)
        ("pause", false)
    );

    game = get_game(0);

    BOOST_REQUIRE_EQUAL(game["paused"].as<bool>(), false);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_contract_game, platform_tester) try {
    account_name game_account = N(game.1);
    account_name game_account_new = N(game.2);

    create_account(game_account);
    create_account(game_account_new);

    base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
        ("contract", game_account)
        ("params_cnt", 0)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(setcontrgame), platform_name, mvo()
        ("id", 0)
        ("contract", game_account_new)
    );

    auto game = get_game(0);

    BOOST_REQUIRE_EQUAL(game["contract"].as<account_name>(), game_account_new);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_meta_game, platform_tester) try {
    account_name game_account = N(game.1);

    create_account(game_account);

    base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
        ("contract", game_account)
        ("params_cnt", 0)
        ("meta", bytes())
    );

    bytes new_meta { 'n', 'e', 'w', ' ', 'm', 'e', 't', 'a' };

    base_tester::push_action(platform_name, N(setmetagame), platform_name, mvo()
        ("id", 0)
        ("meta", new_meta)
    );

    auto game = get_game(0);

    BOOST_TEST(game["meta"].as<bytes>() == new_meta);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(game_delete_test, platform_tester) try {
    account_name game_account = N(game.1);

    create_account(game_account);

    base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
        ("contract", game_account)
        ("params_cnt", 0)
        ("meta", bytes())
    );

    base_tester::push_action(platform_name, N(delgame), platform_name, mvo()
        ("id", 0)
    );

    auto game = get_game(0);

    BOOST_REQUIRE_EQUAL(game.is_null(), true);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(game_id_auto_increment_test, platform_tester) try {
    std::vector<account_name> games = {
        N(game.1),
        N(game.2),
        N(game.3),
        N(game.4),
        N(game.5),
    };

    for (auto i = 0; i < 3; ++i) {
        create_account(games[i]);

        base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", games[i])
            ("params_cnt", 0)
            ("meta", bytes())
        );

        produce_blocks(2);

        auto game = get_game(i);
        BOOST_REQUIRE_EQUAL(game["id"].as<uint64_t>(), i);
    }

    for (auto i = 0; i < 3; ++i) {
        base_tester::push_action(platform_name, N(delgame), platform_name, mvo()
            ("id", i)
        );
    }

    for (auto i = 3; i < 5; ++i) {
        create_account(games[i]);

        base_tester::push_action(platform_name, N(addgame), platform_name, mvo()
            ("contract", games[i])
            ("params_cnt", 0)
            ("meta", bytes())
        );

        produce_blocks(2);

        auto game = get_game(i);
        BOOST_REQUIRE_EQUAL(game.is_null(), false);
        BOOST_REQUIRE_EQUAL(game["id"].as<uint64_t>(), i);
    }

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(add_del_token, platform_tester) try {
    const name token_eth = N(token.eth);
    const name token_btc = N(token.btc);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("only uppercase letters allowed in symbol_code string"),
        push_action(platform_name, N(addtoken), platform_name, mvo()
            ("token_name", "dETH")
            ("contract", token_eth)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addtoken), platform_name, mvo()
            ("token_name", "DETH")
            ("contract", token_eth)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addtoken), platform_name, mvo()
            ("token_name", "DBTC")
            ("contract", token_btc)
        )
    );

    BOOST_REQUIRE_EQUAL(get_token("DETH")["token_name"].as<std::string>(), "DETH");
    BOOST_REQUIRE_EQUAL(get_token("DETH")["contract"].as<name>(), token_eth);

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(deltoken), platform_name, mvo()
            ("token_name", "DETH")
        )
    );

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("del token: no token found"),
        push_action(platform_name, N(deltoken), platform_name, mvo()
            ("token_name", "DETH")
        )
    );

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
} // namespace testing
