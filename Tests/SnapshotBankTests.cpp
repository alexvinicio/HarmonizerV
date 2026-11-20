#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Dsp/SnapshotBank.h"

using namespace harmonizer::dsp;

namespace
{
SceneValues makeScene(float seed)
{
    SceneValues vals{};
    for (size_t i = 0; i < vals.size(); ++i)
        vals[i] = seed + static_cast<float>(i) * 0.25f;
    return vals;
}
} // namespace

TEST_CASE("SnapshotBank initializes with A populated/active and B/C/D empty", "[SnapshotBank]")
{
    SnapshotBank bank;
    const auto sceneA = makeScene(10.0f);
    bank.initializeFrom(sceneA, 0);

    REQUIRE(bank.getActiveIndex() == 0);
    REQUIRE(bank.isPopulated(0));
    REQUIRE_FALSE(bank.isPopulated(1));
    REQUIRE_FALSE(bank.isPopulated(2));
    REQUIRE_FALSE(bank.isPopulated(3));
    REQUIRE_FALSE(bank.isDirty());
    REQUIRE(bank.getSlot(0).values[0] == Catch::Approx(sceneA[0]));
}

TEST_CASE("SnapshotBank clones active scene on first empty slot activation, saves edits on switch, and preserves clean scenes", "[SnapshotBank]")
{
    SnapshotBank bank;
    const auto sceneA = makeScene(10.0f);
    bank.initializeFrom(sceneA, 0);

    // First click on empty B seeds B from A without pushing overwriting values
    SceneValues out{};
    REQUIRE_FALSE(bank.selectSlot(1, sceneA, out));
    REQUIRE(bank.getActiveIndex() == 1);
    REQUIRE(bank.isPopulated(1));
    REQUIRE(bank.getSlot(1).values[4] == Catch::Approx(sceneA[4]));

    // Simulate editing B
    const auto sceneB = makeScene(42.0f);
    bank.markDirty();
    REQUIRE(bank.selectSlot(0, sceneB, out));
    REQUIRE(bank.getActiveIndex() == 0);
    REQUIRE(out[0] == Catch::Approx(sceneA[0]));
    REQUIRE(out[12] == Catch::Approx(sceneA[12]));

    // Clean automation switch A -> B -> A must recall B & A cleanly without clobbering
    REQUIRE(bank.selectSlot(1, makeScene(999.0f), out)); // dirty==false so stale arg ignored
    REQUIRE(out[0] == Catch::Approx(sceneB[0]));
    REQUIRE(bank.selectSlot(0, makeScene(888.0f), out));
    REQUIRE(out[0] == Catch::Approx(sceneA[0]));
}

TEST_CASE("SnapshotBank supports explicit store, copy, and clear operations", "[SnapshotBank]")
{
    SnapshotBank bank;
    bank.initializeFrom(makeScene(1.0f), 0);

    const auto sceneC = makeScene(77.0f);
    bank.storeSlot(2, sceneC);
    REQUIRE(bank.getActiveIndex() == 2);
    REQUIRE(bank.isPopulated(2));
    REQUIRE_FALSE(bank.isDirty());

    // Copy C -> B (inactive slot: returns false so host params untouched)
    SceneValues out{};
    REQUIRE_FALSE(bank.copySlot(2, 1, out));
    REQUIRE(bank.isPopulated(1));
    REQUIRE(bank.getSlot(1).values[3] == Catch::Approx(sceneC[3]));

    // Copy A -> C (active slot: returns true with A's values to apply)
    REQUIRE(bank.copySlot(0, 2, out));
    REQUIRE(out[0] == Catch::Approx(1.0f));

    // Cannot clear active slot C, can clear inactive slot B
    REQUIRE_FALSE(bank.clearSlot(2));
    REQUIRE(bank.isPopulated(2));
    REQUIRE(bank.clearSlot(1));
    REQUIRE_FALSE(bank.isPopulated(1));
}
