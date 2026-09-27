// SPDX-License-Identifier: GPL-2.0-or-later
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

#define LOG_INFO(...) do {} while(false)
#define SETTINGSMANAGER_UPDATE_REASON_SETTINGS_UPDATED 0
enum { NET_PACKET_ID_REQUEST_EFFECT_LIST = 0, NET_PACKET_ID_START_EFFECT = 20,
       NET_PACKET_ID_STOP_EFFECT = 21 };
static std::atomic<unsigned> checks{0};
#define CHECK(value) do { ++checks; if(!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " #value "\n"; std::_Exit(2); } } while(false)
static unsigned language_calls = 0;

class OpenRGBEffectTab : public QObject
{
public:
    std::vector<std::string> operations;
    nlohmann::json profile = {{"name", "initial"}};
    bool running = false;
    unsigned saves = 0;
    void OnGui() { CHECK(QThread::currentThread() == thread()); }
    void SetEffectState(const std::string& name, bool value)
    {
        OnGui(); CHECK(name == "Screen test"); running = value;
        operations.push_back(value ? "start" : "stop");
    }
    unsigned char* GetEffectListDescription(unsigned* size)
    {
        OnGui(); CHECK(*size == 0); *size += 3;
        return new unsigned char[3]{0x12, 0x34, static_cast<unsigned char>(running)};
    }
    void AboutToLoadProfile() { OnGui(); operations.push_back("about"); }
    void LoadProfileJson(const nlohmann::json& value)
    {
        OnGui();
        if(value.contains("throw")) throw std::runtime_error("test profile exception");
        profile = value; operations.push_back("load");
    }
    nlohmann::json GetProfileJson(bool state) { OnGui(); CHECK(state); ++saves; return profile; }
    void StopAll() { OnGui(); running = false; operations.push_back("unload"); }
    void SetLanguage() { OnGui(); ++language_calls; }
};

class OpenRGBEffectsPlugin : public QObject
{
public:
    explicit OpenRGBEffectsPlugin(OpenRGBEffectTab* target) : ui(target) {}
    void Unload();
    unsigned char* OnSDKCommand(unsigned, unsigned char*, unsigned*);
    void OnProfileAboutToLoad();
    void OnProfileLoad(nlohmann::json);
    nlohmann::json OnProfileSave();
    void SettingsManagerUpdated(unsigned);
    OpenRGBEffectTab* ui;
};

#include "production_dispatch.inc"

static std::vector<unsigned char> Command()
{
    const std::string name = "Screen test";
    const unsigned short length = static_cast<unsigned short>(name.size() + 1);
    std::vector<unsigned char> bytes(2 + length);
    std::memcpy(bytes.data(), &length, 2);
    std::memcpy(bytes.data() + 2, name.c_str(), length);
    return bytes;
}

