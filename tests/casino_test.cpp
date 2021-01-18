#include "basic_tester.hpp"


namespace testing {

using bytes = std::vector<char>;
using game_params_type = std::vector<std::pair<uint16_t, uint64_t>>;

static constexpr int64_t seconds_per_day = 24 * 3600;
static constexpr int64_t seconds_per_month = 30 * seconds_per_day;

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

class casino_tester : public basic_tester {
public:
    static const account_name casino_account;
    static const account_name undefined_acc;

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

        set_authority(platform_name, N(gameaction), {get_public_key(platform_name, "gameaction")}, N(active));
        link_authority(platform_name, casino_account, N(gameaction), N(newplayer));
        link_authority(platform_name, casino_account, N(gameaction), N(newplayer.t));

        allow_token(CORE_SYM_NAME, CORE_SYM_PRECISION, N(eosio.token));
    }

    fc::variant get_game(uint64_t game_id) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(game), game_id );
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("game_row", data, abi_serializer_max_time);
    }

    fc::variant get_game_state(uint64_t game_id) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(gamestate), game_id );
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("game_state_row", data, abi_serializer_max_time);
    }

    asset get_game_balance(uint64_t game_id, symbol balance_symbol = symbol{CORE_SYM}) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(gametokens), game_id );
        if (data.empty()) {
            return asset(0LL, balance_symbol);
        }
        const auto balances = abi_ser[casino_account].binary_to_variant("game_tokens_row", data, abi_serializer_max_time)["balance"];
        return get_asset_from_map(balances, balance_symbol);
    }

    asset get_balance( const account_name& act, symbol balance_symbol = symbol{CORE_SYM} ) {
        name token_account = get_token_contract(balance_symbol);
        if (token_account == undefined_acc) {
            return asset(0, balance_symbol);
        }
        vector<char> data = get_row_by_account( token_account, act, N(accounts), account_name(balance_symbol.to_symbol_code().value) );
        return data.empty() ? asset(0, balance_symbol) : abi_ser[token_account].binary_to_variant("account", data, abi_serializer_max_time)["balance"].as<asset>();
    }

    fc::variant get_session_state(uint64_t ses_id) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(sessionstate), ses_id );
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("session_state_row", data, abi_serializer_max_time);
    }

    fc::variant get_global() {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(global), N(global) );
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("global_state", data, abi_serializer_max_time);
    }

    fc::variant get_global_tokens() {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(globaltokens), N(globaltokens) );
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("global_tokens_state", data, abi_serializer_max_time);
    }

    fc::variant get_bonus() {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(bonuspool), N(bonuspool) );
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("bonus_pool_state", data, abi_serializer_max_time);
    }

    asset get_bonus_balance(name player) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(bonusbalance), player.value);
        return data.empty() ? asset(0, symbol{CORE_SYM}) : abi_ser[casino_account].binary_to_variant("bonus_balance_row", data, abi_serializer_max_time)["balance"].as<asset>();
    }

    fc::variant get_player_stats(name player) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(playerstats), player);
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("player_stats_row", data, abi_serializer_max_time);
    }

    fc::variant get_player_tokens(name player, symbol balance_symbol = symbol{CORE_SYM}) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(playertokens), player.value );
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("player_tokens_row", data, abi_serializer_max_time);
    }

    action_result push_action_custom_auth(const action_name& contract,
                                        const action_name& name,
                                        const permission_level& auth,
                                        const permission_level& key,
                                        const variant_object& data) {
        using namespace eosio;
        using namespace eosio::chain;

        string action_type_name = abi_ser[contract].get_action_type(name);

        action act;
        act.account = contract;
        act.name = name;
        act.data = abi_ser[contract].variant_to_binary(action_type_name, data, abi_serializer_max_time);

        act.authorization.push_back(auth);

        signed_transaction trx;
        trx.actions.emplace_back(std::move(act));
        set_transaction_headers(trx);
        trx.sign(get_private_key(key.actor, key.permission.to_string()), control->get_chain_id());
        try {
            push_transaction(trx);
        } catch (const fc::exception& ex) {
            edump((ex.to_detail_string()));
            return error(ex.top_message()); // top_message() is assumed by many tests; otherwise they fail
                                            // return error(ex.to_detail_string());
        }
        produce_block();
        BOOST_REQUIRE_EQUAL(true, chain_has_transaction(trx.id()));
        return success();
    }

    action_result push_action_custom_auth(const action_name& contract,
                              const action_name& name,
                              const permission_level& auth,
                              const variant_object& data) {
        return push_action_custom_auth(contract, name, auth, auth, data);
    }

    fc::variant get_game_no_bonus(uint64_t game_id) {
        vector<char> data = get_row_by_account(casino_account, casino_account, N(gamesnobon), game_id);
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("games_no_bonus_row", data, abi_serializer_max_time);
    }

    fc::variant get_token(std::string token_name) {
        const uint64_t pk = get_token_pk(token_name);
        vector<char> data = get_row_by_account(casino_account, casino_account, N(token), pk);
        return data.empty() ? fc::variant() : abi_ser[casino_account].binary_to_variant("token_row", data, abi_serializer_max_time);
    }

    asset get_asset_from_map(const fc::variant& data, symbol symbol) {
        auto amount = 0;
        for (auto& it : data.as<vector<fc::variant>>()) {
            if (it["key"].as<uint64_t>() == symbol.value()) {
                amount = it["value"].as<int64_t>();
            }
        }
        return asset(amount, symbol);
    }

    time_point get_time_from_map(const fc::variant& data, symbol symbol) {
        for (auto& it : data.as<vector<fc::variant>>()) {
            if (it["key"].as<uint64_t>() == symbol.value()) {
                return it["value"].as<time_point>();
            }
        }
        throw std::runtime_error("symbol not found");
    }

    game_params_type get_game_params(uint64_t game_id, const std::string& token = "BET") {
        game_params_type params = {};
        vector<char> data = get_row_by_account(casino_account, casino_account, N(gameparams), game_id);
        if (data.empty()) {
            return params;
        }
        const auto params_raw = abi_ser[casino_account].binary_to_variant("game_params_row", data, abi_serializer_max_time)["params"];
        const auto token_raw = get_token_pk(token);
        for (auto& it : params_raw.as<vector<fc::variant>>()) {
            if (it["key"].as<uint64_t>() == token_raw) {
                for (auto& pair: it["value"].as<vector<fc::variant>>()) {
                    params.push_back({pair["first"].as<uint16_t>(), pair["second"].as<uint64_t>()});
                }
                return params;
            }
        }
        return params;
    }

    void allow_token(const std::string& token_name, uint8_t precision, name contract) {
        create_account(contract);
        deploy_contract<contracts::system::token>(contract);

        symbol symb = symbol{string_to_symbol_c(precision, token_name.c_str())};
        create_currency( contract, config::system_account_name, asset(100000000000000, symb) );
        issue( config::system_account_name, asset(1672708210000, symb), config::system_account_name, contract );

        push_action(platform_name, N(addtoken), platform_name, mvo()
            ("token_name", token_name)
            ("contract", contract)
        );
        push_action(casino_account, N(addtoken), casino_account, mvo()
            ("token_name", token_name)
        );
    }

    name get_token_contract(const symbol symbol) {
        vector<char> data = get_row_by_account( platform_name, platform_name, N(token), symbol.to_symbol_code().value );
        return data.empty() ? undefined_acc : abi_ser[platform_name].binary_to_variant("token_row", data, abi_serializer_max_time)["contract"].as<name>();
    }

    action_result transfer( const name& from, const name& to, const asset& amount, const std::string& memo = "") {
        name token_contract = get_token_contract(amount.get_symbol());
        if (token_contract == undefined_acc) {
            return wasm_assert_msg("token is not in the list");
        }
        return push_action( token_contract, N(transfer), from, mutable_variant_object()
                                ("from",     from)
                                ("to",       to)
                                ("quantity", amount)
                                ("memo",     memo) );
    }
};

