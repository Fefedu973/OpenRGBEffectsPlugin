// SPDX-License-Identifier: GPL-2.0-or-later
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <iostream>
#include <thread>

enum { RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED = 1 };
class OpenRGBEffectsPlugin : public QObject
{
    Q_OBJECT
public:
    unsigned calls = 0;
    bool wrong_thread = false;
    void ResourceManagerUpdated(unsigned);
public slots:
    void UpdateControllers()
    {
        wrong_thread |= QThread::currentThread() != thread();
        ++calls;
    }
};
#include "production_callback.inc"
#include "moc_callback_tests.cpp"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    OpenRGBEffectsPlugin plugin;
    // Qt can reject the old self-blocking call or deadlock here. The runner
    // bounds the entire process and verifies the remap was actually executed.
    plugin.ResourceManagerUpdated(RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED);
    if(plugin.calls != 1 || plugin.wrong_thread) {
        std::cerr << "GUI result calls=" << plugin.calls << " wrong_thread=" << plugin.wrong_thread
                  << " Qt=" << qVersion() << " slot=" << plugin.metaObject()->indexOfMethod("UpdateControllers()") << '\n';
        return 2;
    }
    plugin.ResourceManagerUpdated(0);
    if(plugin.calls != 1) return 3;

    std::atomic<bool> worker_returned{false};
    std::thread worker([&] {
        plugin.ResourceManagerUpdated(RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED);
        worker_returned = true;
        QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
    });
    app.exec();
    worker.join();
    if(!worker_returned || plugin.calls != 2 || plugin.wrong_thread) return 4;
    std::cout << "PASS: GUI callback is synchronous; worker callback completes on GUI; unrelated event ignored\n";
}