static void Send(OpenRGBEffectsPlugin& plugin, unsigned id)
{
    auto bytes = Command(); unsigned size = static_cast<unsigned>(bytes.size());
    CHECK(plugin.OnSDKCommand(id, bytes.data(), &size) == nullptr);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    OpenRGBEffectTab ui;
    OpenRGBEffectsPlugin plugin(&ui);
    // The receiver widget, not a plugin object's incidental affinity, decides
    // which thread owns UI operations. This also catches self-blocking calls.
    QThread other_affinity;
    other_affinity.start(); plugin.moveToThread(&other_affinity);
    plugin.OnProfileAboutToLoad();
    plugin.OnProfileLoad({{"name", "gui"}});
    CHECK(plugin.OnProfileSave()["name"] == "gui");
    CHECK(ui.operations == std::vector<std::string>({"about", "load"}));
    Send(plugin, NET_PACKET_ID_START_EFFECT);
    CHECK(ui.running);

    // Settings callbacks can hold the host callback-list mutex. A worker must
    // return even while the GUI deliberately waits for that callback to finish.
    std::thread notification([&] { plugin.SettingsManagerUpdated(0); });
    notification.join();
    CHECK(language_calls == 0);
    QCoreApplication::sendPostedEvents();
    CHECK(language_calls == 1);
    plugin.SettingsManagerUpdated(999);
    QCoreApplication::sendPostedEvents();
    CHECK(language_calls == 1);
    {
        auto* ephemeral = new OpenRGBEffectTab;
        OpenRGBEffectsPlugin transient(ephemeral);
        transient.SettingsManagerUpdated(0);
        delete ephemeral;
    }
    QCoreApplication::sendPostedEvents();
    CHECK(language_calls == 1); // destroyed context cancels the pending post

    // Malformed buffers never reach the UI and are never read past their size.
    const auto before = ui.operations.size();
    unsigned size = 0;
    CHECK(plugin.OnSDKCommand(20, nullptr, nullptr) == nullptr);
    CHECK(plugin.OnSDKCommand(20, nullptr, &size) == nullptr);
    auto bytes = Command();
    for(unsigned length = 0; length < bytes.size(); ++length)
        CHECK(plugin.OnSDKCommand(20, bytes.data(), &length) == nullptr);
    auto bad = bytes; bad.back() = 'x'; size = static_cast<unsigned>(bad.size());
    CHECK(plugin.OnSDKCommand(20, bad.data(), &size) == nullptr);
    bad = bytes; bad[3] = 0;
    CHECK(plugin.OnSDKCommand(20, bad.data(), &size) == nullptr);
    bad = bytes; bad[0] = 0; bad[1] = 0;
    CHECK(plugin.OnSDKCommand(20, bad.data(), &size) == nullptr);
    CHECK(plugin.OnSDKCommand(999, bytes.data(), &size) == nullptr);
    CHECK(ui.operations.size() == before);

    std::atomic<bool> done{false};
    unsigned checkpoint_calls = 0;
    QTimer checkpoint;
    QObject::connect(&checkpoint, &QTimer::timeout, &app, [&] {
        // Session autosave must only read; it must not re-enter a load/stop.
        const auto count = ui.operations.size();
        CHECK(plugin.OnProfileSave().is_object());
        CHECK(ui.operations.size() == count); ++checkpoint_calls;
    });
    checkpoint.start(1);
    std::thread network([&] {
        plugin.OnProfileAboutToLoad();
        plugin.OnProfileLoad({{"name", "network"}});
        CHECK(plugin.OnProfileSave()["name"] == "network");
        for(unsigned i = 0; i < 64; ++i)
        {
            Send(plugin, 20);
            unsigned reply_size = 99; // response length never inherits input size
            auto* reply = plugin.OnSDKCommand(0, nullptr, &reply_size);
            CHECK(reply_size == 3 && reply[0] == 0x12 && reply[1] == 0x34 && reply[2] == 1);
            delete[] reply;
            Send(plugin, 21);
        }
        bool caught = false;
        try { plugin.OnProfileLoad({{"throw", true}}); }
        catch(const std::runtime_error& e) { caught = std::string(e.what()) == "test profile exception"; }
        CHECK(caught);
        CHECK(plugin.OnProfileSave()["name"] == "network");
        plugin.Unload();
        done = true;
        QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
    });
    app.exec();
    network.join(); checkpoint.stop();
    CHECK(done && checkpoint_calls > 0 && !ui.running);
    CHECK(ui.operations.back() == "unload");
    CHECK(ui.operations[3] == "about" && ui.operations[4] == "load");
    CHECK(ui.operations.size() == 134);
    QMetaObject::invokeMethod(&plugin, [&] { plugin.moveToThread(app.thread()); }, Qt::BlockingQueuedConnection);
    other_affinity.quit(); other_affinity.wait();
    std::cout << "PASS " << checks << " assertions; SDK/profile UI affinity, ordered returns, checkpoint reads, "
                 "callback lifetime, malformed packets, exception relay; no hardware\n";
}
