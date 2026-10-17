/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "DiscoveryFixture.h"

// Real Discovery worker, private HTTP listener, no installed Better instance.
struct CaptureServer
{
    Server http;
    QString scene=SceneId,previous_scene,lease;
    bool recording=false,paused=false,changing=false,revoked=false,effective=false,owned=false;
    bool previous_recording=false,previous_paused=false;
    unsigned revision=1,scene_revision=1,manual_revision=0,previous_manual=0;
    unsigned acquires=0,starts=0,releases=0,renews=0,scene_changes=0;
    unsigned acquire_delay=0,capture_delay=0;
    bool lose_capture=false,change_scene_on_renew=false;
    QString capture_error,acquire_error;
    QVector<unsigned> ttl;
    std::function<void()> on_capture;
    CaptureServer()
    {
        http.route=[this](QByteArray method,QByteArray path,QJsonObject body){return Route(method,path,body);};
    }
    void Capability(bool enabled)
    {
        auto caps=DiscoveryJSON().value("capabilities").toArray();
        if(enabled)caps.append("captureControl");http.discovery["capabilities"]=caps;
    }
    QJsonObject Scene()const
    {
        return {{"activeSceneId",scene.isEmpty()?QJsonValue(QJsonValue::Null):QJsonValue(scene)},
            {"stateRevision",int(scene_revision)},{"manualRevision",int(manual_revision)},
            {"sceneLoading",false},{"isRecording",recording},{"isPaused",paused},{"isStopping",changing},
            {"isCaptureChanging",changing},{"captureControlRevoked",revoked}};
    }
    QJsonObject Control()const
    {return {{"revision",int(revision)},{"scene",Scene()},{"lease",owned?QJsonValue(QJsonObject{{"id",lease}}):QJsonValue(QJsonValue::Null)}};}
    QJsonObject Envelope()const
    {
        return {{"controlRevision",int(revision)},{"scene",Scene()},
            {"image",QJsonObject{{"outputId","canvas-raw"},{"channel",Prefix+"-Raw"},
                {"format","BGRA8_OPAQUE_SRGB"},{"generation","7000"},{"sequence","2"}}}};
    }
    QJsonObject Ack(bool capture=false)const
    {
        return {{"apiVersion",1},{"result",capture?"capture_applied":effective?"effective":"pending_frame"},
            {"control",Control()},{"targetControlRevision",int(revision)},{"targetSceneRevision",int(scene_revision)},
            {"effectiveState",effective?QJsonValue(Envelope()):QJsonValue(QJsonValue::Null)}};
    }
    QJsonObject Error(QString code,int status=409){http.status=status;return {{"apiVersion",1},{"error",code}};}
    QJsonObject Route(QByteArray method,QByteArray path,QJsonObject body)
    {
        http.status=200;http.delay_ms=0;http.drop_response=false;
        if(path.endsWith("discovery"))return http.discovery;
        if(path.endsWith("scenes"))return http.scenes;
        if(method=="POST"&&path.endsWith("/leases"))
        {
            ++acquires;if(!acquire_error.isEmpty())return Error(acquire_error);
            Check(!owned,"never acquire a second lease while the first is owned");
            Check(!body.contains("overrides"),"capture never changes appearance preferences");
            ttl.push_back(body.value("ttlSeconds").toInt());
            previous_scene=scene;previous_recording=recording;previous_paused=paused;previous_manual=manual_revision;
            owned=true;revoked=false;lease=QString("eeeeeeee-eeee-eeee-eeee-%1").arg(acquires,12,10,QChar('0'));
            if(body.contains("sceneId"))scene=body.value("sceneId").toString();
            ++revision;++scene_revision;http.delay_ms=acquire_delay;http.status=effective?200:202;return Ack();
        }
        if(path.endsWith("/status"))return {{"apiVersion",1},{"control",Control()},
            {"output",QJsonObject{{"effectiveState",effective?QJsonValue(Envelope()):QJsonValue(QJsonValue::Null)}}}};
        Check(owned&&path.contains(lease.toLatin1()),"mutations use the currently owned lease capability");
        if(path.endsWith("/capture"))
        {
            if(!capture_error.isEmpty())return Error(capture_error,capture_error=="capture_failed"?503:409);
            Check(body.size()==1&&body.value("recording").isBool()&&body.value("recording").toBool(),
                "only explicit capture=true is sent; cleanup never blindly stops recording");
            ++starts;recording=true;paused=false;++revision;++scene_revision;
            http.delay_ms=capture_delay;http.drop_response=lose_capture;
            if(lose_capture)changing=true;
            if(on_capture)on_capture();
            return Ack(true);
        }
        if(path.endsWith("/renew"))
        {
            ++renews;ttl.push_back(body.value("ttlSeconds").toInt());++revision;
            if(change_scene_on_renew){change_scene_on_renew=false;scene.clear();++manual_revision;++scene_revision;}
            return Control();
        }
        if(path.endsWith("/scene"))
        {
            if(manual_revision!=previous_manual)return Error("manual_override");
            ++scene_changes;scene=body.value("sceneId").toString();++revision;++scene_revision;
            http.status=effective?200:202;return Ack();
        }
        if(method=="DELETE")
        {
            ++releases;
            if(!revoked){recording=previous_recording;paused=previous_paused;}
            if(manual_revision==previous_manual)scene=previous_scene;
            owned=false;revoked=false;++revision;++scene_revision;http.status=effective?200:202;return Ack();
        }
        return Error("not_found",404);
    }
};

