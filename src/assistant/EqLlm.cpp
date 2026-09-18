#include "assistant/EqLlm.h"
#include "assistant/EqChat.h"
#include <juce_core/juce_core.h>
#include <cstdlib>

namespace puzzleq {
namespace {

juce::File settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("PuzzlEQ")
        .getChildFile ("assistant.json");
}

juce::String httpPost (const juce::String& url,
                       const juce::String& body,
                       const juce::String& extraHeaders,
                       int timeoutMs = 12000)
{
    juce::URL u (url);
    const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostBody)
                          .withExtraHeaders (extraHeaders)
                          .withConnectionTimeoutMs (timeoutMs)
                          .withHttpRequestCmd ("POST");
    auto stream = u.withPOSTData (body).createInputStream (opts);
    if (stream == nullptr)
        return {};
    return stream->readEntireStreamAsString();
}

juce::String httpGet (const juce::String& url, int timeoutMs = 2500)
{
    juce::URL u (url);
    const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                          .withConnectionTimeoutMs (timeoutMs);
    auto stream = u.createInputStream (opts);
    if (stream == nullptr)
        return {};
    return stream->readEntireStreamAsString();
}

juce::String extractOpenAiContent (const juce::String& json)
{
    auto v = juce::JSON::parse (json);
    if (auto* obj = v.getDynamicObject())
    {
        auto choices = obj->getProperty ("choices");
        if (auto* arr = choices.getArray(); arr != nullptr && ! arr->isEmpty())
        {
            if (auto* ch = arr->getUnchecked (0).getDynamicObject())
                if (auto* msg = ch->getProperty ("message").getDynamicObject())
                    return msg->getProperty ("content").toString();
        }
        if (auto* err = obj->getProperty ("error").getDynamicObject())
            return {};
        // Gemini
        auto cands = obj->getProperty ("candidates");
        if (auto* arr = cands.getArray(); arr != nullptr && ! arr->isEmpty())
        {
            if (auto* cand = arr->getUnchecked (0).getDynamicObject())
                if (auto* content = cand->getProperty ("content").getDynamicObject())
                    if (auto* parts = content->getProperty ("parts").getArray(); parts != nullptr && ! parts->isEmpty())
                        if (auto* p = parts->getUnchecked (0).getDynamicObject())
                            return p->getProperty ("text").toString();
        }
    }
    return {};
}

juce::var makeOpenAiBody (const juce::String& model, const juce::String& user)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("model", model);
    root->setProperty ("temperature", 0.2);
    juce::Array<juce::var> messages;
    auto* sys = new juce::DynamicObject();
    sys->setProperty ("role", "system");
    sys->setProperty ("content", juce::String (eqChatSystemPrompt()));
    auto* usr = new juce::DynamicObject();
    usr->setProperty ("role", "user");
    usr->setProperty ("content", user);
    messages.add (sys);
    messages.add (usr);
    root->setProperty ("messages", messages);
    return juce::var (root);
}

} // namespace

LlmSettings loadLlmSettings()
{
    LlmSettings s;
    auto f = settingsFile();
    if (! f.existsAsFile())
        return s;
    auto v = juce::JSON::parse (f.loadFileAsString());
    if (auto* o = v.getDynamicObject())
    {
        s.groqKey = o->getProperty ("groqKey").toString().toStdString();
        s.geminiKey = o->getProperty ("geminiKey").toString().toStdString();
        const auto url = o->getProperty ("ollamaUrl").toString();
        if (url.isNotEmpty())
            s.ollamaUrl = url.toStdString();
        s.ollamaModel = o->getProperty ("ollamaModel").toString().toStdString();
    }
    if (s.groqKey.empty())
        if (auto* e = std::getenv ("GROQ_API_KEY"))
            s.groqKey = e;
    if (s.geminiKey.empty())
        if (auto* e = std::getenv ("GEMINI_API_KEY"))
            s.geminiKey = e;
    return s;
}

