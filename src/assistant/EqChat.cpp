#include "assistant/EqChat.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <vector>

namespace puzzleq {
namespace {

std::string squash (const std::string& s)
{
    std::string compact;
    bool sp = false;
    for (unsigned char c : s)
    {
        if (c < 128 && std::isalpha (c))
        {
            compact.push_back (static_cast<char> (std::tolower (c)));
            sp = false;
        }
        else if ((c < 128 && std::isdigit (c)) || c >= 128)
        {
            compact.push_back (static_cast<char> (c));
            sp = false;
        }
        else if (! sp && ! compact.empty())
        {
            compact.push_back (' ');
            sp = true;
        }
    }
    if (! compact.empty() && compact.back() == ' ')
        compact.pop_back();
    return compact;
}

bool has (const std::string& hay, const char* needle)
{
    return hay.find (needle) != std::string::npos;
}

bool hasWord (const std::string& hay, const char* needle)
{
    const std::string n = needle;
    size_t p = 0;
    while ((p = hay.find (n, p)) != std::string::npos)
    {
        const bool left = (p == 0) || hay[p - 1] == ' ';
        const bool right = (p + n.size() == hay.size()) || hay[p + n.size()] == ' ';
        if (left && right)
            return true;
        ++p;
    }
    return false;
}

bool looksHebrew (const std::string& s)
{
    for (size_t i = 0; i + 1 < s.size(); ++i)
    {
        const auto a = static_cast<unsigned char> (s[i]);
        const auto b = static_cast<unsigned char> (s[i + 1]);
        if (a == 0xD7 && b >= 0x90 && b <= 0xAA)
            return true;
        if (a == 0xD6 && b >= 0x90)
            return true;
    }
    return false;
}

ChatOp addBand (FilterShape shape, float hz, float gain, float q, float slope, const char* label)
{
    ChatOp op;
    op.kind = ChatOp::Kind::AddOrUpdate;
    op.band.active = true;
    op.band.enabled = true;
    op.band.shape = shape;
    op.band.frequencyHz = std::clamp (hz, kMinHz, kMaxHz);
    op.band.gainDb = std::clamp (gain, kMinGainDb, kMaxGainDb);
    op.band.q = std::clamp (q, kMinQ, kMaxQ);
    op.band.slopeDbOct = std::clamp (slope, kMinSlope, kMaxSlope);
    op.label = label;
    return op;
}

float amount (const std::string& t, float mild, float normal, float strong)
{
    if (has (t, "subtle") || has (t, "gentle") || has (t, "a little") || has (t, "slight")
        || hasWord (t, "soft") || has (t, "\xd7\xa7\xd7\xa6\xd7\xaa")
        || has (t, "\xd7\x9e\xd7\xa2\xd7\x98")
        || has (t, "\xd7\x91\xd7\xa2\xd7\x93\xd7\x99\xd7\xa0"))
        return mild;
    if (has (t, "heavy") || has (t, "aggressive") || has (t, "lots") || hasWord (t, "more")
        || has (t, "strong") || has (t, "hard")
        || has (t, "\xd7\x94\xd7\xa8\xd7\x91\xd7\x94")
        || has (t, "\xd7\x97\xd7\x96\xd7\xa7")
        || has (t, "\xd7\x90\xd7\x92\xd7\xa8\xd7\xa1\xd7\x99\xd7\x91"))
        return strong;
    return normal;
}

bool isDbUnit (const std::string& u)
{
    return u == "db" || u == "dbfs" || u == "dB"
        || u == "\xd7\x93\xd7\x99\xd7\x91\xd7\x99"
        || u == "\xd7\x93\xd7\xa6\xd7\x99\xd7\x91\xd7\x9c"
        || u == "\xd7\x93\xd7\x91";
}

bool isHzUnit (const std::string& u)
{
    return u == "hz" || u == "\xd7\x94\xd7\xa8\xd7\xa5";
}

bool isKiloUnit (const std::string& u)
{
    return u == "k" || u == "khz"
        || u == "\xd7\x90\xd7\x9c\xd7\xa3"
        || u == "\xd7\x90\xd7\x9c\xd7\xa4\xd7\x99\xd7\x99\xd7\x9d";
}

struct Quantities
{
    float hz = 0.0f;
    float db = 0.0f;
    bool hasHz = false;
    bool hasDb = false;
};

bool parseLeadingNumber (const std::string& tok, float& value, std::string& rest)
{
    size_t i = 0;
    bool dot = false;
    while (i < tok.size()
           && (std::isdigit (static_cast<unsigned char> (tok[i]))
               || (tok[i] == '.' && ! dot)))
    {
        if (tok[i] == '.')
            dot = true;
        ++i;
    }
    if (i == 0)
        return false;
    try { value = std::stof (tok.substr (0, i)); }
    catch (...) { return false; }
    rest = tok.substr (i);
    return true;
}

Quantities parseQuantities (const std::string& t)
{
    std::istringstream iss (t);
    std::vector<std::string> tokens;
    for (std::string tok; iss >> tok; )
        tokens.push_back (tok);

    Quantities q;
    std::vector<float> loose;
    for (size_t n = 0; n < tokens.size(); ++n)
    {
        float value = 0.0f;
        std::string rest;
        if (! parseLeadingNumber (tokens[n], value, rest))
            continue;
        const std::string next = (n + 1 < tokens.size()) ? tokens[n + 1] : std::string();

        if (isHzUnit (rest) || isHzUnit (next))
        {
            q.hz = value;
            q.hasHz = true;
        }
        else if (isKiloUnit (rest) || isKiloUnit (next))
        {
            q.hz = value * 1000.0f;
            q.hasHz = true;
        }
        else if (isDbUnit (rest) || isDbUnit (next))
        {
            q.db = value;
            q.hasDb = true;
        }
        else if (rest.empty())
        {
            loose.push_back (value);
        }
    }

    if (! q.hasHz)
    {
        for (float v : loose)
        {
            if (v >= 20.0f && v <= 30000.0f)
            {
                q.hz = v;
                q.hasHz = true;
            }
        }
    }
    if (! q.hasDb)
    {
        for (float v : loose)
        {
            if (v > 0.2f && v <= 18.0f && (! q.hasHz || std::abs (v - q.hz) > 0.5f))
            {
                q.db = v;
                q.hasDb = true;
                break;
            }
        }
    }
    if (q.hasHz && q.hz >= kMinHz && q.hz <= kMaxHz)
        ;
    else if (q.hasHz)
        q.hasHz = false;
    return q;
}

float parseHzHint (const std::string& t, float fallback)
{
    const auto q = parseQuantities (t);
    return q.hasHz ? q.hz : fallback;
}

int lastActiveBand (const ChatContext& ctx)
{
    if (ctx.selectedBand >= 0 && ctx.selectedBand < kMaxBands
        && ctx.bands[static_cast<size_t> (ctx.selectedBand)].active)
        return ctx.selectedBand;
    for (int i = kMaxBands - 1; i >= 0; --i)
        if (ctx.bands[static_cast<size_t> (i)].active)
            return i;
    return -1;
}

std::string hzText (float hz)
{
    char buf[32];
    if (hz >= 1000.0f)
        std::snprintf (buf, sizeof (buf), "%.2f kHz", hz / 1000.0f);
    else
        std::snprintf (buf, sizeof (buf), "%.0f Hz", hz);
    return buf;
}

} // namespace

std::string eqChatHelpText()
{
    return "EN: roll off the low end, clean mud, boost presence, add air, de-ess, "
           "make the band narrower, add a narrow dynamic bell on the harshest vocal spot.\n"
           "HE: \xd7\xaa\xd7\x95\xd7\xa8\xd7\x99\xd7\x93 \xd7\x91 500 \xd7\x94\xd7\xa8\xd7\xa5 3 \xd7\x93\xd7\x99\xd7\x91\xd7\x99, "
           "\xd7\xaa\xd7\xa2\xd7\xa9\xd7\x94 \xd7\x90\xd7\xaa \xd7\x94\xd7\x91\xd7\x90\xd7\xa0\xd7\x93 \xd7\xa6\xd7\xa8 \xd7\x99\xd7\x95\xd7\xaa\xd7\xa8, "
           "\xd7\xaa\xd7\x95\xd7\xa1\xd7\x99\xd7\xa3 \xd7\xa7\xd7\xa6\xd7\xaa \xd7\x92\xd7\x91\xd7\x95\xd7\x94\xd7\x99\xd7\x9d \xd7\x9e\xd7\x90\xd7\x99\xd7\x96\xd7\x95\xd7\xa8 \xd7\x94 8 \xd7\x90\xd7\x9c\xd7\xa3.";
}

std::string eqChatSystemPrompt()
{
    return "You control PuzzlEQ, a 24-band parametric EQ. Hebrew and English. "
           "A live FFT analysis is provided. Use those frequencies when the user says "
           "the harshest / boxiest / muddiest area. Reply with JSON only, no markdown:\n"
           "{\"say\":\"short confirmation\",\"actions\":["
           "{\"op\":\"add\",\"shape\":\"Bell\",\"freq\":500,\"gain\":-3,\"q\":1.4,\"slope\":12,\"dyn\":0},"
           "{\"op\":\"tweakq\",\"qmul\":1.28}]}\n"
           "ops: add, clear, preset, tweakq. shapes: Bell, Notch, HighShelf, LowShelf, HighCut, LowCut, "
           "BandPass, TiltShelf, FlatTilt, AllPass. "
           "dyn is dynamic range in dB (negative = downward). "
           "Narrower band = raise Q gently (~1.25-1.4). "
           "preset names: Init, Vocal Presence, Mix Bus, Master Glue, De-Mud, Air, Telephone, "
           "Kick Punch, Snare Crack, Guitar Scoop. Prefer 1-4 bands.";
}

ChatPlan parseEqChat (const std::string& userText, const ChatContext& ctx)
{
    ChatPlan plan;
    if (ctx.spectrum.ok)
        plan.analysisNote = ctx.spectrum.summary;

    const std::string t = squash (userText);
    const bool he = looksHebrew (userText) || looksHebrew (t);
    if (t.empty())
    {
        plan.reply = he ? "\xd7\xaa\xd7\x9b\xd7\xaa\xd7\x95\xd7\x91 \xd7\x9e\xd7\x94 \xd7\x9c\xd7\xa2\xd7\xa9\xd7\x95\xd7\xaa \xd7\x9c\xd7\x90\xd7\x99\xd7\xa7\xd7\x99\xd7\x95."
                        : "Say what you want the EQ to do.";
        return plan;
    }

    auto finishNames = [&] ()
    {
        std::string names;
        for (size_t i = 0; i < plan.ops.size(); ++i)
        {
            if (i)
                names += ", ";
            names += plan.ops[i].label;
        }
        if (plan.reply.empty())
            plan.reply = (he ? "\xd7\x91\xd7\x99\xd7\xa6\xd7\xa2\xd7\xaa\xd7\x99: " : "Applied: ") + names + ".";
        if (ctx.spectrum.ok)
            plan.reply += he ? " \xd7\xa0\xd7\x99\xd7\xaa\xd7\x97 \xd7\xa1\xd7\xa4\xd7\xa7\xd7\x98\xd7\xa8\xd7\x95\xd7\x9d \xd7\x9c\xd7\xa4\xd7\xa0\xd7\x99."
                             : " Analyzed the live spectrum first.";
        plan.understood = true;
    };

    if (hasWord (t, "help") || t == "hi" || t == "hello" || has (t, "what can you")
        || has (t, "\xd7\xa2\xd7\x96\xd7\xa8\xd7\x94") || has (t, "\xd7\x9e\xd7\x94 \xd7\x90\xd7\xaa\xd7\x94 \xd7\x99\xd7\x95\xd7\x93\xd7\xa2"))
    {
        ChatOp op;
        op.kind = ChatOp::Kind::Help;
        plan.ops.push_back (op);
        plan.reply = eqChatHelpText();
        plan.understood = true;
        return plan;
    }

    const bool wantsMud = hasWord (t, "mud") || has (t, "muddy") || has (t, "woofy")
                          || has (t, "\xd7\x91\xd7\x95\xd7\xa5") || has (t, "\xd7\x9e\xd7\x90\xd7\x93\xd7\x99");
    if (has (t, "clear all") || has (t, "remove all") || has (t, "clear bands")
        || has (t, "clear everything") || has (t, "init eq") || has (t, "bypass eq")
        || t == "clear" || t == "reset" || t == "flat"
        || has (t, "\xd7\xaa\xd7\x90\xd7\xa4\xd7\xa1 \xd7\x94\xd7\x9b\xd7\x9c")
        || has (t, "\xd7\xa0\xd7\xa7\xd7\x94 \xd7\x90\xd7\xaa \xd7\x94\xd7\x9b\xd7\x9c")
        || ((hasWord (t, "reset") || hasWord (t, "flat")) && ! wantsMud))
    {
        ChatOp op;
        op.kind = ChatOp::Kind::ClearAll;
        plan.ops.push_back (op);
        plan.reply = he ? "\xd7\xa0\xd7\x99\xd7\xa7\xd7\x99\xd7\xaa\xd7\x99 \xd7\x90\xd7\xaa \xd7\x9b\xd7\x9c \xd7\x94\xd7\x91\xd7\x90\xd7\xa0\xd7\x93\xd7\x99\xd7\x9d."
                        : "Cleared every band.";
        plan.understood = true;
        return plan;
    }

    struct PresetHit { const char* key; int index; const char* name; };
    const PresetHit presets[] = {
        { "vocal", 1, "Vocal Presence" },
        { "mix bus", 2, "Mix Bus" },
        { "master", 3, "Master Glue" },
        { "de mud", 4, "De-Mud" },
        { "demud", 4, "De-Mud" },
        { "telephone", 6, "Telephone" },
        { "kick", 7, "Kick Punch" },
        { "snare", 8, "Snare Crack" },
        { "guitar", 9, "Guitar Scoop" },
        { "air preset", 5, "Air" },
    };
    if (hasWord (t, "preset") || has (t, "load ") || has (t, "use the ")
        || has (t, "\xd7\xa4\xd7\xa8\xd7\xa1\xd7\x98"))
    {
        for (const auto& p : presets)
            if (has (t, p.key))
            {
                ChatOp op;
                op.kind = ChatOp::Kind::ApplyPreset;
                op.presetIndex = p.index;
                op.label = p.name;
                plan.ops.push_back (op);
                plan.reply = std::string (he ? "\xd7\x98\xd7\xa2\xd7\xa0\xd7\xaa\xd7\x99 " : "Loaded ") + p.name + ".";
                plan.understood = true;
                return plan;
            }
    }

    const auto qty = parseQuantities (t);
    const bool wantsCut = hasWord (t, "cut") || hasWord (t, "dip") || hasWord (t, "reduce")
                          || hasWord (t, "notch") || has (t, "roll off")
                          || has (t, "\xd7\xaa\xd7\x95\xd7\xa8\xd7\x99\xd7\x93")
                          || has (t, "\xd7\x94\xd7\x95\xd7\xa8\xd7\x93")
                          || has (t, "\xd7\xaa\xd7\xa7\xd7\x98\xd7\x99\xd7\x9f")
                          || has (t, "\xd7\x9c\xd7\x94\xd7\x95\xd7\xa8\xd7\x99\xd7\x93");
    const bool wantsBoost = hasWord (t, "boost") || hasWord (t, "lift") || hasWord (t, "add")
                            || has (t, "\xd7\xaa\xd7\x95\xd7\xa1\xd7\x99\xd7\xa3")
                            || has (t, "\xd7\xaa\xd7\xa8\xd7\x99\xd7\x9d")
                            || has (t, "\xd7\xaa\xd7\xa2\xd7\x9c\xd7\x94")
                            || has (t, "\xd7\x9c\xd7\x94\xd7\x95\xd7\xa1\xd7\x99\xd7\xa3");
    const bool wantsBell = hasWord (t, "bell") || hasWord (t, "\xd7\x91\xd7\x9c")
                           || has (t, "\xd7\xa4\xd7\xa2\xd7\x9e\xd7\x95\xd7\x9f");
    const bool wantsDynamic = has (t, "dynamic") || has (t, "\xd7\x93\xd7\x99\xd7\xa0\xd7\x90\xd7\x9e")
                              || has (t, "\xd7\x93\xd7\x99\xd7\xa0\xd7\x9e");
    const bool wantsNarrow = hasWord (t, "narrow") || hasWord (t, "narrower") || hasWord (t, "tighter")
                             || hasWord (t, "\xd7\xa6\xd7\xa8")
                             || has (t, "\xd7\xa6\xd7\xa8\xd7\x94");
    const bool wantsWide = hasWord (t, "wider") || hasWord (t, "widen") || hasWord (t, "broader")
                           || hasWord (t, "\xd7\xa8\xd7\x97\xd7\x91");
    const bool wantsHighs = hasWord (t, "highs") || hasWord (t, "treble") || has (t, "top end")
                            || has (t, "\xd7\x92\xd7\x91\xd7\x95\xd7\x94\xd7\x99\xd7\x9d")
                            || has (t, "\xd7\x94\xd7\x99\xd7\x99\xd7\x96");
    const bool wantsHarsh = has (t, "harsh") || has (t, "harshness") || has (t, "icepick")
                            || has (t, "\xd7\xa6\xd7\x95\xd7\xa8\xd7\x9d")
                            || has (t, "\xd7\xa6\xd7\xa8\xd7\x99\xd7\x9e")
                            || has (t, "\xd7\x94\xd7\x90\xd7\xa8\xd7\xa9");
    const bool mentionsBand = has (t, "band") || has (t, "\xd7\x91\xd7\x90\xd7\xa0\xd7\x93")
                              || has (t, "\xd7\x91\xd7\xa8\xd7\xa0\xd7\x93")
                              || has (t, "\xd7\x94\xd7\x91\xd7\xa8\xd7\xa0\xd7\x93")
                              || has (t, "\xd7\x94\xd7\x91\xd7\x90\xd7\xa0\xd7\x93");
    const bool wantsQTweak = (wantsNarrow || wantsWide)
                             && ! wantsDynamic && ! wantsBell && ! wantsHighs && ! wantsHarsh
                             && ! qty.hasHz
                             && (mentionsBand || has (t, "q ") || hasWord (t, "q")
                                 || has (t, "\xd7\xaa\xd7\xa2\xd7\xa9\xd7\x94")
                                 || has (t, "make the") || has (t, "make it"));

    if (wantsQTweak)
    {
        const int slot = lastActiveBand (ctx);
        if (slot < 0)
        {
            plan.reply = he ? "\xd7\x90\xd7\x99\xd7\x9f \xd7\x91\xd7\x90\xd7\xa0\xd7\x93 \xd7\x9c\xd7\xa9\xd7\xa0\xd7\x95\xd7\xaa. \xd7\xaa\xd7\x95\xd7\xa1\xd7\x99\xd7\xa3 \xd7\x90\xd7\x95 \xd7\xaa\xd7\x91\xd7\x97\xd7\xa8 \xd7\x91\xd7\x90\xd7\xa0\xd7\x93 \xd7\xa7\xd7\x95\xd7\x93\xd7\x9d."
                            : "No band to change. Add or select a band first.";
            plan.understood = true;
            return plan;
        }
        ChatOp op;
        op.kind = ChatOp::Kind::TweakQ;
        op.targetBand = slot;
        const float mul = amount (t, 1.18f, 1.28f, 1.65f);
        op.qMul = wantsWide ? (1.0f / mul) : mul;
        op.label = wantsWide ? "wider" : "narrower";
        plan.ops.push_back (op);
        const float now = ctx.bands[static_cast<size_t> (slot)].q;
        const float next = std::clamp (now * op.qMul, kMinQ, kMaxQ);
        char buf[160];
        if (he)
            std::snprintf (buf, sizeof (buf),
                           "\xd7\xa6\xd7\xa8\xd7\xaa\xd7\x99 \xd7\x90\xd7\xaa \xd7\x94\xd7\x91\xd7\x90\xd7\xa0\xd7\x93 \xd7\x91\xd7\xa2\xd7\x93\xd7\x99\xd7\xa0\xd7\x95\xd7\xaa (Q %.2f \xe2\x86\x92 %.2f).",
                           now, next);
        else
            std::snprintf (buf, sizeof (buf),
                           "Nudged the band %s (Q %.2f to %.2f).",
                           wantsWide ? "wider" : "narrower", now, next);
        plan.reply = buf;
        plan.understood = true;
        return plan;
    }

    if (has (t, "roll off") || has (t, "high pass") || has (t, "highpass") || hasWord (t, "hpf")
        || has (t, "cut the rumble") || has (t, "cut rumble") || has (t, "low end out")
        || has (t, "take out the lows") || has (t, "remove the low") || has (t, "filter the low")
        || has (t, "cut the low") || has (t, "cut lows") || has (t, "remove lows")
        || hasWord (t, "lowcut") || (has (t, "low cut") && ! has (t, "low cut the top"))
        || has (t, "\xd7\x94\xd7\x99\xd7\x99 \xd7\xa4\xd7\x90\xd7\xa1")
        || has (t, "\xd7\xaa\xd7\x97\xd7\xaa\xd7\x95\xd7\x9a \xd7\xa0\xd7\x9e\xd7\x95\xd7\x9b\xd7\x99\xd7\x9d"))
    {
        float hz = 80.0f;
        if (ctx.spectrum.ok)
            hz = std::clamp (ctx.spectrum.rumbleHz * 1.6f, 30.0f, 140.0f);
        if (hasWord (t, "kick") || hasWord (t, "bass")) hz = 40.0f;
        if (hasWord (t, "vocal") || hasWord (t, "voice") || hasWord (t, "speech")
            || has (t, "\xd7\x95\xd7\x95\xd7\xa7\xd7\x90\xd7\x9c"))
            hz = 100.0f;
        hz = parseHzHint (t, hz);
        const float sl = amount (t, 12.0f, 18.0f, 24.0f);
        plan.ops.push_back (addBand (FilterShape::LowCut, hz, 0.0f, 0.7f, sl, "low-cut"));
    }

    if (wantsMud || has (t, "boxy low mid"))
    {
        const float g = qty.hasDb ? -std::abs (qty.db) : -amount (t, 1.5f, 3.0f, 4.5f);
        const float hz = qty.hasHz ? qty.hz : (ctx.spectrum.ok ? ctx.spectrum.mudHz : 280.0f);
        plan.ops.push_back (addBand (FilterShape::Bell, hz, g, 1.3f, 12.0f, "mud cut"));
    }

    if (has (t, "boxy") || has (t, "boxiness") || has (t, "honk") || has (t, "cardboard"))
        plan.ops.push_back (addBand (FilterShape::Bell, parseHzHint (t, 450.0f),
                                    qty.hasDb ? -std::abs (qty.db) : -amount (t, 1.5f, 3.0f, 4.0f),
                                    1.6f, 12.0f, "boxiness cut"));

    if (has (t, "presence") || has (t, "forward") || has (t, "in your face") || has (t, "definition")
        || has (t, "\xd7\xa0\xd7\x95\xd7\x9b\xd7\x97\xd7\x95\xd7\xaa"))
        plan.ops.push_back (addBand (FilterShape::Bell,
                                    qty.hasHz ? qty.hz : (ctx.spectrum.ok ? ctx.spectrum.presenceHz : 3500.0f),
                                    qty.hasDb ? std::abs (qty.db) : amount (t, 1.5f, 2.5f, 4.0f),
                                    1.2f, 12.0f, "presence"));

    if ((hasWord (t, "air") || hasWord (t, "sparkle") || has (t, "open the top") || has (t, "open up")
         || hasWord (t, "breathe") || hasWord (t, "shiny")
         || has (t, "\xd7\x90\xd7\x95\xd7\x95\xd7\x99\xd7\xa8"))
        && ! wantsHighs)
        plan.ops.push_back (addBand (FilterShape::HighShelf, parseHzHint (t, 12000.0f),
                                    qty.hasDb ? std::abs (qty.db) : amount (t, 1.5f, 2.5f, 4.0f),
                                    0.7f, 6.0f, "air"));

    if (has (t, "de ess") || has (t, "deess") || has (t, "sibil") || hasWord (t, "esses")
        || has (t, "harsh s"))
        plan.ops.push_back (addBand (FilterShape::Bell,
                                    qty.hasHz ? qty.hz : (ctx.spectrum.ok ? ctx.spectrum.sibilanceHz : 7500.0f),
                                    qty.hasDb ? -std::abs (qty.db) : -amount (t, 2.0f, 3.5f, 6.0f),
                                    2.2f, 12.0f, "de-ess"));

    if (wantsHarsh && ! wantsDynamic && ! wantsBell)
        plan.ops.push_back (addBand (FilterShape::Bell,
                                    qty.hasHz ? qty.hz : (ctx.spectrum.ok ? ctx.spectrum.harshHz : 3200.0f),
                                    qty.hasDb ? -std::abs (qty.db) : -amount (t, 1.5f, 2.5f, 4.0f),
                                    wantsNarrow ? 3.2f : 1.5f, 12.0f, "harshness cut"));

    if (hasWord (t, "warm") || has (t, "warmth") || hasWord (t, "body") || hasWord (t, "fatter"))
        plan.ops.push_back (addBand (FilterShape::LowShelf, parseHzHint (t, 140.0f),
                                    qty.hasDb ? std::abs (qty.db) : amount (t, 1.0f, 2.0f, 3.5f),
                                    0.7f, 12.0f, "warmth"));

    if (has (t, "brighten") || has (t, "brighter") || hasWord (t, "dull") || hasWord (t, "dark"))
        plan.ops.push_back (addBand (FilterShape::HighShelf, parseHzHint (t, 8000.0f),
                                    qty.hasDb ? std::abs (qty.db) : amount (t, 1.2f, 2.2f, 3.5f),
                                    0.7f, 12.0f, "brightness"));

    if (hasWord (t, "thin") || hasWord (t, "skinny") || has (t, "no low"))
        plan.ops.push_back (addBand (FilterShape::LowShelf, parseHzHint (t, 120.0f),
                                    amount (t, 1.5f, 2.5f, 4.0f), 0.7f, 12.0f, "low shelf"));

    if (has (t, "low cut the top") || has (t, "darken") || has (t, "too bright")
        || has (t, "tame the top") || has (t, "high cut") || has (t, "highcut")
        || has (t, "low pass") || has (t, "lowpass") || hasWord (t, "lpf"))
        plan.ops.push_back (addBand (FilterShape::HighCut, parseHzHint (t, 12000.0f), 0.0f, 0.7f,
                                    amount (t, 6.0f, 12.0f, 24.0f), "high-cut"));

    if (hasWord (t, "rumble"))
        plan.ops.push_back (addBand (FilterShape::LowCut,
                                    qty.hasHz ? qty.hz : (ctx.spectrum.ok ? ctx.spectrum.rumbleHz : 40.0f),
                                    0.0f, 0.7f, 24.0f, "rumble filter"));

    if (hasWord (t, "nasal") || hasWord (t, "nose"))
        plan.ops.push_back (addBand (FilterShape::Bell, parseHzHint (t, 1000.0f),
                                    -amount (t, 1.5f, 3.0f, 4.0f), 2.0f, 12.0f, "nasal cut"));

    if (hasWord (t, "click") && hasWord (t, "kick"))
        plan.ops.push_back (addBand (FilterShape::Bell, parseHzHint (t, 4000.0f),
                                    amount (t, 1.5f, 2.5f, 4.0f), 1.4f, 12.0f, "click"));

    if (hasWord (t, "sub") || hasWord (t, "808") || hasWord (t, "weight"))
        plan.ops.push_back (addBand (FilterShape::Bell, parseHzHint (t, 50.0f),
                                    amount (t, 1.0f, 2.0f, 3.5f), 1.1f, 12.0f, "sub"));

    if (hasWord (t, "scoop") || has (t, "hollow the mids") || has (t, "cut the mids"))
        plan.ops.push_back (addBand (FilterShape::Bell, parseHzHint (t, 400.0f),
                                    -amount (t, 1.5f, 3.0f, 4.5f), 0.9f, 12.0f, "mid scoop"));

    if (wantsHighs)
    {
        const float hz = qty.hasHz ? qty.hz : 8000.0f;
        const float g = qty.hasDb ? std::abs (qty.db) : amount (t, 2.4f, 2.6f, 4.0f);
        const auto shape = wantsBell ? FilterShape::Bell : FilterShape::HighShelf;
        plan.ops.push_back (addBand (shape, hz, wantsCut ? -g : g, wantsNarrow ? 2.8f : 0.75f,
                                    6.0f, "highs"));
        if (he)
        {
            char buf[160];
            std::snprintf (buf, sizeof (buf),
                           "\xd7\x94\xd7\x95\xd7\xa1\xd7\xa4\xd7\xaa\xd7\x99 \xd7\x92\xd7\x91\xd7\x95\xd7\x94\xd7\x99\xd7\x9d \xd7\xa1\xd7\x91\xd7\x99\xd7\x91 %s, +%.1f dB.",
                           hzText (hz).c_str(), g);
            plan.reply = buf;
        }
    }

    if (wantsDynamic && (wantsBell || wantsHarsh || has (t, "\xd7\x95\xd7\x95\xd7\xa7\xd7\x90\xd7\x9c")
                         || hasWord (t, "vocal")))
    {
        float hz = qty.hasHz ? qty.hz : (ctx.spectrum.ok ? ctx.spectrum.harshHz : 3200.0f);
        if (! qty.hasHz && wantsHarsh && ctx.spectrum.ok)
            hz = ctx.spectrum.harshHz;
        auto op = addBand (FilterShape::Bell, hz, 0.0f, wantsNarrow ? 5.4f : 2.4f, 12.0f, "dynamic bell");
        op.band.dynRangeDb = qty.hasDb ? -std::abs (qty.db) : -amount (t, 3.0f, 5.0f, 8.0f);
        op.band.thresholdDb = ctx.spectrum.ok
            ? std::clamp (ctx.spectrum.harshMagDb - 4.0f, -36.0f, -8.0f)
            : -18.0f;
        op.band.attackMs = 8.0f;
        op.band.releaseMs = 90.0f;
        plan.ops.push_back (op);
        if (he)
            plan.reply = std::string ("\xd7\x94\xd7\x95\xd7\xa1\xd7\xa4\xd7\xaa\xd7\x99 \xd7\x91\xd7\x9c \xd7\x93\xd7\x99\xd7\xa0\xd7\x9e\xd7\x99 \xd7\xa6\xd7\xa8 \xd7\x91-")
                         + hzText (op.band.frequencyHz)
                         + (ctx.spectrum.ok ? " (\xd7\x94\xd7\x90\xd7\x96\xd7\x95\xd7\xa8 \xd7\x94\xd7\x9b\xd7\x99 \xd7\xa6\xd7\x95\xd7\xa8\xd7\x9d \xd7\xa9\xd7\x9c \xd7\x94\xd7\x95\xd7\x95\xd7\xa7\xd7\x90\xd7\x9c)."
                                            : ".");
        else
            plan.reply = std::string ("Added a narrow dynamic bell at ")
                         + hzText (op.band.frequencyHz)
                         + (ctx.spectrum.ok ? " (harshest spot on the live vocal spectrum)." : ".");
    }

    if (plan.ops.empty() && (wantsCut || wantsBoost) && qty.hasHz)
    {
        const float g = qty.hasDb ? qty.db : amount (t, 1.5f, 3.0f, 5.0f);
        const float signedG = wantsCut ? -std::abs (g) : std::abs (g);
        const auto shape = has (t, "shelf")
            ? (qty.hz < 800.0f ? FilterShape::LowShelf : FilterShape::HighShelf)
            : (hasWord (t, "notch") ? FilterShape::Notch : FilterShape::Bell);
        auto op = addBand (shape, qty.hz, hasWord (t, "notch") ? 0.0f : signedG,
                           wantsNarrow ? 3.6f : 1.3f, 12.0f, wantsCut ? "cut" : "boost");
        if (wantsDynamic)
        {
            op.band.dynRangeDb = signedG;
            op.band.gainDb = 0.0f;
            op.label = "dynamic bell";
        }
        plan.ops.push_back (op);
        if (he)
        {
            char buf[192];
            std::snprintf (buf, sizeof (buf),
                           wantsCut ? "\xd7\x94\xd7\x95\xd7\xa8\xd7\x93\xd7\xaa\xd7\x99 %.1f dB \xd7\x91-%s."
                                    : "\xd7\x94\xd7\xa8\xd7\x9e\xd7\xaa\xd7\x99 %.1f dB \xd7\x91-%s.",
                           std::abs (signedG), hzText (qty.hz).c_str());
            plan.reply = buf;
        }
    }

    if (plan.ops.empty())
    {
        plan.reply = (he ? "\xd7\x9c\xd7\x90 \xd7\xaa\xd7\xa4\xd7\xa1\xd7\xaa\xd7\x99 \xd7\x90\xd7\xaa \xd7\x94\xd7\xa4\xd7\xa7\xd7\x95\xd7\x93\xd7\x94. " : "I did not catch an EQ move. ")
                     + eqChatHelpText();
        plan.understood = false;
        return plan;
    }

    finishNames();
    return plan;
}

namespace {

std::string jsonString (const std::string& src, const char* key)
{
    const std::string pat = std::string ("\"") + key + "\"";
    auto k = src.find (pat);
    if (k == std::string::npos)
        return {};
    k = src.find (':', k);
    if (k == std::string::npos)
        return {};
    k = src.find_first_not_of (" \t\n\r", k + 1);
    if (k == std::string::npos)
        return {};
    if (src[k] == '"')
    {
        auto e = src.find ('"', k + 1);
        if (e == std::string::npos)
            return {};
        return src.substr (k + 1, e - k - 1);
    }
    auto e = src.find_first_of (",} \t\n", k);
    return src.substr (k, e == std::string::npos ? std::string::npos : e - k);
}

float jsonNumber (const std::string& src, const char* key, float fallback)
{
    const auto s = jsonString (src, key);
    if (s.empty())
        return fallback;
    try { return std::stof (s); }
    catch (...) { return fallback; }
}

FilterShape shapeFromName (std::string name)
{
    for (char& c : name)
        c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    if (name.find ("lowcut") != std::string::npos || name.find ("highpass") != std::string::npos
        || name == "hpf" || name.find ("low cut") != std::string::npos)
        return FilterShape::LowCut;
    if (name.find ("highcut") != std::string::npos || name.find ("lowpass") != std::string::npos
        || name == "lpf" || name.find ("high cut") != std::string::npos)
        return FilterShape::HighCut;
    if (name.find ("highshelf") != std::string::npos || name.find ("high shelf") != std::string::npos)
        return FilterShape::HighShelf;
    if (name.find ("lowshelf") != std::string::npos || name.find ("low shelf") != std::string::npos)
        return FilterShape::LowShelf;
    if (name.find ("notch") != std::string::npos)
        return FilterShape::Notch;
    if (name.find ("bandpass") != std::string::npos)
        return FilterShape::BandPass;
    if (name.find ("tiltshelf") != std::string::npos || name.find ("tilt shelf") != std::string::npos)
        return FilterShape::TiltShelf;
    if (name.find ("flattilt") != std::string::npos)
        return FilterShape::FlatTilt;
    if (name.find ("allpass") != std::string::npos)
        return FilterShape::AllPass;
    return FilterShape::Bell;
}

} // namespace

ChatPlan parseEqChatJson (const std::string& jsonText)
{
    ChatPlan plan;
    std::string src = jsonText;
    const auto fence = src.find ('{');
    const auto last = src.rfind ('}');
    if (fence == std::string::npos || last == std::string::npos || last <= fence)
    {
        plan.reply = "The model did not return EQ JSON.";
        return plan;
    }
    src = src.substr (fence, last - fence + 1);
    plan.reply = jsonString (src, "say");
    if (plan.reply.empty())
        plan.reply = jsonString (src, "message");

    size_t pos = 0;
    while (true)
    {
        auto a = src.find ('{', pos);
        if (a == std::string::npos)
            break;
        auto b = src.find ('}', a);
        if (b == std::string::npos)
            break;
        const std::string obj = src.substr (a, b - a + 1);
        pos = b + 1;
        if (obj.find ("\"op\"") == std::string::npos && obj.find ("\"shape\"") == std::string::npos)
            continue;

        auto opName = jsonString (obj, "op");
        for (char& c : opName)
            c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
        if (opName == "clear")
        {
            ChatOp c;
            c.kind = ChatOp::Kind::ClearAll;
            plan.ops.push_back (c);
            plan.understood = true;
            continue;
        }
        if (opName == "tweakq")
        {
            ChatOp c;
            c.kind = ChatOp::Kind::TweakQ;
            c.qMul = jsonNumber (obj, "qmul", 1.28f);
            if (c.qMul < 0.2f || c.qMul > 8.0f)
                c.qMul = 1.28f;
            c.targetBand = static_cast<int> (jsonNumber (obj, "band", -1.0f));
            c.label = "narrower";
            plan.ops.push_back (c);
            plan.understood = true;
            continue;
        }
        if (opName == "preset")
        {
            auto name = jsonString (obj, "name");
            for (char& c : name)
                c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
            ChatOp c;
            c.kind = ChatOp::Kind::ApplyPreset;
            c.presetIndex = 1;
            if (name.find ("mix") != std::string::npos) c.presetIndex = 2;
            else if (name.find ("master") != std::string::npos) c.presetIndex = 3;
            else if (name.find ("mud") != std::string::npos) c.presetIndex = 4;
            else if (name.find ("air") != std::string::npos) c.presetIndex = 5;
            else if (name.find ("tele") != std::string::npos) c.presetIndex = 6;
            else if (name.find ("kick") != std::string::npos) c.presetIndex = 7;
            else if (name.find ("snare") != std::string::npos) c.presetIndex = 8;
            else if (name.find ("guitar") != std::string::npos) c.presetIndex = 9;
            else if (name.find ("init") != std::string::npos) c.presetIndex = 0;
            plan.ops.push_back (c);
            plan.understood = true;
            continue;
        }

        if (opName == "add" || obj.find ("\"shape\"") != std::string::npos)
        {
            const auto shape = shapeFromName (jsonString (obj, "shape"));
            const float freq = jsonNumber (obj, "freq", 1000.0f);
            const float gain = jsonNumber (obj, "gain", 0.0f);
            const float q = jsonNumber (obj, "q", 1.0f);
            const float slope = jsonNumber (obj, "slope", 12.0f);
            auto op = addBand (shape, freq, gain, q, slope, "model band");
            op.band.dynRangeDb = jsonNumber (obj, "dyn", jsonNumber (obj, "dynRange", 0.0f));
            if (op.band.dynRangeDb != 0.0f)
                op.band.thresholdDb = jsonNumber (obj, "thresh", -18.0f);
            plan.ops.push_back (op);
            plan.understood = true;
        }
    }

    if (plan.reply.empty() && plan.understood)
        plan.reply = "Applied the model's EQ moves.";
    if (! plan.understood)
        plan.reply = plan.reply.empty() ? "The model did not return usable EQ actions." : plan.reply;
    return plan;
}

} // namespace puzzleq