int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    try
    {
        QTemporaryDir dir;Check(dir.isValid(),"private capture fixture directory");
        CaptureServer server;const auto path=dir.filePath("connection.json");Write(path,Descriptor(server.http.Port()));
        Config config;config.descriptor_path=path;config.refresh_ms=60000;config.request_timeout_ms=250;
        auto client=Discovery::Acquire(config);
        Check(Wait([&]{return client->Read()->Ready();}),"capture fixture discovery ready");
        Check(server.acquires==0&&server.starts==0,"discovery alone is passive");
        Check(client->RequestCapture("capture-a",true)==RequestResult::Accepted,"explicit capture interest accepted");
        Check(Wait([&]{return client->ReadControl("capture-a")->detail=="capture_control_unsupported";}),"old producer reports passive fallback");
        Check(server.acquires==0&&server.starts==0,"missing capability never acquires or changes capture");

        server.Capability(true);server.acquire_delay=2100;server.capture_delay=2200;client->Refresh();
        Check(Wait([&]{return client->ReadControl("capture-a")->capture_active&&server.renews==1;},7000),
            "delayed acquisition and capture acknowledgements succeed without a first frame");
        Check(server.acquires==1&&server.starts==1&&server.ttl==QVector<unsigned>({300,30}),"startup lease300 shortens to30 after capture acknowledgement");
        Check(client->ReadControl("capture-a")->effective_generation==0,"capture active does not invent an available image");
        const auto minimum_capture=client->ReadControl("capture-a")->minimum_capture_revision;
        Check(minimum_capture>0,"capture acknowledgement establishes a minimum frame control revision");
        server.acquire_delay=server.capture_delay=0;
        Check(client->RequestCapture("capture-b",true)==RequestResult::Accepted,"second effect shares capture interest");
        client->ReleaseOwner("capture-a");
        Check(Wait([&]{return client->ReadControl("capture-b")->capture_active;}),"remaining effect retains active capture");
        Check(server.releases==0&&server.acquires==1&&server.starts==1,"first effect stop does not release shared capture");

        // Longer than the scene-only pending-frame timeout. Capture ownership is
        // independent of frame availability and must still renew every ten seconds.
        QElapsedTimer no_frame;no_frame.start();
        Check(Wait([&]{return no_frame.elapsed()>26000;},27500),"bounded no-first-frame observation");
        Check(server.renews>=3&&server.releases==0&&server.starts==1&&client->ReadControl("capture-b")->capture_active,
            "healthy capture survives absent first images and renews its30-second lease");

        ++server.revision; // Appearance edit, no playback revocation.
        Check(Wait([&]{return client->ReadControl("capture-b")->control_revision==server.revision||client->ReadControl("capture-b")->capture_active;}),"appearance state stays capture-active");
        server.changing=true;++server.revision;++server.scene_revision;
        Check(Wait([&]{return !client->ReadControl("capture-b")->capture_active;}),"source transition exposes waiting status");
        Check(server.releases==0,"non-playback transition does not revoke capture");
        server.changing=false;++server.revision;++server.scene_revision;
        Check(Wait([&]{return client->ReadControl("capture-b")->capture_active;}),"capture resumes after transition without a second start");
        ++server.manual_revision;++server.revision;++server.scene_revision;server.scene.clear();
        QElapsedTimer observation;observation.start();Wait([&]{return observation.elapsed()>600;},1000);
        Check(server.releases==0&&server.starts==1&&client->ReadControl("capture-b")->capture_active,"canvas edits retain independent capture ownership");
        Check(client->ReadControl("capture-b")->minimum_capture_revision==minimum_capture,"renewals and edits never pin capture to a newer frame revision");
        server.effective=true;client->ReleaseOwner("capture-b");
        Check(Wait([&]{return server.releases==1;}),"last interested effect releases capture lease");
        Check(!server.recording&&!server.paused,"last release restores the stopped baseline");

        server.scene=SceneId;server.recording=true;server.paused=false;
        client->RequestCapture("user-recording",true);
        Check(Wait([&]{return client->ReadControl("user-recording")->capture_active&&server.ttl.last()==30;}),"capture can join a user recording");
        client->ReleaseOwner("user-recording");Check(Wait([&]{return !server.owned;}),"user-recording lease released");
        Check(server.recording&&!server.paused,"release preserves a pre-existing user recording");server.recording=false;

        const QString second="11111111-2222-3333-4444-555555555555";
        client->RequestCapture("mixed-capture",true);
        Check(Wait([&]{return client->ReadControl("mixed-capture")->capture_active&&server.ttl.last()==30;}),"mixed capture lease ready");
        const auto before_mixed_acquire=server.acquires,before_mixed_start=server.starts;
        client->RequestScene("mixed-scene",second);
        Check(Wait([&]{return server.scene==second&&client->ReadControl("mixed-scene")->phase==ControlPhase::Effective;}),"scene joins the existing capture lease");
        Check(server.acquires==before_mixed_acquire&&server.starts==before_mixed_start,"compatible scene interest creates no second lease or start");
        client->ReleaseOwner("mixed-capture");
        Check(Wait([&]{return server.acquires==before_mixed_acquire+1&&client->ReadControl("mixed-scene")->phase==ControlPhase::Effective;}),"last capture interest restores then reacquires scene-only");
        Check(!server.recording&&server.scene==second&&server.ttl.last()==30,"remaining scene preserves baseline recording state");
        client->ReleaseOwner("mixed-scene");Check(Wait([&]{return !server.owned;}),"mixed scene cleanup");

        client->RequestScene("both",second);client->RequestCapture("both",true);
        Check(Wait([&]{return client->ReadControl("both")->capture_active&&server.ttl.last()==30;}),"combined scene and capture ready");
        auto before=server.acquires;client->ReleaseScene("both");
        Check(Wait([&]{return server.acquires==before+1&&client->ReadControl("both")->capture_active&&server.ttl.last()==30;}),"last scene interest restores then reacquires capture-only");
        Check(server.scene==SceneId,"capture-only lease follows the original restored scene");

        server.revoked=true;server.paused=true;++server.revision;++server.scene_revision;
        Check(Wait([&]{return client->ReadControl("both")->phase==ControlPhase::Superseded&&!server.owned;}),"manual pause revokes and releases capture");
        auto starts_before=server.starts;client->RequestScene("both",second);
        Check(Wait([&]{return server.owned&&server.scene==second;}),"scene remains independently selectable after manual pause");
        Check(server.starts==starts_before&&server.paused&&!client->ReadControl("both")->capture_active,
            "changing scene cannot rearm capture after manual playback override");
        client->RequestCapture("both",false);client->RequestCapture("both",true);
        Check(Wait([&]{return client->ReadControl("both")->capture_active&&server.starts==starts_before+1;}),"explicit off/on rearms capture interest");
        client->ReleaseOwner("both");Check(Wait([&]{return !server.owned;}),"rearmed capture cleanup");
        Check(server.recording&&server.paused,"rearmed capture release restores the manually paused baseline");

        server.recording=false;server.paused=false;server.scene=SceneId;
        client->RequestScene("manual-layout",second);client->RequestCapture("manual-layout",true);
        Check(Wait([&]{return client->ReadControl("manual-layout")->capture_active&&server.ttl.last()==30;}),"layout override fixture starts");
        server.scene.clear();++server.manual_revision;++server.revision;++server.scene_revision;
        Check(Wait([&]{auto state=client->ReadControl("manual-layout");return state->phase==ControlPhase::Superseded&&state->capture_active;}),
            "manual layout supersedes only scene selection while capture keeps running");
        client->RequestCapture("manual-layout",false);
        Check(Wait([&]{return !server.owned;}),"capture opt-out releases despite superseded scene interest");
        auto acquire_before=server.acquires;observation.restart();Wait([&]{return observation.elapsed()>600;},1000);
        Check(server.acquires==acquire_before&&server.scene.isEmpty(),"lease cleanup does not revive a superseded scene over manual canvas edits");
        client->RequestCapture("manual-layout",true);
        Check(Wait([&]{return client->ReadControl("manual-layout")->capture_active&&server.ttl.last()==30;}),"new explicit capture interest can follow the edited canvas");
        Check(server.scene.isEmpty(),"capture-only reacquisition excludes the superseded sceneId");
        server.recording=false;server.paused=false;server.revoked=true;++server.revision;++server.scene_revision;
        Check(Wait([&]{return !server.owned&&client->ReadControl("manual-layout")->phase==ControlPhase::Superseded;}),"manual stop releases independently after a canvas edit");
        acquire_before=server.acquires;observation.restart();Wait([&]{return observation.elapsed()>600;},1000);
        Check(server.acquires==acquire_before&&server.scene.isEmpty(),"manual playback stop cannot trigger reacquisition of the old scene");
        client->ReleaseOwner("manual-layout");

        client->RequestScene("scene-first",second);
        Check(Wait([&]{return client->ReadControl("scene-first")->phase==ControlPhase::Effective;}),"scene-only fixture becomes effective");
        server.scene.clear();++server.manual_revision;++server.revision;++server.scene_revision;
        Check(Wait([&]{return !server.owned&&client->ReadControl("scene-first")->phase==ControlPhase::Superseded;}),"manual canvas edit supersedes a scene-only lease");
        client->RequestCapture("scene-first",true);
        Check(Wait([&]{return client->ReadControl("scene-first")->capture_active&&server.ttl.last()==30;}),"new capture opt-in follows a previously superseded scene-only request");
        Check(server.scene.isEmpty(),"capture opt-in cannot revive a revoked scene-only selection");
        client->ReleaseOwner("scene-first");Check(Wait([&]{return !server.owned;}),"scene-only supersession cleanup");

        server.scene=SceneId;server.change_scene_on_renew=true;before=server.releases;
        client->RequestScene("renew-race",second);client->RequestCapture("renew-race",true);
        Check(Wait([&]{return server.ttl.last()==30&&server.scene.isEmpty()&&client->ReadControl("renew-race")->capture_active;}),
            "scene edit inside renewal acknowledgement does not reject successful capture ownership");
        Check(server.releases==before&&server.recording,"renewal scene mismatch never stops unrelated capture");
        client->ReleaseOwner("renew-race");Check(Wait([&]{return !server.owned;}),"renewal race cleanup");

        server.recording=false;server.paused=false;server.lose_capture=true;before=server.releases;
        client->RequestCapture("lost-response",true);
        Check(Wait([&]{return client->ReadControl("lost-response")->detail=="capture_outcome_unknown";}),"lost capture response observes producer status");
        starts_before=server.starts;
        Check(server.changing&&server.releases==before,"unknown in-flight capture is not immediately restored");
        server.changing=false;server.lose_capture=false;
        Check(Wait([&]{return server.releases==before+1;}),"known completion permits owned cleanup after response loss");
        Check(server.starts==starts_before&&!server.recording,"uncertain start is never blindly retried");client->ReleaseOwner("lost-response");

        server.acquire_error="lease_conflict";client->RequestCapture("remote-owner",true);
        Check(Wait([&]{return client->ReadControl("remote-owner")->phase==ControlPhase::Conflict;}),"remote ownership conflict is visible");
        acquire_before=server.acquires;observation.restart();Wait([&]{return observation.elapsed()>600;},1000);
        Check(server.acquires==acquire_before,"conflict never loops acquiring");client->ReleaseOwner("remote-owner");server.acquire_error.clear();

        server.capture_error="capture_failed";client->RequestCapture("failed-start",true);
        Check(Wait([&]{return client->ReadControl("failed-start")->detail=="capture_failed"&&!server.owned;}),"capture failure is surfaced and owned lease restored");
        client->ReleaseOwner("failed-start");server.capture_error.clear();
        server.on_capture=[&]{client->ReleaseOwner("cancel-start");};before=server.releases;
        client->RequestCapture("cancel-start",true);
        Check(Wait([&]{return server.releases==before+1;}),"owner leaving during capture request still releases its known lease");
        server.on_capture={};Check(!server.recording,"cancelled late start restores stopped baseline");
        Check(server.http.authenticated==server.http.requests&&server.http.cookies==0,"all capture transport calls remain authenticated and cookieless");
        client.reset();std::cout<<"PASS "<<checks<<" Better capture-control checks\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
