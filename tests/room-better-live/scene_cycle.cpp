// SPDX-License-Identifier: GPL-2.0-or-later
// Explicit opt-in scene mutation test. Original scene/settings are observed,
// never forced back: lease release owns restoration and manual edits take priority.
#include "ScreenSources/BetterDiscovery.h"
#include "ScreenSources/BetterFrameSource.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QThread>
#include <QUuid>
#include <functional>
#include <iostream>

static bool Wait(QElapsedTimer& clock,qint64 until,const std::function<bool()>& predicate)
{
    do{QCoreApplication::processEvents();if(predicate())return true;QThread::msleep(10);}while(clock.elapsed()<until);
    return false;
}
static QJsonObject State(const better_source::FrameSnapshot& frame)
{
    return {{"scene_id",frame.scene_id},{"instance_id",frame.instance_id},
            {"control_revision",QString::number(qulonglong(frame.control_revision))},
            {"scene_revision",QString::number(qulonglong(frame.scene_revision))},
            {"raw_generation",QString::number(qulonglong(frame.raw_generation))},
            {"raw_sequence",QString::number(qulonglong(frame.raw_sequence))},
            {"effectiveSettings",frame.metadata.value("effectiveSettings")}};
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);QString report_path,descriptor;
    const auto args=app.arguments();
    for(int i=1;i<args.size();++i)
    {
        if(args[i]=="--report"&&i+1<args.size())report_path=args[++i];
        else if(args[i]=="--descriptor"&&i+1<args.size())descriptor=args[++i];
        else return 64;
    }
    if(!QFileInfo(report_path).isAbsolute()||(!descriptor.isEmpty()&&!QFileInfo(descriptor).isAbsolute()))return 64;
    QElapsedTimer clock;clock.start();QJsonObject report{{"scene_mutation_opt_in",true},{"hardware_output",false},{"image_export",false}};
    std::shared_ptr<better_source::Discovery> discovery;
    std::shared_ptr<better_source::FrameSource> frames;
    const QString owner="native-validation-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool claimed=false,manual=false,complete=false,restored=false;
    std::shared_ptr<const better_source::FrameSnapshot> initial,last;
    QJsonArray targets;
    try
    {
        better_source::Config config;config.descriptor_path=descriptor;config.refresh_ms=250;config.request_timeout_ms=1000;
        discovery=better_source::Discovery::Acquire(config);frames=better_source::FrameSource::Acquire(discovery);
        if(!Wait(clock,6000,[&]{initial=frames->Read();return initial->Usable()&&discovery->Read()->Ready();}))throw std::runtime_error("initial_unavailable");
        report["initial"]=State(*initial);
        QString main,full;for(const auto& scene:discovery->Read()->scenes)
        {if(scene.name=="Main screen")main=scene.id;if(scene.name=="Complete screen setup")full=scene.id;}
        if(main.isEmpty()||full.isEmpty())throw std::runtime_error("test_scenes_unavailable");
        const QString first=initial->scene_id==main?full:main,second=first==main?full:main;
        for(const auto& target:{first,second})
        {
            if(discovery->RequestScene(owner,target)!=better_source::RequestResult::Accepted)throw std::runtime_error("local_scene_request_rejected");
            claimed=true;
            const auto until=std::min<qint64>(clock.elapsed()+6500,19000);
            bool success=false;
            Wait(clock,until,[&]
            {
                const auto control=discovery->ReadControl(owner);const auto pair=frames->Read();
                if(control->phase==better_source::ControlPhase::Superseded){manual=true;return true;}
                if(control->phase==better_source::ControlPhase::Conflict||control->phase==better_source::ControlPhase::Unavailable)return true;
                if(pair->Usable()&&control->phase==better_source::ControlPhase::Effective&&
                   control->instance_id==initial->instance_id&&pair->instance_id==control->instance_id&&
                   pair->scene_id==target&&control->scene_id==target&&
                   pair->scene_revision==control->scene_revision&&pair->control_revision==control->control_revision&&
                   pair->raw_generation==control->effective_generation&&pair->raw_sequence>=control->minimum_sequence)
                {success=true;last=pair;return true;}
                return false;
            });
            QJsonObject result{{"requested_scene",target},{"effective",success},{"elapsed_ms",double(clock.elapsed())}};
            if(success)result["state"]=State(*last);targets.append(result);
            if(!success)throw std::runtime_error(manual?"manual_change_respected":"scene_not_effective");
        }
        complete=true;
    }
    catch(const std::exception& error){report["failure"]=QString::fromLatin1(error.what());}
    // Finally: always release known local intent. Do not select the old scene
    // manually or reacquire after a supersession, even if restoration differs.
    std::uint64_t before_release=last?last->control_revision:0;
    if(frames){const auto current=frames->Read();if(current->Usable())before_release=std::max(before_release,current->control_revision);}
    if(claimed&&discovery)
    {
        discovery->ReleaseScene(owner);claimed=false;report["release_requested"]=true;
        if(initial && !manual)
        {
            restored=Wait(clock,29000,[&]
            {
                const auto pair=frames->Read();
                if(!pair->Usable()||pair->instance_id!=initial->instance_id||pair->control_revision<=before_release)return false;
                if(pair->scene_id==initial->scene_id&&pair->metadata.value("effectiveSettings")==initial->metadata.value("effectiveSettings"))
                {report["restored_state"]=State(*pair);return true;}
                return false;
            });
        }
    }
    report["targets"]=targets;report["both_targets_effective"]=complete;report["manual_change_observed"]=manual;
    report["original_scene_and_settings_restored"]=restored;
    // Destroy readers before the HTTP owner. Its bounded cleanup retries a
    // release still in flight; an unknown/expired capability is never recreated.
    frames.reset();discovery.reset();report["elapsed_ms"]=double(clock.elapsed());
    QSaveFile file(report_path);
    const bool saved=file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(report).toJson(QJsonDocument::Indented))>=0&&file.commit();
    QJsonObject summary{{"both_targets_effective",complete},{"restored",restored},{"manual_change_observed",manual},
                        {"report_saved",saved},{"elapsed_ms",double(clock.elapsed())}};
    std::cout<<QJsonDocument(summary).toJson(QJsonDocument::Compact).constData()<<'\n';
    return complete&&restored&&saved?0:2;
}
