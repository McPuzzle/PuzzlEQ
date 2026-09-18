#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "assistant/EqChat.h"
#include "assistant/EqAnalyze.h"
#include "dsp/SpectrumAnalyzer.h"

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

TEST_CASE ("Hebrew cut 3 dB at 500 Hz")
{
    const auto plan = parseEqChat ("\xd7\xaa\xd7\x95\xd7\xa8\xd7\x99\xd7\x93 \xd7\x91 500 \xd7\x94\xd7\xa8\xd7\xa5 3 \xd7\x93\xd7\x99\xd7\x91\xd7\x99");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].kind == ChatOp::Kind::AddOrUpdate);
    REQUIRE (plan.ops[0].band.shape == FilterShape::Bell);
    REQUIRE_THAT (plan.ops[0].band.frequencyHz, WithinAbs (500.0f, 0.1f));
    REQUIRE_THAT (plan.ops[0].band.gainDb, WithinAbs (-3.0f, 0.15f));
}

TEST_CASE ("Hebrew make the band narrower")
{
    ChatContext ctx;
    ctx.selectedBand = 2;
    ctx.bands[2].active = true;
    ctx.bands[2].q = 1.20f;
    ctx.bands[2].frequencyHz = 1000.0f;
    const auto plan = parseEqChat (
        "\xd7\xaa\xd7\xa2\xd7\xa9\xd7\x94 \xd7\x90\xd7\xaa \xd7\x94\xd7\x91\xd7\xa8\xd7\xa0\xd7\x93 \xd7\xa6\xd7\xa8 \xd7\x99\xd7\x95\xd7\xaa\xd7\xa8",
        ctx);
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].kind == ChatOp::Kind::TweakQ);
    REQUIRE (plan.ops[0].targetBand == 2);
    REQUIRE (plan.ops[0].qMul > 1.12f);
    REQUIRE (plan.ops[0].qMul < 1.45f);
}

TEST_CASE ("English make the band narrower")
{
    ChatContext ctx;
    ctx.selectedBand = 0;
    ctx.bands[0].active = true;
    ctx.bands[0].q = 1.0f;
    const auto plan = parseEqChat ("make the band narrower", ctx);
    REQUIRE (plan.understood);
    REQUIRE (plan.ops[0].kind == ChatOp::Kind::TweakQ);
    REQUIRE (plan.ops[0].qMul > 1.1f);
}

TEST_CASE ("Hebrew add a bit of highs around 8 kHz")
{
    const auto plan = parseEqChat (
        "\xd7\xaa\xd7\x95\xd7\xa1\xd7\x99\xd7\xa3 \xd7\xa7\xd7\xa6\xd7\xaa \xd7\x92\xd7\x91\xd7\x95\xd7\x94\xd7\x99\xd7\x9d \xd7\x9e\xd7\x90\xd7\x99\xd7\x96\xd7\x95\xd7\xa8 \xd7\x94 8 \xd7\x90\xd7\x9c\xd7\xa3");
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].band.shape == FilterShape::HighShelf);
    REQUIRE_THAT (plan.ops[0].band.frequencyHz, WithinAbs (8000.0f, 1.0f));
    REQUIRE (plan.ops[0].band.gainDb >= 2.0f);
    REQUIRE (plan.ops[0].band.gainDb <= 3.2f);
}

TEST_CASE ("Hebrew narrow dynamic bell at analyzed harsh peak")
{
    ChatContext ctx;
    ctx.spectrum.ok = true;
    ctx.spectrum.harshHz = 4120.0f;
    ctx.spectrum.harshMagDb = -9.0f;
    ctx.spectrum.summary = "Harsh vocal pocket: 4.12 kHz";
    const auto plan = parseEqChat (
        "\xd7\xaa\xd7\x95\xd7\xa1\xd7\x99\xd7\xa3 \xd7\x91\xd7\x9c \xd7\x93\xd7\x99\xd7\xa0\xd7\x90\xd7\x9e\xd7\x99 \xd7\xa6\xd7\xa8 \xd7\x91\xd7\x90\xd7\x99\xd7\x96\xd7\x95\xd7\xa8 \xd7\x94\xd7\x9b\xd7\x99 Harsh \xd7\x94\xd7\x95\xd7\x95\xd7\xa7\xd7\x90\xd7\x9c \xd7\x94\xd7\x96\xd7\x94",
        ctx);
    REQUIRE (plan.understood);
    REQUIRE (plan.ops.size() == 1);
    REQUIRE (plan.ops[0].band.shape == FilterShape::Bell);
    REQUIRE_THAT (plan.ops[0].band.frequencyHz, WithinAbs (4120.0f, 1.0f));
    REQUIRE (plan.ops[0].band.q >= 4.0f);
    REQUIRE (plan.ops[0].band.dynRangeDb < -1.0f);
}

TEST_CASE ("spectrum analyze finds a planted harsh peak")
{
    const int fft = 4096;
    const float sr = 48000.0f;
    std::vector<float> mag (static_cast<size_t> (fft / 2 + 1), -36.0f);
    const int bin = SpectrumAnalyzer::hzToBin (3500.0f, fft, sr);
    mag[static_cast<size_t> (bin - 1)] = -18.0f;
    mag[static_cast<size_t> (bin)] = -8.0f;
    mag[static_cast<size_t> (bin + 1)] = -19.0f;
    const auto report = analyzeSpectrum (mag, mag, sr, fft);
    REQUIRE (report.ok);
    REQUIRE (report.harshHz > 3000.0f);
    REQUIRE (report.harshHz < 4200.0f);
    REQUIRE (report.harshProminenceDb > 2.0f);
    REQUIRE_FALSE (report.summary.empty());
}
