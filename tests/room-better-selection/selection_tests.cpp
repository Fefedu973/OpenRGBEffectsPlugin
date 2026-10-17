// SPDX-License-Identifier: GPL-2.0-or-later
#include "Effects/SignalFavorites/ScreenSourceSelection.h"
#include "WindowsScreenCapturer.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

// Only the I/O boundaries are replaced. The real QWidget, signals, JSON loading,
// Reconfigure/RefreshBetter/SetRunning and image-read gates are exercised.
namespace fake
{
struct Event { QString operation,owner;bool enabled=false; };
std::vector<Event> events;
std::shared_ptr<better_source::Snapshot> discovered;
std::shared_ptr<better_source::ControlSnapshot> control;
std::shared_ptr<better_source::FrameSnapshot> frame;
std::set<QString> scene_owners,capture_owners;
unsigned paired_acquires=0,discovery_acquires=0,native_starts=0;
const QString scene_id="aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
const QString instance="12345678-1234-1234-1234-123456789abc";
void Reset()
{
    events.clear();scene_owners.clear();capture_owners.clear();paired_acquires=discovery_acquires=native_starts=0;
    discovered=std::make_shared<better_source::Snapshot>();discovered->state=better_source::State::Ready;
    discovered->instance_id=instance;discovered->raw_channel="BetterCapture-0123456789abcdef01234567-Raw";
    discovered->coverage_channel="BetterCapture-0123456789abcdef01234567-Coverage";
    discovered->capabilities={"captureControl"};discovered->scenes={{scene_id,"Desk"}};
    control=std::make_shared<better_source::ControlSnapshot>();control->instance_id=instance;
    frame=std::make_shared<better_source::FrameSnapshot>();frame->state=better_source::FrameState::Ready;
    frame->instance_id=instance;frame->raw=QImage(4,4,QImage::Format_ARGB32);frame->raw.fill(Qt::red);frame->coverage=frame->raw;
    frame->raw_generation=10;frame->raw_sequence=1;frame->expires=std::chrono::steady_clock::time_point::max();
}
unsigned Count(const QString& operation)
{return unsigned(std::count_if(events.begin(),events.end(),[&](const Event& e){return e.operation==operation;}));}
}

WindowsScreenCapturer::WindowsScreenCapturer()=default;
WindowsScreenCapturer::~WindowsScreenCapturer()=default;
void WindowsScreenCapturer::Start(){++fake::native_starts;}
void WindowsScreenCapturer::Stop(){}
void WindowsScreenCapturer::SetScreen(int){}

namespace better_source
{
class Discovery::Impl{};
Discovery::Discovery(const Config&):impl(std::make_unique<Impl>()){}
Discovery::~Discovery()=default;
std::shared_ptr<Discovery> Discovery::Acquire(const Config& c)
{
    ++fake::discovery_acquires;
    static std::map<QString,std::weak_ptr<Discovery>> cache;
    if(auto existing=cache[c.descriptor_path].lock())return existing;
    auto result=std::shared_ptr<Discovery>(new Discovery(c));cache[c.descriptor_path]=result;return result;
}
QString Discovery::DefaultDescriptorPath(){return "synthetic";}
std::shared_ptr<const Snapshot> Discovery::Read()const{return fake::discovered;}
void Discovery::Refresh(){}
RequestResult Discovery::RequestScene(const QString& owner,const QString& scene)
{
    fake::events.push_back({"scene",owner});
    if(scene.isEmpty())fake::scene_owners.erase(owner);else fake::scene_owners.insert(owner);
    return RequestResult::Accepted;
}
void Discovery::ReleaseScene(const QString& owner){fake::events.push_back({"release_scene",owner});fake::scene_owners.erase(owner);}
RequestResult Discovery::RequestCapture(const QString& owner,bool enabled)
{
    fake::events.push_back({"capture",owner,enabled});
    if(enabled)fake::capture_owners.insert(owner);else fake::capture_owners.erase(owner);
    return RequestResult::Accepted;
}
void Discovery::ReleaseOwner(const QString& owner)
{fake::events.push_back({"release_owner",owner});fake::scene_owners.erase(owner);fake::capture_owners.erase(owner);}
std::shared_ptr<const ControlSnapshot> Discovery::ReadControl(const QString&)const{return fake::control;}
class FrameSource::Impl{};
FrameSource::FrameSource(std::shared_ptr<Discovery>):impl(std::make_unique<Impl>()){}
FrameSource::~FrameSource()=default;
std::shared_ptr<FrameSource> FrameSource::Acquire(std::shared_ptr<Discovery> d)
{++fake::paired_acquires;return std::shared_ptr<FrameSource>(new FrameSource(std::move(d)));}
std::shared_ptr<const FrameSnapshot> FrameSource::Read()const{return fake::frame;}
bool FrameSnapshot::Usable()const{return Ready()&&!raw.isNull()&&!coverage.isNull()&&std::chrono::steady_clock::now()<=expires;}
class AppearanceInput::Impl{};
AppearanceInput::AppearanceInput(std::shared_ptr<FrameSource>,unsigned,unsigned):impl(std::make_unique<Impl>()){}
AppearanceInput::~AppearanceInput()=default;
std::shared_ptr<const ShaderRenderGraphFrame> AppearanceInput::Read()const{return std::make_shared<const ShaderRenderGraphFrame>();}
QString AppearanceInput::Status()const{return "Synthetic appearance";}
void AppearanceInput::Resize(unsigned,unsigned){}
}
namespace screen_source
{
const char* StateName(State){return "synthetic";}
bool Snapshot::Usable()const{return bool(frame);}
Source::Source(const Config& c):config(c){}
Source::~Source()=default;
std::shared_ptr<Source> Source::Acquire(const Config& c){return std::shared_ptr<Source>(new Source(c));}
Snapshot Source::Read()const
{
    auto image=std::make_shared<Frame>();image->image=fake::frame->raw;image->sequence=1;image->generation=10;image->expires=std::chrono::steady_clock::time_point::max();
    return {State::Live,image,1,{}};
}
}

