#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "assistant/EqChat.h"

using Catch::Matchers::WithinAbs;
using namespace puzzleq;

TEST_CASE ("EQ chat rolls off the low end")
{
    const auto plan = parseEqChat ("roll off the low end");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].kind == ChatOp::Kind::AddOrUpdate);
    REQUIRE (plan.ops[0].band.shape == FilterShape::LowCut);
    REQUIRE (plan.ops[0].band.active);
    REQUIRE_THAT (plan.ops[0].band.frequencyHz, WithinAbs (80.0f, 0.1f));
}

TEST_CASE ("EQ chat cleans mud")
{
    const auto plan = parseEqChat ("clean mud");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].band.shape == FilterShape::Bell);
    REQUIRE (plan.ops[0].band.gainDb < -1.0f);
    REQUIRE_THAT (plan.ops[0].band.frequencyHz, WithinAbs (280.0f, 1.0f));
}

TEST_CASE ("EQ chat boosts presence")
{
    const auto plan = parseEqChat ("boost presence");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].band.gainDb > 1.0f);
    REQUIRE_THAT (plan.ops[0].band.frequencyHz, WithinAbs (3500.0f, 1.0f));
}

TEST_CASE ("EQ chat adds air as a high shelf")
{
    const auto plan = parseEqChat ("add air");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops[0].band.shape == FilterShape::HighShelf);
    REQUIRE (plan.ops[0].band.gainDb > 0.0f);
}

TEST_CASE ("EQ chat stacks roll off and mud")
{
    const auto plan = parseEqChat ("roll off the low end and clean the mud");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 2);
    REQUIRE (plan.ops[0].band.shape == FilterShape::LowCut);
    REQUIRE (plan.ops[1].band.gainDb < 0.0f);
}

TEST_CASE ("EQ chat does not treat clear the mud as reset")
{
    const auto plan = parseEqChat ("clear the mud");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].kind == ChatOp::Kind::AddOrUpdate);
    REQUIRE (plan.ops[0].band.gainDb < 0.0f);
}

TEST_CASE ("EQ chat clears all bands")
{
    const auto plan = parseEqChat ("clear bands");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].kind == ChatOp::Kind::ClearAll);
}

TEST_CASE ("subtle presence does not add a sub band")
{
    const auto plan = parseEqChat ("subtle presence");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].label == "presence");
    REQUIRE (plan.ops[0].band.gainDb < 2.0f);
}

TEST_CASE ("EQ chat parses a high-pass frequency")
{
    const auto plan = parseEqChat ("high pass at 120 Hz");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops[0].band.shape == FilterShape::LowCut);
    REQUIRE_THAT (plan.ops[0].band.frequencyHz, WithinAbs (120.0f, 0.1f));
}

TEST_CASE ("EQ chat loads a vocal preset")
{
    const auto plan = parseEqChat ("load the vocal preset");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops[0].kind == ChatOp::Kind::ApplyPreset);
    REQUIRE (plan.ops[0].presetIndex == 1);
}

TEST_CASE ("EQ chat help")
{
    const auto plan = parseEqChat ("help");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops[0].kind == ChatOp::Kind::Help);
    REQUIRE_FALSE (eqChatHelpText().empty());
}

TEST_CASE ("unknown chat falls through")
{
    const auto plan = parseEqChat ("make it sound like a spaceship");
    REQUIRE_FALSE (plan.understood);
    REQUIRE (plan.ops.empty());
}

TEST_CASE ("EQ chat JSON add band")
{
    const auto plan = parseEqChatJson (
        R"({"say":"ok","actions":[{"op":"add","shape":"LowCut","freq":90,"gain":0,"q":0.7,"slope":18}]})");
    REQUIRE (plan.understood);
    REQUIRE (plan.reply == "ok");
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].band.shape == FilterShape::LowCut);
    REQUIRE_THAT (plan.ops[0].band.frequencyHz, WithinAbs (90.0f, 0.1f));
}

TEST_CASE ("EQ chat JSON clamps extreme values")
{
    const auto plan = parseEqChatJson (
        R"({"say":"ok","actions":[{"op":"add","shape":"Bell","freq":999999,"gain":80,"q":0.01,"slope":6}]})");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops[0].band.frequencyHz <= kMaxHz);
    REQUIRE (plan.ops[0].band.gainDb <= kMaxGainDb);
}

TEST_CASE ("generic cut at a frequency")
{
    const auto plan = parseEqChat ("cut 250 Hz");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops[0].band.shape == FilterShape::Bell);
    REQUIRE (plan.ops[0].band.gainDb < 0.0f);
    REQUIRE_THAT (plan.ops[0].band.frequencyHz, WithinAbs (250.0f, 0.1f));
}
