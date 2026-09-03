#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <queue>
#include <vector>

#include "scheduler/policies/ISchedulingPolicy.hpp"
#include "scheduler/policies/FifoSchedulingPolicy.hpp"
#include "scheduler/policies/PrioritySchedulingPolicy.hpp"
#include "scheduler/policies/SchedulingPolicyCompare.hpp"
#include "functions/Functions.hpp"

namespace anasa
{
    using PendingQueue = std::priority_queue<PendingRenderTile, std::vector<PendingRenderTile>, SchedulingPolicyCompare>;

    TEST_CASE("Scheduler policy: FIFO dispatches the oldest submitted tile first")
    {
        std::shared_ptr<const ISchedulingPolicy> policy = std::make_shared<FifoSchedulingPolicy>();
        PendingQueue queue{SchedulingPolicyCompare(policy)};

        // FIFO must ignore urgency and preserve submission order.
        queue.push(functions::makeTile(RenderPriority::Urgent, 20));
        queue.push(functions::makeTile(RenderPriority::Background, 10));
        queue.push(functions::makeTile(RenderPriority::Visible, 30));

        REQUIRE(queue.top().sequence == 10);
        queue.pop();

        REQUIRE(queue.top().sequence == 20);
        queue.pop();

        REQUIRE(queue.top().sequence == 30);
    }

    TEST_CASE("Scheduler policy: Priority orders urgent before visible before background")
    {
        std::shared_ptr<const ISchedulingPolicy> policy = std::make_shared<PrioritySchedulingPolicy>();
        PendingQueue queue{SchedulingPolicyCompare(policy)};

        queue.push(functions::makeTile(RenderPriority::Background, 1));
        queue.push(functions::makeTile(RenderPriority::Visible, 2));
        queue.push(functions::makeTile(RenderPriority::Urgent, 3, 100));

        REQUIRE(queue.top().priority == RenderPriority::Urgent);
        queue.pop();

        REQUIRE(queue.top().priority == RenderPriority::Visible);
        queue.pop();

        REQUIRE(queue.top().priority == RenderPriority::Background);
    }

    TEST_CASE("Scheduler policy: Priority applies deadline distance and FIFO tie breakers")
    {
        PrioritySchedulingPolicy policy;

        const PendingRenderTile earlyDeadline = functions::makeTile(RenderPriority::Urgent, 20, 100, 1000);

        const PendingRenderTile lateDeadline = functions::makeTile(RenderPriority::Urgent, 10, 200, 1);

        REQUIRE(policy.hasLowerPrecedence(lateDeadline, earlyDeadline));
        REQUIRE_FALSE(policy.hasLowerPrecedence(earlyDeadline, lateDeadline));

        const PendingRenderTile nearVisible = functions::makeTile(RenderPriority::Visible, 20, -1, 100);

        const PendingRenderTile farVisible = functions::makeTile(RenderPriority::Visible, 10, -1, 500);

        REQUIRE(policy.hasLowerPrecedence(farVisible, nearVisible));
        REQUIRE_FALSE(policy.hasLowerPrecedence(nearVisible, farVisible));

        const PendingRenderTile older = functions::makeTile(RenderPriority::Background, 10, -1, 100);

        const PendingRenderTile newer = functions::makeTile(RenderPriority::Background, 20, -1, 100);

        REQUIRE(policy.hasLowerPrecedence(newer, older));
        REQUIRE_FALSE(policy.hasLowerPrecedence(older, newer));
    }

} // namespace anasa