static unsigned checks=0;
static void Check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
static bool Wait(const std::function<bool()>& done,int ms=1500)
{QElapsedTimer timer;timer.start();while(!done()&&timer.elapsed()<ms){QApplication::processEvents();QThread::msleep(2);}return done();}
static void Tick(){QElapsedTimer timer;timer.start();Wait([&]{return timer.elapsed()>600;},700);}
static nlohmann::json Settings(bool capture=false)
{return {{"kind","better"},{"connection_file","synthetic"},{"capture_on_demand",capture},{"follow_better_appearance",false}};}
template<typename T>static T* Child(ScreenSourceSelection& widget,const char* name)
{auto* result=widget.findChild<T*>(name);Check(result!=nullptr,"Expected production control exists");return result;}

int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    try
    {
        fake::Reset();
        {
            ScreenSourceSelection widget(nullptr,true);widget.Load(Settings(true));widget.show();Tick();
            Check(widget.Save()["capture_on_demand"]==true,"Capture opt-in survives JSON load/save");
            Check(fake::discovery_acquires>0,"Visible stopped settings may discover connection information");
            Check(fake::Count("capture")==0&&fake::Count("scene")==0,"Showing/loading stopped settings never requests capture or scene control");
            auto* option=Child<QCheckBox>(widget,"screen_source_capture_on_demand");
            option->setChecked(false);option->setChecked(true);Tick();
            Check(fake::Count("capture")==0,"Changing the option while effect is inactive does not claim capture");
            auto* inactiveScenes=Child<QComboBox>(widget,"screen_source_scene");inactiveScenes->setCurrentIndex(1);inactiveScenes->setCurrentIndex(0);Tick();
            Check(fake::Count("scene")==0&&fake::Count("capture")==0,"Selecting a scene in stopped settings does not take control");
            widget.SetRunning(true);
            Check(fake::Count("capture")==1&&fake::capture_owners.size()==1,"Actual effect activation registers exactly one capture interest");
            widget.SetRunning(true);Tick();
            Check(fake::Count("capture")==1,"Repeated active state and status refresh do not re-register capture");
            auto acquisitions=fake::discovery_acquires;auto releases=fake::Count("release_owner");
            auto* appearance=Child<QCheckBox>(widget,"screen_source_follow_appearance");
            appearance->setChecked(true);appearance->setChecked(false);widget.SetOutputSize(800,600);Tick();
            Check(fake::discovery_acquires==acquisitions&&fake::Count("release_owner")==releases&&fake::Count("capture")==1,
                "Appearance/output size edits retain ownership and the initial baseline");
            fake::control->phase=better_source::ControlPhase::Superseded;fake::control->detail="manual_override";
            appearance->setChecked(true);appearance->setChecked(false);Tick();
            Check(fake::Count("release_owner")==releases&&fake::Count("capture")==1,"Appearance changes do not rearm manually revoked capture");
            Check(Child<QLabel>(widget,"screen_source_status")->text().contains("manually"),"Manual override gets a readable status");
            auto* scenes=Child<QComboBox>(widget,"screen_source_scene");scenes->setCurrentIndex(1);Tick();
            Check(fake::Count("capture")==1&&fake::Count("release_owner")==releases,"Scene change does not issue a new capture claim or release owner from the UI");
            widget.SetRunning(false);
            Check(fake::capture_owners.empty()&&fake::scene_owners.empty(),"Stopping the effect releases both kinds of ownership");
            Check(std::none_of(fake::events.begin(),fake::events.end(),[](const fake::Event& e){return e.operation=="capture"&&!e.enabled;}),
                "Stopping uses ReleaseOwner instead of a blind capture=false request");
            widget.Load({{"kind","better"}});
            Check(widget.Save()["capture_on_demand"]==false,"Legacy settings without the option remain passive");
            widget.Load({{"kind","better"},{"capture_on_demand","true"}});
            Check(widget.Save()["capture_on_demand"]==false,"Nonboolean persisted values do not activate capture");
        }
        fake::Reset();
        {
            ScreenSourceSelection first,second;first.Load(Settings(true));second.Load(Settings(true));
            first.SetRunning(true);second.SetRunning(true);
            Check(fake::capture_owners.size()==2,"Two active effects have distinct owner interests");
            first.SetRunning(false);
            Check(fake::capture_owners.size()==1,"Stopping one effect keeps the other owner's interest");
        }
        Check(fake::capture_owners.empty(),"Destroying the final active widget releases its interest");
        fake::Reset();
        {
            ScreenSourceSelection passive;passive.Load(Settings());passive.SetRunning(true);Tick();
            Check(fake::Count("capture")==0&&fake::Count("scene")==0,"Active default Better source remains a passive reader");
            Check(bool(passive.Latest()),"Passive raw image reading remains available");
        }
        fake::Reset();fake::discovered->capabilities.clear();
        {
            ScreenSourceSelection widget;widget.Load(Settings(true));widget.SetRunning(true);
            Check(fake::paired_acquires==0&&bool(widget.Latest()),"Unsupported capture API uses passive frames without exact-control gating");
            fake::discovered->capabilities={"captureControl"};fake::control->phase=better_source::ControlPhase::Pending;
            Check(Wait([&]{return fake::paired_acquires>0;}),"Capability appearing on the same channel creates the required paired metadata reader");
            Check(!widget.Latest(),"Newly supported capture control blocks unacknowledged raw frames");
            fake::control->capture_active=true;fake::control->phase=better_source::ControlPhase::Effective;
            Check(!widget.Latest(),"An active flag without an acknowledgement revision is insufficient");
            fake::control->control_revision=3;fake::control->minimum_capture_revision=3;fake::frame->control_revision=2;
            Check(!widget.Latest(),"Capture acknowledgement never relabels the older stopped generation as a running image");
            fake::frame->control_revision=8;fake::frame->scene_revision=12;
            Check(bool(widget.Latest()),"Capture-only follows a fresh matched frame after manual canvas/appearance revisions");
            fake::control->control_revision=15;
            Check(bool(widget.Latest()),"Renewal revisions do not move the initial capture acknowledgement floor");
            fake::frame->instance_id="different-instance";
            Check(!widget.Latest(),"Capture-only never accepts a frame from another producer instance");
            fake::frame->instance_id=fake::instance;fake::frame->expires=std::chrono::steady_clock::now()-std::chrono::seconds(1);
            Check(!widget.Latest(),"Capture-only never displays an expired transport frame");
            fake::frame->expires=std::chrono::steady_clock::time_point::max();
            auto* scene=Child<QComboBox>(widget,"screen_source_scene");scene->setCurrentIndex(1);
            fake::control->scene_id=fake::scene_id;fake::control->scene_revision=12;fake::frame->scene_id=fake::scene_id;
            Check(!widget.Latest(),"Explicit scene owners still require exact frame/control association");
            fake::control->control_revision=8;
            Check(bool(widget.Latest()),"Explicit scene image appears when identity and revisions match");
            scene->setCurrentIndex(0);
            fake::control->capture_active=false;
            Check(!widget.Latest(),"Revoked capture control immediately blocks capture-dependent images");
            fake::discovered->capabilities.clear();Tick();
            Check(bool(widget.Latest()),"Capability removal returns to the passive raw reader on the same channel");
        }
        Check(fake::native_starts==0,"All UI checks leave native screen capture untouched");
        std::cout<<"PASS: "<<checks<<" production selection-widget assertions (offscreen, no network/capture).\n";
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<"\n";return 1;}
}
