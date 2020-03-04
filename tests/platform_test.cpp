#include "basic_tester.hpp"


namespace testing {

class platform_tester : public basic_tester {
public:
    const static account_name platform_name;

public:
    platform_tester() {
        create_accounts({
            platform_name
        });

        produce_blocks(2);

        deploy_contract<contracts::platform>(platform_name);
    }
};

const account_name platform_tester::platform_name = N(platform);


BOOST_AUTO_TEST_SUITE(platform_tests)


BOOST_FIXTURE_TEST_CASE(simple_test, platform_tester) try {
    std::cout << "OK\n";
} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()

} // namespace testing
