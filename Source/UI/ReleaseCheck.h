#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <optional>

namespace pontedsp::gui {

inline std::optional<std::array<int, 3>> parseReleaseVersion(juce::String text)
{
    if (text.startsWithChar('v')) text = text.substring(1);
    const auto parts = juce::StringArray::fromTokens(text, ".", "");
    if (parts.size() != 3) return {};
    std::array<int, 3> result;
    for (int i = 0; i < 3; ++i)
    {
        if (parts[i].isEmpty() || parts[i].length() > 6 || !parts[i].containsOnly("0123456789")) return {};
        result[static_cast<std::size_t>(i)] = parts[i].getIntValue();
    }
    return result;
}

inline bool isNewerRelease(const juce::String& candidate, const juce::String& installed)
{
    const auto a = parseReleaseVersion(candidate), b = parseReleaseVersion(installed);
    return a && b && *a > *b;
}

inline juce::String versionFromReleaseJson(const juce::String& json)
{
    const auto object = juce::JSON::parse(json);
    if (!object.isObject() || !object["draft"].isBool() || !object["prerelease"].isBool()
        || static_cast<bool>(object["draft"]) || static_cast<bool>(object["prerelease"])
        || !object["tag_name"].isString()) return {};
    auto tag = object["tag_name"].toString();
    if (!parseReleaseVersion(tag)) return {};
    return tag.startsWithChar('v') ? tag.substring(1) : tag;
}

// One cancellable worker shared by open headers in this plugin module. No
// network calls from the audio thread, paint(), or timer. Cache survives editor
// reopenings; errors and rate limits never trigger an immediate retry loop.
class ReleaseCheck final : private juce::Thread
{
public:
    ReleaseCheck() : juce::Thread("Ponte release check")
    {
#if ! PONTE_DISABLE_NETWORK_CHECK
        startThread();
#endif
    }
    ~ReleaseCheck() override
    {
        signalThreadShouldExit(); notify();
        {
            const juce::ScopedLock lock(streamLock);
            if (stream) stream->cancel();
        }
        // cancel() unblocks the pending request before joining the worker.
        stopThread(5000);
    }
    juce::String latest() const
    {
        auto& state = cache();
        const juce::ScopedLock lock(state.lock);
        return state.latest;
    }

private:
    struct Cache { juce::CriticalSection lock; juce::String latest; double nextCheckMs {}; };
    static Cache& cache() { static Cache state; return state; }
    void run() override
    {
        while (!threadShouldExit())
        {
            auto& state = cache();
            bool due = false;
            {
                const juce::ScopedLock lock(state.lock);
                const auto now = juce::Time::getMillisecondCounterHiRes();
                due = now >= state.nextCheckMs;
                if (due) state.nextCheckMs = now + 3600000.0;
            }
            if (due) fetch();
            wait(60000);
        }
    }
    void fetch()
    {
        juce::WebInputStream* request;
        {
            const juce::ScopedLock lock(streamLock);
            if (threadShouldExit()) return;
            stream = std::make_unique<juce::WebInputStream>(juce::URL(
                "https://api.github.com/repos/Ponteh/ponte-mbc4/releases/latest"), false);
            stream->withConnectionTimeout(3000).withNumRedirectsToFollow(0)
                .withExtraHeaders("User-Agent: Ponte-MBC4-Update-Check\r\nAccept: application/vnd.github+json\r\n");
            request = stream.get();
        }
        juce::MemoryOutputStream body;
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 4000.0;
        bool complete = false;
        if (request->connect(nullptr) && request->getStatusCode() == 200)
        {
            std::array<char, 4096> buffer;
            while (!threadShouldExit() && juce::Time::getMillisecondCounterHiRes() < deadline
                   && body.getDataSize() < 65536)
            {
                const auto count = request->read(buffer.data(), static_cast<int>(buffer.size()));
                if (count <= 0) { complete = !request->isError(); break; }
                body.write(buffer.data(), static_cast<std::size_t>(count));
            }
        }
        if (complete && !threadShouldExit())
        {
            const auto version = versionFromReleaseJson(body.toUTF8());
            if (version.isNotEmpty())
            {
                auto& state = cache();
                const juce::ScopedLock lock(state.lock);
                state.latest = version;
            }
        }
        const juce::ScopedLock lock(streamLock);
        stream.reset();
    }
    juce::CriticalSection streamLock;
    std::unique_ptr<juce::WebInputStream> stream;
};
} // namespace pontedsp::gui