const account_name casino_tester::casino_account = N(dao.casino);
const account_name casino_tester::undefined_acc = N(undefined);

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

BOOST_FIXTURE_TEST_CASE(remove_game, casino_tester) try {

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


BOOST_FIXTURE_TEST_CASE(remove_game_active_session_failure, casino_tester) try {
    name player_account = N(game.boy);
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
        push_action(casino_account, N(newsession), casino_account, mvo()
            ("game_account", casino_account)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(newsessionpl), casino_account, mvo()
            ("game_account", casino_account)
            ("player_account", player_account)
        )
    );

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("trying to remove a game with non-zero active sessions"),
        push_action(casino_account, N(rmgame), casino_account, mvo()
            ("game_id", 0)
        )
    );
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
    transfer(game_account, casino_account, STRSYM("3.0000"));
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
        push_action(platform_name, N(setmargin), platform_name, mvo()
            ("id", 0)
            ("profit_margin", 50)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );

    transfer(game_account, casino_account, STRSYM("3.0000"));
    BOOST_REQUIRE_EQUAL(get_game_balance(0), STRSYM("1.5000"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("303.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(game_account), STRSYM("0.0000"));

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};
    allow_token("KEK", 5, N(token.kek));
    transfer(config::system_account_name, game_account, ASSET("3.00000 KEK"));
    transfer(game_account, casino_account, ASSET("3.00000 KEK"));

    BOOST_REQUIRE_EQUAL(get_game_balance(0, kek_symbol), ASSET("1.50000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account, kek_symbol), ASSET("3.00000 KEK"));
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

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(pausegame), casino_account, mvo()
            ("game_id", 0)
            ("pause", true)
        )
    );

    transfer(config::system_account_name, game_account, STRSYM("3.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));

    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("300.0000"));
    BOOST_REQUIRE_EQUAL(success(),
        transfer(game_account, casino_account, STRSYM("3.0000"))
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(on_loss_update_game_balance, casino_tester) try {
    name game_account = N(game.boy);
    name player_account = N(din.don);

    create_accounts({
        game_account,
        player_account
    });
    transfer(config::system_account_name, casino_account, STRSYM("5.0000"));

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
    BOOST_REQUIRE_EQUAL(get_game_balance(0), STRSYM("-1.5000"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("2.0000"));
    
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token is not in the list"),
        push_action(casino_account, N(onloss), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", ASSET("3.00000 KEK"))
        )
    );

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};
    allow_token("KEK", 5, N(token.kek));
    transfer(config::system_account_name, casino_account, ASSET("5.00000 KEK"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(onloss), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", ASSET("3.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(get_balance(player_account, kek_symbol), ASSET("3.00000 KEK"));
    BOOST_REQUIRE_EQUAL(get_game_balance(0, kek_symbol), ASSET("-1.50000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account, kek_symbol), ASSET("2.00000 KEK"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(on_loss_from_nonexistent_casino_game, casino_tester) try {
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
        wasm_assert_msg("game not found"),
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

BOOST_FIXTURE_TEST_CASE(claim_profit, casino_tester) try {
    name game_account = N(game.boy);
    name game_beneficiary_account = N(din.don);

    create_accounts({
        game_account,
        game_beneficiary_account
    });
    transfer(config::system_account_name, game_account, STRSYM("3.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};
    allow_token("KEK", 5, N(token.kek));
    transfer(config::system_account_name, game_account, ASSET("3.00000 KEK"));
    transfer(config::system_account_name, casino_account, ASSET("300.00000 KEK"));

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
        transfer(game_account, casino_account, STRSYM("3.0000"))
    );

    BOOST_REQUIRE_EQUAL(success(),
        transfer(game_account, casino_account, ASSET("3.00000 KEK"))
    );

    produce_block(fc::seconds(seconds_per_month + 1));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(claimprofit), game_account, mvo()
            ("game_account", game_account)
        )
    );

    BOOST_REQUIRE_EQUAL(get_game_balance(0), STRSYM("0.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("301.5000"));
    BOOST_REQUIRE_EQUAL(get_balance(game_beneficiary_account), STRSYM("1.5000"));
    BOOST_REQUIRE_EQUAL(get_balance(game_account), STRSYM("0.0000"));

    BOOST_REQUIRE_EQUAL(get_game_balance(0, kek_symbol), ASSET("0.00000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account, kek_symbol), ASSET("301.50000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(game_beneficiary_account, kek_symbol), ASSET("1.50000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(game_account, kek_symbol), ASSET("0.00000 KEK"));

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
    name player_account = N(player.acc);
    name game_beneficiary_account = N(din.don);
    name casino_beneficiary_account = N(don.din);

    create_accounts({
        game_account,
        player_account,
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
        transfer(game_account, casino_account, STRSYM("10.0000"))
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
        push_action(casino_account, N(newsession), game_account, mvo()
            ("game_account", game_account)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(newsessionpl), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
        )
    );

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

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token is not in the list"),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", ASSET("1.00000 KEK"))
        )
    );

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};
    allow_token("KEK", 5, N(token.kek));
    transfer(config::system_account_name, game_account, ASSET("100.00000 KEK"));
    transfer(config::system_account_name, casino_account, ASSET("290.00000 KEK"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sesupdate), game_account, mvo()
            ("game_account", game_account)
            ("max_win_delta", ASSET("10.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        transfer(game_account, casino_account, ASSET("10.00000 KEK"))
    );

    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_global_tokens()["game_active_sessions_sum"], kek_symbol), 
        ASSET("10.00000 KEK")
    );
    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_global_tokens()["game_profits_sum"], kek_symbol), 
        ASSET("5.00000 KEK")
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", ASSET("30.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(get_balance(casino_account, kek_symbol), ASSET("270.00000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_beneficiary_account, kek_symbol), ASSET("30.00000 KEK"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(newsession), game_account, mvo()
            ("game_account", game_account)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(newsessionpl), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sesupdate), game_account, mvo()
            ("game_account", game_account)
            ("max_win_delta", ASSET("300.00000 KEK"))
        )
    );

    // max transfer is 27 now

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("quantity exceededs max transfer amount"),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", ASSET("30.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("already claimed within past week"),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", ASSET("20.00000 KEK"))
        )
    );

    produce_block(fc::seconds(seconds_per_month + 1));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sesclose), game_account, mvo()
            ("game_account", game_account)
            ("quantity", ASSET("310.00000 KEK"))
        )
    );

    // claim all except game profits

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", ASSET("265.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(get_balance(casino_beneficiary_account, kek_symbol), ASSET("295.00000 KEK"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(withdraw_negative_profits, casino_tester) try {
    name game_account = N(game.boy);
    name player_account = N(din.don);
    name casino_beneficiary_account = N(don.din);

    create_accounts({
        game_account,
        player_account,
        casino_beneficiary_account
    });
    transfer(config::system_account_name, casino_account, STRSYM("100.0000"));

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
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(onloss), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", STRSYM("50.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("quantity exceededs max transfer amount"),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", STRSYM("75.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", STRSYM("50.0000"))
        )
    );

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};
    allow_token("KEK", 5, N(token.kek));
    transfer(config::system_account_name, casino_account, ASSET("100.00000 KEK"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(onloss), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", ASSET("50.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("quantity exceededs max transfer amount"),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", ASSET("75.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", casino_beneficiary_account)
            ("quantity", ASSET("50.00000 KEK"))
        )
    );
} FC_LOG_AND_RETHROW()

// bonus

BOOST_FIXTURE_TEST_CASE(bonus, casino_tester) try {
    name bonus_admin = N(admin.bon);
    name bonus_hunter = N(hunter.bon);
    name player = N(player);

    create_accounts({bonus_admin, bonus_hunter, player});
    transfer(config::system_account_name, casino_account, STRSYM("1000.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(setadminbon), casino_account, mvo()
            ("new_admin", bonus_admin)
        )
    );
    BOOST_REQUIRE_EQUAL(get_bonus()["admin"].as<name>(), bonus_admin);

    transfer(config::system_account_name, casino_account, STRSYM("100.0000"), "bonus");
    BOOST_REQUIRE_EQUAL(get_bonus()["total_allocated"].as<asset>(), STRSYM("100.0000"));
    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdrawbon), casino_account, mvo()
            ("to", bonus_hunter)
            ("quantity", STRSYM("3.0000"))
            ("memo", "")
        )
    );
    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("1097.0000"));
    BOOST_REQUIRE_EQUAL(get_bonus()["total_allocated"].as<asset>(), STRSYM("97.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(bonus_hunter), STRSYM("3.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sendbon), bonus_admin, mvo()
            ("to", player)
            ("amount", STRSYM("100.0000"))
            ("memo", "")
        )
    );

    BOOST_REQUIRE_EQUAL(get_bonus_balance(player), STRSYM("100.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(subtractbon), bonus_admin, mvo()
            ("from", player)
            ("amount", STRSYM("50.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(get_bonus_balance(player), STRSYM("50.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(convertbon), bonus_admin, mvo()
            ("account", player)
            ("memo", "")
        )
    );

    BOOST_REQUIRE_EQUAL(get_bonus()["total_allocated"].as<asset>(), STRSYM("47.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(player), STRSYM("50.0000"));
    BOOST_REQUIRE_EQUAL(get_bonus_balance(player), STRSYM("0.0000"));

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};

    transfer(config::system_account_name, casino_account, ASSET("100.00000 KEK"), "bonus");
    
    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_global_tokens()["total_allocated_bonus"], kek_symbol), 
        ASSET("0.00000 KEK")
    );

    allow_token("KEK", 5, N(token.kek));
    transfer(config::system_account_name, casino_account, ASSET("1000.00000 KEK"));
    transfer(config::system_account_name, casino_account, ASSET("100.00000 KEK"), "bonus");

    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_global_tokens()["total_allocated_bonus"], kek_symbol), 
        ASSET("100.00000 KEK")
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdrawbon), casino_account, mvo()
            ("to", bonus_hunter)
            ("quantity", ASSET("3.00000 KEK"))
            ("memo", "")
        )
    );

    BOOST_REQUIRE_EQUAL(get_balance(casino_account, kek_symbol), ASSET("1097.00000 KEK"));
    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_global_tokens()["total_allocated_bonus"], kek_symbol), 
        ASSET("97.00000 KEK")
    );
    BOOST_REQUIRE_EQUAL(get_balance(bonus_hunter, kek_symbol), ASSET("3.00000 KEK"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sendbon), bonus_admin, mvo()
            ("to", player)
            ("amount", ASSET("100.00000 KEK"))
            ("memo", "")
        )
    );

    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player)["bonus_balance"], kek_symbol), 
        ASSET("100.00000 KEK")
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(subtractbon), bonus_admin, mvo()
            ("from", player)
            ("amount", ASSET("50.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player)["bonus_balance"], kek_symbol),
        ASSET("50.00000 KEK")
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(convertbon.t), bonus_admin, mvo()
            ("account", player)
            ("symbol", kek_symbol)
            ("memo", "")
        )
    );

    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_global_tokens()["total_allocated_bonus"], kek_symbol), 
        ASSET("47.00000 KEK")
    );
    BOOST_REQUIRE_EQUAL(get_balance(player, kek_symbol), ASSET("50.00000 KEK"));
    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player)["bonus_balance"], kek_symbol), 
        ASSET("0.00000 KEK")
    );
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(ses_bonus_lock_add, casino_tester) try {
    name game_account = N(game.boy);
    name player_account = N(player.acc);
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
        push_action(casino_account, N(addgame), casino_account, mvo()
            ("game_id", 0)
            ("params", game_params_type{{0, 0}})
        )
    );

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("player has no bonus"),
        push_action(casino_account, N(seslockbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", STRSYM("100.0000"))
        )
    );

    transfer(config::system_account_name, casino_account, STRSYM("100.0000"), "bonus");

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sendbon), casino_account, mvo()
            ("to", player_account)
            ("amount", STRSYM("100.0000"))
            ("memo", "")
        )
    );

    BOOST_REQUIRE_EQUAL(get_bonus_balance(player_account), STRSYM("100.0000"));
    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(seslockbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", STRSYM("100.0000"))
        )
    );
    BOOST_REQUIRE_EQUAL(get_bonus_balance(player_account), STRSYM("0.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sesaddbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", STRSYM("200.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(get_bonus_balance(player_account), STRSYM("200.0000"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token is not in the list"),
        push_action(casino_account, N(seslockbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", ASSET("100.00000 KEK"))
        )
    );

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};
    allow_token("KEK", 5, N(token.kek));
    transfer(config::system_account_name, game_account, ASSET("3.00000 KEK"));
    transfer(config::system_account_name, casino_account, ASSET("300.00000 KEK"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("lock amount cannot exceed player's bonus balance"),
        push_action(casino_account, N(seslockbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", ASSET("100.00000 KEK"))
        )
    );

    transfer(config::system_account_name, casino_account, ASSET("100.00000 KEK"), "bonus");

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sendbon), casino_account, mvo()
            ("to", player_account)
            ("amount", ASSET("100.00000 KEK"))
            ("memo", "")
        )
    );

    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player_account)["bonus_balance"], kek_symbol), 
        ASSET("100.00000 KEK")
    );    
    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(seslockbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", ASSET("100.00000 KEK"))
        )
    );
    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player_account)["bonus_balance"], kek_symbol), 
        ASSET("0.00000 KEK")
    );  
    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sesaddbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", ASSET("200.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player_account)["bonus_balance"], kek_symbol), 
        ASSET("200.00000 KEK")
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(pool_withdraw, casino_tester) try {
    name beneficiary = N(bene.bene);
    name bonus_acc = N(bonus.acc);
    create_accounts({beneficiary, bonus_acc});
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("100.0000"), "bonus");

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("quantity exceededs max transfer amount"),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", beneficiary)
            ("quantity", STRSYM("400.0000"))
        )
    );
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("withdraw quantity cannot exceed total bonus"),
        push_action(casino_account, N(withdrawbon), casino_account, mvo()
            ("to", bonus_acc)
            ("quantity", STRSYM("200.0000"))
            ("memo", "")
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", beneficiary)
            ("quantity", STRSYM("300.0000"))
        )
    );
    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdrawbon), casino_account, mvo()
            ("to", bonus_acc)
            ("quantity", STRSYM("100.0000"))
            ("memo", "")
        )
    );
    BOOST_REQUIRE_EQUAL(get_balance(beneficiary), STRSYM("300.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(bonus_acc), STRSYM("100.0000"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token is not in the list"),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", beneficiary)
            ("quantity", ASSET("400.00000 KEK"))
        )
    );
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token is not in the list"),
        push_action(casino_account, N(withdrawbon), casino_account, mvo()
            ("to", bonus_acc)
            ("quantity", ASSET("200.00000 KEK"))
            ("memo", "")
        )
    );

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};
    allow_token("KEK", 5, N(token.kek));
    transfer(config::system_account_name, casino_account, ASSET("300.00000 KEK"));
    transfer(config::system_account_name, casino_account, ASSET("100.00000 KEK"), "bonus");

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("quantity exceededs max transfer amount"),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", beneficiary)
            ("quantity", ASSET("400.00000 KEK"))
        )
    );
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("withdraw quantity cannot exceed total bonus"),
        push_action(casino_account, N(withdrawbon), casino_account, mvo()
            ("to", bonus_acc)
            ("quantity", ASSET("200.00000 KEK"))
            ("memo", "")
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdraw), casino_account, mvo()
            ("beneficiary_account", beneficiary)
            ("quantity", ASSET("300.00000 KEK"))
        )
    );
    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(withdrawbon), casino_account, mvo()
            ("to", bonus_acc)
            ("quantity", ASSET("100.00000 KEK"))
            ("memo", "")
        )
    );
    BOOST_REQUIRE_EQUAL(get_balance(beneficiary, kek_symbol), ASSET("300.00000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(bonus_acc, kek_symbol), ASSET("100.00000 KEK"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(player_stats, casino_tester) try {
    name game_account = N(game.acc);
    name player_account = N(game.boy);
    create_accounts({game_account, player_account});

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
        push_action(casino_account, N(newsessionpl), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
        )
    );

    // just update stats
    BOOST_REQUIRE_EQUAL(get_player_stats(player_account)["sessions_created"].as<int>(), 1);

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sesnewdepo2), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", STRSYM("10.0000"))
        )
    );
    BOOST_REQUIRE_EQUAL(get_player_stats(player_account)["profit_real"].as<asset>(), STRSYM("-10.0000"));
    BOOST_REQUIRE_EQUAL(get_player_stats(player_account)["volume_real"].as<asset>(), STRSYM("10.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sespayout), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", STRSYM("20.0000"))
        )
    );
    BOOST_REQUIRE_EQUAL(get_player_stats(player_account)["profit_real"].as<asset>(), STRSYM("10.0000"));


    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sendbon), casino_account, mvo()
            ("to", player_account)
            ("amount", STRSYM("200.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(seslockbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", STRSYM("100.0000"))
        )
    );
    BOOST_REQUIRE_EQUAL(get_player_stats(player_account)["profit_bonus"].as<asset>(), STRSYM("-100.0000"));
    BOOST_REQUIRE_EQUAL(get_player_stats(player_account)["volume_bonus"].as<asset>(), STRSYM("100.0000"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token is not in the list"),
        push_action(casino_account, N(sesnewdepo2), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", ASSET("10.00000 KEK"))
        )
    );

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};
    allow_token("KEK", 5, N(token.kek));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sesnewdepo2), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", ASSET("10.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player_account)["profit_real"], kek_symbol), 
        ASSET("-10.00000 KEK")
    );
    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player_account)["volume_real"], kek_symbol), 
        ASSET("10.00000 KEK")
    );
    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sespayout), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("quantity", ASSET("20.00000 KEK"))
        )
    );
    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player_account)["profit_real"], kek_symbol), 
        ASSET("10.00000 KEK")
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sendbon), casino_account, mvo()
            ("to", player_account)
            ("amount", ASSET("200.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(seslockbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", ASSET("100.00000 KEK"))
        )
    );
    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player_account)["profit_bonus"], kek_symbol), 
        ASSET("-100.00000 KEK")
    );
    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player_account)["volume_bonus"], kek_symbol), 
        ASSET("100.00000 KEK")
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(add_new_player, casino_tester) try {
    const name player = N(player.x);
    create_account(player);

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(setgreetbon), casino_account, mvo()
            ("amount", STRSYM("1.0000"))
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action_custom_auth(casino_account, N(newplayer), {platform_name, N(gameaction)}, mvo()
            ("player_account", player)
        )
    );

    BOOST_REQUIRE_EQUAL(get_bonus_balance(player), STRSYM("1.0000"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token is not in the list"),
        push_action(casino_account, N(setgreetbon), casino_account, mvo()
            ("amount", ASSET("1.00000 KEK"))
        )
    );

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};
    allow_token("KEK", 5, N(token.kek));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(setgreetbon), casino_account, mvo()
            ("amount", ASSET("1.00000 KEK"))
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action_custom_auth(casino_account, N(newplayer.t), {platform_name, N(gameaction)}, mvo()
            ("player_account", player)
            ("token", "KEK")
        )
    );
    
    BOOST_REQUIRE_EQUAL(
        get_asset_from_map(get_player_tokens(player)["bonus_balance"], kek_symbol), 
        ASSET("1.00000 KEK")
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(games_no_bonus, casino_tester) try {
    name game_account = N(game.boy);
    name player_account = N(player.acc);

    create_accounts({game_account, player_account});

    transfer(config::system_account_name, casino_account, STRSYM("100.0000"), "bonus");

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
        push_action(casino_account, N(addgamenobon), casino_account, mvo()
            ("game_account", game_account)
        )
    );

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("game is already restricted"),
        push_action(casino_account, N(addgamenobon), casino_account, mvo()
            ("game_account", game_account)
        )
    );

    BOOST_REQUIRE_EQUAL(get_game_no_bonus(0)["game_id"].as<uint64_t>(), 0);

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(sendbon), casino_account, mvo()
            ("to", player_account)
            ("amount", STRSYM("100.0000"))
            ("memo", "")
        )
    );
    BOOST_REQUIRE_EQUAL(get_bonus_balance(player_account), STRSYM("100.0000"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("game is restricted to bonus"),
        push_action(casino_account, N(seslockbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", STRSYM("100.0000"))
        )
    );
    BOOST_REQUIRE_EQUAL(get_bonus_balance(player_account), STRSYM("100.0000"));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(rmgamenobon), casino_account, mvo()
            ("game_account", game_account)
        )
    );

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("game is not restricted"),
        push_action(casino_account, N(rmgamenobon), casino_account, mvo()
            ("game_account", game_account)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(seslockbon), game_account, mvo()
            ("game_account", game_account)
            ("player_account", player_account)
            ("amount", STRSYM("100.0000"))
        )
    );
    BOOST_REQUIRE_EQUAL(get_bonus_balance(player_account), STRSYM("0.0000"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(legacy_actions, casino_tester) try {
    const name game_account = N(game.acc);
    create_account(game_account);

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
        push_action(casino_account, N(sesnewdepo), game_account, mvo()
            ("game_account", game_account)
            ("quantity", STRSYM("10.0000"))
        )
    );
    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(newsession), game_account, mvo()
            ("game_account", game_account)
        )
    );

    // newsession is just a stub
    BOOST_REQUIRE_EQUAL(get_global()["active_sessions_amount"].as<int>(), 1);
    BOOST_REQUIRE_EQUAL(get_game_state(0)["active_sessions_amount"].as<int>(), 1);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(token, casino_tester) try {
    const name token_eth = N(token.eth);
    const name token_btc = N(token.btc);

    BOOST_REQUIRE_EQUAL(success(),
        push_action(platform_name, N(addtoken), platform_name, mvo()
            ("token_name", "DETH")
            ("contract", token_eth)
        )
    );

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token is not in the list"),
        push_action(casino_account, N(addtoken), casino_account, mvo()
            ("token_name", "EOS")
        )
    );

    create_account(token_eth);
    deploy_contract<contracts::system::token>(token_eth);
    symbol symb_eth = symbol{string_to_symbol_c(4, "DETH")};
    create_currency( token_eth, config::system_account_name, asset(100000000000000, symb_eth) );
    issue( config::system_account_name, asset(1672708210000, symb_eth), config::system_account_name, token_eth );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(addtoken), casino_account, mvo()
            ("token_name", "DETH")
        )
    );

    BOOST_REQUIRE_EQUAL(get_token("DETH")["token_name"].as<std::string>(), "DETH");
    BOOST_REQUIRE_EQUAL(get_token("DETH")["paused"].as<bool>(), false);

    // test pause

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(pausetoken), casino_account, mvo()
            ("token_name", "DETH")
            ("pause", true)
        )
    );

    BOOST_REQUIRE_EQUAL(get_token("DETH")["paused"].as<bool>(), true);

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(pausetoken), casino_account, mvo()
            ("token_name", "DETH")
            ("pause", false)
        )
    );

    BOOST_REQUIRE_EQUAL(get_token("DETH")["paused"].as<bool>(), false);

    // test error messages

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(rmtoken), casino_account, mvo()
            ("token_name", "DETH")
        )
    );

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token is not supported"),
        push_action(casino_account, N(rmtoken), casino_account, mvo()
            ("token_name", "DETH")
        )
    );

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token is not supported"),
        push_action(casino_account, N(pausetoken), casino_account, mvo()
            ("token_name", "DETH")
            ("pause", false)
        )
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(claim_profit_on_game_remove, casino_tester) try {
    name game_account = N(game.boy);
    name game_beneficiary_account = N(din.don);

    create_accounts({
        game_account,
        game_beneficiary_account
    });
    transfer(config::system_account_name, game_account, STRSYM("10.0000"));
    transfer(config::system_account_name, casino_account, STRSYM("300.0000"));

    symbol kek_symbol = symbol{string_to_symbol_c(5, "KEK")};
    allow_token("KEK", 5, N(token.kek));
    transfer(config::system_account_name, game_account, ASSET("10.00000 KEK"));
    transfer(config::system_account_name, casino_account, ASSET("300.00000 KEK"));

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
        transfer(game_account, casino_account, STRSYM("5.0000"))
    );

    BOOST_REQUIRE_EQUAL(success(),
        transfer(game_account, casino_account, ASSET("5.00000 KEK"))
    );

    produce_block(fc::seconds(seconds_per_month + 1));

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(claimprofit), game_account, mvo()
            ("game_account", game_account)
        )
    );

    BOOST_REQUIRE_EQUAL(get_game_balance(0), STRSYM("0.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("302.5000"));
    BOOST_REQUIRE_EQUAL(get_balance(game_beneficiary_account), STRSYM("2.5000"));
    BOOST_REQUIRE_EQUAL(get_balance(game_account), STRSYM("5.0000"));

    BOOST_REQUIRE_EQUAL(get_game_balance(0, kek_symbol), ASSET("0.00000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(casino_account, kek_symbol), ASSET("302.50000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(game_beneficiary_account, kek_symbol), ASSET("2.50000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(game_account, kek_symbol), ASSET("5.00000 KEK"));

    produce_block(fc::seconds(3));
    // second claim should fail
    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("already claimed within past month"),
        push_action(casino_account, N(claimprofit), game_account, mvo()
            ("game_account", game_account)
        )
    );

    BOOST_REQUIRE_EQUAL(success(),
        transfer(game_account, casino_account, STRSYM("5.0000"))
    );

    BOOST_REQUIRE_EQUAL(success(),
        transfer(game_account, casino_account, ASSET("5.00000 KEK"))
    );

    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(rmgame), casino_account, mvo()
            ("game_id", 0)
        )
    );

    BOOST_REQUIRE_EQUAL(get_balance(casino_account), STRSYM("305.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(game_beneficiary_account), STRSYM("5.0000"));
    BOOST_REQUIRE_EQUAL(get_balance(game_account), STRSYM("0.0000"));

    BOOST_REQUIRE_EQUAL(get_balance(casino_account, kek_symbol), ASSET("305.00000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(game_beneficiary_account, kek_symbol), ASSET("5.00000 KEK"));
    BOOST_REQUIRE_EQUAL(get_balance(game_account, kek_symbol), ASSET("0.00000 KEK"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_params_test, casino_tester) try {
    const auto expected_params = game_params_type{{0, 0}};

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
            ("params", expected_params)
        )
    );

    auto require_equal_game_params = [](const auto& params, const auto& expected_params) {
        BOOST_REQUIRE_EQUAL(params.size(), expected_params.size());
        for (int i = 0; i < params.size(); ++i) {
            BOOST_REQUIRE_EQUAL(params[i].first, expected_params[i].first);
            BOOST_REQUIRE_EQUAL(params[i].second, expected_params[i].second);
        }
    };

    const auto params = get_game_params(0);
    require_equal_game_params(params, expected_params);

    const auto kek_token = "KEK";
    allow_token(kek_token, 4, N(token.kek));
    const auto expected_params_kek = game_params_type{{0, 0}, {1, 2}};
    BOOST_REQUIRE_EQUAL(success(),
        push_action(casino_account, N(setgameparam2), casino_account, mvo()
            ("game_id", 0)
            ("token", "KEK")
            ("params", expected_params_kek)
        )
    );

    const auto params_kek = get_game_params(0, kek_token);
    require_equal_game_params(params_kek, expected_params_kek);
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()

} // namespace testing