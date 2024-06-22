#include "staking.hpp"

ACTION staking::stake(name user, asset quantity, uint32_t stake_term) {
    require_auth(user);
    check(quantity.amount > 0, "Stake amount must be positive.");
    check(quantity.symbol == TOKEN_SYMBOL, "Invalid token symbol.");
    check(std::find(STAKE_TERMS.begin(), STAKE_TERMS.end(), stake_term) != STAKE_TERMS.end(), "Invalid stake term.");

    whitelist_table whitelist(get_self(), get_self().value);
    auto wl_itr = whitelist.find(user.value);
    check(wl_itr != whitelist.end(), "User is not whitelisted.");

    // Transfer tokens to the contract
    action(
        permission_level{user, "active"_n},
        TOKEN_CONTRACT, "transfer"_n,
        std::make_tuple(user, get_self(), quantity, std::string("Stake tokens"))
    ).send();

    stakes_table stakes(get_self(), get_self().value);

    stakes.emplace(user, [&](auto& row) {
        row.stake_id = stakes.available_primary_key();
        row.user = user;
        row.staked = quantity;
        row.stake_term = stake_term;
        row.stake_time = current_time_point();
        row.unstake_time = time_point_sec(0);
        row.unstake_finish_time = time_point_sec(0); // Initialize to zero
        row.rewards = asset(0, quantity.symbol);
    });
}

ACTION staking::unstake(name user, uint64_t stake_id) {
    require_auth(user);

    stakes_table stakes(get_self(), get_self().value);
    auto itr = stakes.find(stake_id);

    check(itr != stakes.end(), "Stake not found.");
    check(itr->user == user, "Unauthorized.");
    check(itr->unstake_time == time_point_sec(0), "Unstake already in progress.");

    stakes.modify(itr, user, [&](auto& row) {
        row.unstake_time = current_time_point();
        row.unstake_finish_time = time_point_sec(current_time_point().sec_since_epoch() + UNSTAKE_DELAY);
    });
}

ACTION staking::restake(name user, uint64_t stake_id) {
    require_auth(user);

    stakes_table stakes(get_self(), get_self().value);
    auto itr = stakes.find(stake_id);

    check(itr != stakes.end(), "Stake not found.");
    check(itr->user == user, "Unauthorized.");

    stakes.modify(itr, user, [&](auto& row) {
        row.stake_time = current_time_point();
        row.unstake_time = time_point_sec(0);
        row.unstake_finish_time = time_point_sec(0); // Reset the unstake finish time
    });
}

ACTION staking::claim(name user, uint64_t stake_id) {
    require_auth(user);

    stakes_table stakes(get_self(), get_self().value);
    auto itr = stakes.find(stake_id);

    check(itr != stakes.end(), "Stake not found.");
    check(itr->user == user, "Unauthorized.");
    update_rewards(user, stake_id);
    check(itr->rewards.amount > 0, "No rewards to claim.");

    // Transfer rewards logic here (e.g., calling a token contract)
    action(
        permission_level{get_self(), "active"_n},
        TOKEN_CONTRACT, "transfer"_n,
        std::make_tuple(get_self(), user, itr->rewards, std::string("Claim staking rewards"))
    ).send();

    stakes.modify(itr, user, [&](auto& row) {
        row.rewards = asset(0, itr->rewards.symbol);
    });
}

ACTION staking::restakereward(name user, uint64_t stake_id) {
    require_auth(user);

    stakes_table stakes(get_self(), get_self().value);
    auto itr = stakes.find(stake_id);

    check(itr != stakes.end(), "Stake not found.");
    check(itr->user == user, "Unauthorized.");
    update_rewards(user, stake_id);
    check(itr->rewards.amount > 0, "No rewards to restake.");

    stakes.modify(itr, user, [&](auto& row) {
        row.staked += itr->rewards;
        row.rewards = asset(0, itr->rewards.symbol);
    });
}

ACTION staking::lookup(name user) {
    require_auth(user);

    stakes_table stakes(get_self(), get_self().value);
    auto idx = stakes.get_index<"byuser"_n>();
    auto itr = idx.find(user.value);

    while (itr != idx.end() && itr->user == user) {
        print("Stake ID: ", itr->stake_id, "\n");
        print("Staked: ", itr->staked, "\n");
        print("Stake Term: ", itr->stake_term, " seconds\n");
        print("Stake Time: ", itr->stake_time.sec_since_epoch(), "\n");
        print("Unstake Time: ", itr->unstake_time.sec_since_epoch(), "\n");
        print("Unstake Finish Time: ", itr->unstake_finish_time.sec_since_epoch(), "\n");
        print("Rewards: ", itr->rewards, "\n");
        itr++;
    }
}

ACTION staking::addwl(name user) {
    require_auth(get_self());

    whitelist_table whitelist(get_self(), get_self().value);
    auto itr = whitelist.find(user.value);

    check(itr == whitelist.end(), "User already whitelisted.");

    whitelist.emplace(get_self(), [&](auto& row) {
        row.user = user;
    });
}

ACTION staking::removewl(name user) {
    require_auth(get_self());

    whitelist_table whitelist(get_self(), get_self().value);
    auto itr = whitelist.find(user.value);

    check(itr != whitelist.end(), "User not found in whitelist.");

    whitelist.erase(itr);
}

void staking::update_rewards(name user, uint64_t stake_id) {
    stakes_table stakes(get_self(), get_self().value);
    auto itr = stakes.find(stake_id);

    if (itr != stakes.end()) {
        // Calculate the duration of staking in seconds
        uint32_t duration = (current_time_point() - itr->stake_time).to_seconds();
        
        // Calculate the reward based on staked amount, duration, and reward rate
        uint64_t reward_amount = itr->staked.amount * REWARD_RATE * duration / (365 * 24 * 60 * 60); // Annual reward rate

        stakes.modify(itr, same_payer, [&](auto& row) {
            row.rewards = asset(reward_amount, row.staked.symbol);
        });
    }
}

EOSIO_DISPATCH(staking, (stake)(unstake)(restake)(claim)(restakereward)(lookup)(addwl)(removewl))