void saveLlmSettings (const LlmSettings& s)
{
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    auto* o = new juce::DynamicObject();
    o->setProperty ("groqKey", juce::String (s.groqKey));
    o->setProperty ("geminiKey", juce::String (s.geminiKey));
    o->setProperty ("ollamaUrl", juce::String (s.ollamaUrl));
    o->setProperty ("ollamaModel", juce::String (s.ollamaModel));
    f.replaceWithText (juce::JSON::toString (juce::var (o), true));
}

static LlmResult tryOllama (const std::string& user, const LlmSettings& settings)
{
    LlmResult r;
    juce::String base = settings.ollamaUrl.empty() ? "http://127.0.0.1:11434"
                                                   : juce::String (settings.ollamaUrl);
    while (base.endsWithChar ('/'))
        base = base.dropLastCharacters (1);

    juce::String model = settings.ollamaModel;
    if (model.isEmpty())
    {
        const auto tags = httpGet (base + "/api/tags");
        auto v = juce::JSON::parse (tags);
        if (auto* o = v.getDynamicObject())
            if (auto* arr = o->getProperty ("models").getArray(); arr != nullptr && ! arr->isEmpty())
                if (auto* m = arr->getUnchecked (0).getDynamicObject())
                    model = m->getProperty ("name").toString();
    }
    if (model.isEmpty())
    {
        r.error = "No local Ollama model.";
        return r;
    }

    const auto body = juce::JSON::toString (makeOpenAiBody (model, user), false);
    const auto resp = httpPost (base + "/v1/chat/completions", body, "Content-Type: application/json\r\n", 15000);
    const auto content = extractOpenAiContent (resp);
    if (content.isEmpty())
    {
        r.error = "Ollama did not answer.";
        return r;
    }
    r.ok = true;
    r.source = "Ollama " + model.toStdString();
    r.text = content.toStdString();
    return r;
}

static LlmResult tryGroq (const std::string& user, const LlmSettings& settings)
{
    LlmResult r;
    if (settings.groqKey.empty())
    {
        r.error = "No Groq key.";
        return r;
    }
    const auto body = juce::JSON::toString (makeOpenAiBody ("llama-3.1-8b-instant", user), false);
    const juce::String headers = "Content-Type: application/json\r\nAuthorization: Bearer "
                               + juce::String (settings.groqKey) + "\r\n";
    const auto resp = httpPost ("https://api.groq.com/openai/v1/chat/completions", body, headers);
    const auto content = extractOpenAiContent (resp);
    if (content.isEmpty())
    {
        r.error = "Groq free model did not answer.";
        return r;
    }
    r.ok = true;
    r.source = "Groq Llama 3.1 8B";
    r.text = content.toStdString();
    return r;
}

static LlmResult tryGemini (const std::string& user, const LlmSettings& settings)
{
    LlmResult r;
    if (settings.geminiKey.empty())
    {
        r.error = "No Gemini key.";
        return r;
    }
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> contents;
    auto* c = new juce::DynamicObject();
    juce::Array<juce::var> parts;
    auto* p = new juce::DynamicObject();
    p->setProperty ("text", juce::String (eqChatSystemPrompt()) + "\nUser: " + juce::String (user));
    parts.add (p);
    c->setProperty ("parts", parts);
    contents.add (c);
    root->setProperty ("contents", contents);
    const auto body = juce::JSON::toString (juce::var (root), false);
    const auto url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key="
                   + juce::String (settings.geminiKey);
    const auto resp = httpPost (url, body, "Content-Type: application/json\r\n");
    const auto content = extractOpenAiContent (resp);
    if (content.isEmpty())
    {
        r.error = "Gemini free model did not answer.";
        return r;
    }
    r.ok = true;
    r.source = "Gemini 2.0 Flash";
    r.text = content.toStdString();
    return r;
}

LlmResult requestEqLlm (const std::string& userText, const LlmSettings& settings)
{
    auto ollama = tryOllama (userText, settings);
    if (ollama.ok)
        return ollama;
    auto groq = tryGroq (userText, settings);
    if (groq.ok)
        return groq;
    auto gemini = tryGemini (userText, settings);
    if (gemini.ok)
        return gemini;

    LlmResult r;
    r.error = "No free model answered. Local phrases still work. Optional: run Ollama, or paste a free Groq/Gemini key in Chat settings.";
    return r;
}

} // namespace puzzleq
