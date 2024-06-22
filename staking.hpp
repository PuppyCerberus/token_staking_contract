#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <eosio/singleton.hpp>
#include <vector>

using namespace eosio;

CONTRACT staking : public contract {
public:
    using contract::contract;

    ACTION stake(name user, asset quantity, uint32_t stake_term);
    ACTION unstake(name user, uint64_t stake_id);
    ACTION restake(name user, uint64_t stake_id);
    ACTION claim(name user, uint64_t stake_id);
    ACTION restakereward(name user, uint64_t stake_id);
    ACTION lookup(name user);
    ACTION addwl(name user);
    ACTION removewl(name user);

private:
    TABLE stakeinfo {
        uint64_t stake_id;
        name user;
        asset staked;
        uint32_t stake_term;
        time_point_sec stake_time;
        time_point_sec unstake_time;
        time_point_sec unstake_finish_time;
        asset rewards;

        uint64_t primary_key() const { return stake_id; }
        uint64_t by_user() const { return user.value; }
    };

    TABLE whitelistinfo {
        name user;

        uint64_t primary_key() const { return user.value; }
    };

    typedef eosio::multi_index<"stakes"_n, stakeinfo,
        indexed_by<"byuser"_n, const_mem_fun<stakeinfo, uint64_t, &stakeinfo::by_user>>
    > stakes_table;

    typedef eosio::multi_index<"whitelist"_n, whitelistinfo> whitelist_table;

    void update_rewards(name user, uint64_t stake_id);

    // Constants
    const uint32_t UNSTAKE_DELAY = 30 * 24 * 60 * 60; // 30 days in seconds
    const std::vector<uint32_t> STAKE_TERMS = {7 * 24 * 60 * 60, 30 * 24 * 60 * 60, 90 * 24 * 60 * 60, 180 * 24 * 60 * 60, 365 * 24 * 60 * 60};
    const double REWARD_RATE = 0.10; // Example reward rate, 10%
    const name TOKEN_CONTRACT = "pupadventure"_n; // token contract
    const symbol TOKEN_SYMBOL = symbol("GHOST", 4); // token symbol and precision
};
