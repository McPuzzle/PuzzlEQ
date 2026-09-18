#include "assistant/EqChat.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <vector>

namespace puzzleq {
namespace {

std::string lower (std::string s)
{
    for (char& c : s)
        c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    return s;
}

std::string squash (std::string s)
{
    s = lower (std::move (s));
    std::string compact;
    bool sp = false;
    for (char c : s)
    {
        if (std::isalnum (static_cast<unsigned char> (c)))
        {
            compact.push_back (c);
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
        || hasWord (t, "soft"))
        return mild;
    if (has (t, "heavy") || has (t, "aggressive") || has (t, "lots") || hasWord (t, "more")
        || has (t, "strong") || has (t, "hard"))
        return strong;
    return normal;
}

float parseHzHint (const std::string& t, float fallback)
{
    std::istringstream iss (t);
    std::vector<std::string> tokens;
    for (std::string tok; iss >> tok; )
        tokens.push_back (tok);

    float found = 0.0f;
    auto consider = [&] (float hz)
    {
        if (hz >= kMinHz && hz <= kMaxHz)
            found = hz;
    };

    for (size_t n = 0; n < tokens.size(); ++n)
    {
        const auto& tok = tokens[n];
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
            continue;

        float value = 0.0f;
        try { value = std::stof (tok.substr (0, i)); }
        catch (...) { continue; }
        const std::string rest = tok.substr (i);
        const std::string next = (n + 1 < tokens.size()) ? tokens[n + 1] : std::string();

        if (rest == "hz" || next == "hz")
            consider (value);
        else if (rest == "k" || rest == "khz" || next == "k" || next == "khz")
            consider (value * 1000.0f);
        else if (next == "db" || next == "dboct" || next == "oct")
            continue;
        else if (rest.empty() && value >= 20.0f && value <= 30000.0f)
            consider (value);
    }
    return found > 0.0f ? found : fallback;
}

} // namespace

std::string eqChatHelpText()
{
    return "Try: roll off the low end, clean mud, boost presence, add air, de-ess, "
           "cut boxiness, tame harshness, warm it up, vocal preset, clear bands.";
}

std::string eqChatSystemPrompt()
{
    return "You control PuzzlEQ, a 24-band parametric EQ. Reply with JSON only, no markdown:\n"
           "{\"say\":\"short confirmation\",\"actions\":[{\"op\":\"add\",\"shape\":\"LowCut\","
           "\"freq\":80,\"gain\":0,\"q\":0.7,\"slope\":18}]}\n"
           "ops: add, clear, preset. shapes: Bell, Notch, HighShelf, LowShelf, HighCut, LowCut, "
           "BandPass, TiltShelf, FlatTilt, AllPass. preset names: Init, Vocal Presence, Mix Bus, "
           "Master Glue, De-Mud, Air, Telephone, Kick Punch, Snare Crack, Guitar Scoop. "
           "Prefer 1-4 bands. Do not invent shapes.";
}

ChatPlan parseEqChat (const std::string& userText)
{
    ChatPlan plan;
    const std::string t = squash (userText);
    if (t.empty())
    {
        plan.reply = "Say what you want the EQ to do.";
        return plan;
    }

    if (hasWord (t, "help") || t == "hi" || t == "hello" || has (t, "what can you"))
    {
        ChatOp op;
        op.kind = ChatOp::Kind::Help;
        plan.ops.push_back (op);
        plan.reply = eqChatHelpText();
        plan.understood = true;
        return plan;
    }

    const bool wantsMud = hasWord (t, "mud") || has (t, "muddy") || has (t, "woofy");
    if (has (t, "clear all") || has (t, "remove all") || has (t, "clear bands")
        || has (t, "clear everything") || has (t, "init eq") || has (t, "bypass eq")
        || t == "clear" || t == "reset" || t == "flat"
        || ((hasWord (t, "reset") || hasWord (t, "flat")) && ! wantsMud))
    {
        ChatOp op;
        op.kind = ChatOp::Kind::ClearAll;
        plan.ops.push_back (op);
        plan.reply = "Cleared every band.";
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
    if (hasWord (t, "preset") || has (t, "load ") || has (t, "use the "))
    {
        for (const auto& p : presets)
            if (has (t, p.key))
            {
                ChatOp op;
                op.kind = ChatOp::Kind::ApplyPreset;
                op.presetIndex = p.index;
                op.label = p.name;
                plan.ops.push_back (op);
                plan.reply = std::string ("Loaded ") + p.name + ".";
                plan.understood = true;
                return plan;
            }
    }

    if (has (t, "roll off") || has (t, "high pass") || has (t, "highpass") || hasWord (t, "hpf")
        || has (t, "cut the rumble") || has (t, "cut rumble") || has (t, "low end out")
        || has (t, "take out the lows") || has (t, "remove the low") || has (t, "filter the low")
        || has (t, "cut the low") || has (t, "cut lows") || has (t, "remove lows")
        || hasWord (t, "lowcut") || (has (t, "low cut") && ! has (t, "low cut the top")))
    {
        const float hz = parseHzHint (t, (hasWord (t, "kick") || hasWord (t, "bass")) ? 40.0f
                                                     : (hasWord (t, "vocal") || hasWord (t, "voice")
                                                        || hasWord (t, "speech") ? 100.0f : 80.0f));
        const float sl = amount (t, 12.0f, 18.0f, 24.0f);
        plan.ops.push_back (addBand (FilterShape::LowCut, hz, 0.0f, 0.7f, sl, "low-cut"));
    }

    if (wantsMud || has (t, "boxy low mid"))
    {
        const float g = -amount (t, 1.5f, 3.0f, 4.5f);
        const float hz = parseHzHint (t, 280.0f);
        plan.ops.push_back (addBand (FilterShape::Bell, hz, g, 1.3f, 12.0f, "mud cut"));
    }

    if (has (t, "boxy") || has (t, "boxiness") || has (t, "honk") || has (t, "cardboard"))
        plan.ops.push_back (addBand (FilterShape::Bell, parseHzHint (t, 450.0f),
                                    -amount (t, 1.5f, 3.0f, 4.0f), 1.6f, 12.0f, "boxiness cut"));

    if (has (t, "presence") || has (t, "forward") || has (t, "in your face") || has (t, "definition"))
        plan.ops.push_back (addBand (FilterShape::Bell, parseHzHint (t, 3500.0f),
                                    amount (t, 1.5f, 2.5f, 4.0f), 1.2f, 12.0f, "presence"));

    if (hasWord (t, "air") || hasWord (t, "sparkle") || has (t, "open the top") || has (t, "open up")
        || hasWord (t, "breathe") || hasWord (t, "shiny"))
        plan.ops.push_back (addBand (FilterShape::HighShelf, parseHzHint (t, 12000.0f),
                                    amount (t, 1.5f, 2.5f, 4.0f), 0.7f, 6.0f, "air"));

    if (has (t, "de ess") || has (t, "deess") || has (t, "sibil") || hasWord (t, "esses")
        || has (t, "harsh s"))
        plan.ops.push_back (addBand (FilterShape::Bell, parseHzHint (t, 7500.0f),
                                    -amount (t, 2.0f, 3.5f, 6.0f), 2.2f, 12.0f, "de-ess"));

    if (has (t, "harsh") || has (t, "harshness") || has (t, "icepick") || has (t, "grit in the top"))
        plan.ops.push_back (addBand (FilterShape::Bell, parseHzHint (t, 3200.0f),
                                    -amount (t, 1.5f, 2.5f, 4.0f), 1.5f, 12.0f, "harshness cut"));

    if (hasWord (t, "warm") || has (t, "warmth") || hasWord (t, "body") || hasWord (t, "fatter"))
        plan.ops.push_back (addBand (FilterShape::LowShelf, parseHzHint (t, 140.0f),
                                    amount (t, 1.0f, 2.0f, 3.5f), 0.7f, 12.0f, "warmth"));

    if (has (t, "brighten") || has (t, "brighter") || hasWord (t, "dull") || hasWord (t, "dark"))
        plan.ops.push_back (addBand (FilterShape::HighShelf, parseHzHint (t, 8000.0f),
                                    amount (t, 1.2f, 2.2f, 3.5f), 0.7f, 12.0f, "brightness"));

    if (hasWord (t, "thin") || hasWord (t, "skinny") || has (t, "no low"))
        plan.ops.push_back (addBand (FilterShape::LowShelf, parseHzHint (t, 120.0f),
                                    amount (t, 1.5f, 2.5f, 4.0f), 0.7f, 12.0f, "low shelf"));

    if (has (t, "low cut the top") || has (t, "darken") || has (t, "too bright")
        || has (t, "tame the top") || has (t, "high cut") || has (t, "highcut")
        || has (t, "low pass") || has (t, "lowpass") || hasWord (t, "lpf"))
        plan.ops.push_back (addBand (FilterShape::HighCut, parseHzHint (t, 12000.0f), 0.0f, 0.7f,
                                    amount (t, 6.0f, 12.0f, 24.0f), "high-cut"));

    if (hasWord (t, "rumble"))
        plan.ops.push_back (addBand (FilterShape::LowCut, parseHzHint (t, 40.0f), 0.0f, 0.7f, 24.0f,
                                    "rumble filter"));

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

    if (plan.ops.empty())
    {
        const bool cut = hasWord (t, "cut") || hasWord (t, "dip") || hasWord (t, "reduce")
                         || hasWord (t, "notch");
        const bool boost = hasWord (t, "boost") || hasWord (t, "lift") || hasWord (t, "add");
        const float hz = parseHzHint (t, 0.0f);
        if (hz >= kMinHz && (cut || boost))
        {
            const float g = cut ? -amount (t, 1.5f, 3.0f, 5.0f) : amount (t, 1.5f, 2.5f, 4.0f);
            const auto shape = has (t, "shelf")
                ? (hz < 800.0f ? FilterShape::LowShelf : FilterShape::HighShelf)
                : (hasWord (t, "notch") ? FilterShape::Notch : FilterShape::Bell);
            plan.ops.push_back (addBand (shape, hz, hasWord (t, "notch") ? 0.0f : g, 1.2f, 12.0f,
                                         cut ? "cut" : "boost"));
        }
    }

    if (plan.ops.empty())
    {
        plan.reply = "I did not catch an EQ move. " + eqChatHelpText();
        plan.understood = false;
        return plan;
    }

    std::string names;
    for (size_t i = 0; i < plan.ops.size(); ++i)
    {
        if (i)
            names += ", ";
        names += plan.ops[i].label;
    }
    plan.reply = "Applied: " + names + ".";
    plan.understood = true;
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
    name = lower (name);
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

        const auto op = lower (jsonString (obj, "op"));
        if (op == "clear")
        {
            ChatOp c;
            c.kind = ChatOp::Kind::ClearAll;
            plan.ops.push_back (c);
            plan.understood = true;
            continue;
        }
        if (op == "preset")
        {
            const auto name = lower (jsonString (obj, "name"));
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

        if (op == "add" || obj.find ("\"shape\"") != std::string::npos)
        {
            const auto shape = shapeFromName (jsonString (obj, "shape"));
            const float freq = jsonNumber (obj, "freq", 1000.0f);
            const float gain = jsonNumber (obj, "gain", 0.0f);
            const float q = jsonNumber (obj, "q", 1.0f);
            const float slope = jsonNumber (obj, "slope", 12.0f);
            plan.ops.push_back (addBand (shape, freq, gain, q, slope, "model band"));
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
