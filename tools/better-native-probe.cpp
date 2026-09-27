// SPDX-License-Identifier: GPL-2.0-or-later
// Short-lived passive diagnostic. Never acquires a scene lease, exports an
// image, prints credentials/URLs, or starts Better/capture/another application.
#include "ScreenSources/BetterDiscovery.h"
#include "ScreenSources/BetterFrameSource.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <algorithm>
#include <chrono>
#include <iostream>

static int Usage()
{
    std::cerr << "Usage: better-native-probe [--descriptor ABSOLUTE_PATH] [--wait-ms 100..15000]\n"
                 "Read-only discovery and paired-frame check. Exit 0: fresh pair; 2: unavailable; 64: invalid arguments.\n";
    return 64;
}

int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    QString descriptor;
    unsigned wait_ms=10000;
    const auto arguments=app.arguments();
    for(int i=1;i<arguments.size();++i)
    {
        if(arguments[i]=="--descriptor" && i+1<arguments.size())
        { descriptor=arguments[++i];if(descriptor.isEmpty() || !QFileInfo(descriptor).isAbsolute())return Usage(); }
        else if(arguments[i]=="--wait-ms" && i+1<arguments.size())
        { bool ok=false;wait_ms=arguments[++i].toUInt(&ok);if(!ok || wait_ms<100 || wait_ms>15000)return Usage(); }
        else return Usage();
    }
    QElapsedTimer timer;timer.start();
    QJsonObject report{{"diagnostic_version",1},{"read_only",true},{"ready",false}};
    bool ready=false;
    try
    {
        better_source::Config config;
        config.descriptor_path=descriptor;config.refresh_ms=250;config.request_timeout_ms=1000;
        auto discovery=better_source::Discovery::Acquire(config);
        auto frames=better_source::FrameSource::Acquire(discovery);
        std::shared_ptr<const better_source::FrameSnapshot> pair;
        while(timer.elapsed()<wait_ms)
        {
            QCoreApplication::processEvents();
            pair=frames->Read();
            if(pair->Usable()) { ready=true;break; }
            QThread::msleep(8);
        }
        const auto connection=discovery->Read();
        report["discovery_status"]=better_source::StateName(connection->state);
        report["frame_status"]=better_source::FrameStateName(pair?pair->state:better_source::FrameState::Starting);
        report["ready"]=ready;
        if(ready)
        {
            const auto ttl=std::chrono::duration_cast<std::chrono::milliseconds>(pair->expires-std::chrono::steady_clock::now()).count();
            report["raw"]=QJsonObject{{"width",pair->raw.width()},{"height",pair->raw.height()},
                {"generation",QString::number(qulonglong(pair->raw_generation))},{"sequence",QString::number(qulonglong(pair->raw_sequence))}};
            report["coverage"]=QJsonObject{{"width",pair->coverage.width()},{"height",pair->coverage.height()},
                {"generation",QString::number(qulonglong(pair->coverage_generation))},{"sequence",QString::number(qulonglong(pair->coverage_sequence))}};
            report["metadata"]=QJsonObject{{"schema",pair->metadata.value("schema")},{"version",pair->metadata.value("version")},
                {"state_revision",QString::number(qulonglong(pair->state_revision))}};
            report["remaining_ttl_ms"]=double(std::max<std::int64_t>(0,ttl));
            // Recheck at the moment of reporting: a retained shared snapshot
            // does not become fresh just because the diagnostic still owns it.
            ready=pair->Usable() && connection->Ready() && connection->instance_id==pair->instance_id;
            report["ready"]=ready;
        }
        frames.reset();discovery.reset();
    }
    catch(const std::exception&)
    {
        // Exception strings may contain filesystem/transport details. Report
        // only an aggregate state, never producer documents or credentials.
        report["error"]="probe_unavailable";ready=false;report["ready"]=false;
    }
    report["elapsed_ms"]=double(timer.elapsed());
    std::cout<<QJsonDocument(report).toJson(QJsonDocument::Compact).constData()<<'\n';
    return ready?0:2;
}
