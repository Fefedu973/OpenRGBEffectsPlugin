// SPDX-License-Identifier: GPL-2.0-or-later
#include "InferenceWorker.h"
#include "ModelGuard.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>
using namespace room_ai::inference;
std::atomic<unsigned> checks{0};
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool Wait(const std::function<bool()>& ready,double seconds=5)
{const double end=Now()+seconds;while(Now()<end){if(ready())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return ready();}
Request Video(std::uint64_t generation,std::uint64_t sequence,std::uint64_t epoch=1)
{
    Request r;r.generation=generation;r.epoch=epoch;r.sequence=sequence;r.source_time=Now();
    for(const auto& name:{"current_rgb","previous_rgb"}){Tensor t;t.name=name;t.shape={1,3,2,2};t.values.assign(12,.4f);r.inputs.push_back(std::move(t));}
    Tensor rect;rect.name="screen_rect";rect.shape={1,4};rect.values={.25f,.275f,.5f,.45f};r.inputs.push_back(std::move(rect));
    Tensor dt;dt.name="delta_seconds";dt.shape={1};dt.values={.2f};r.inputs.push_back(std::move(dt));return r;
}
float First(const std::shared_ptr<const Result>& r){return r->outputs.front().values.front();}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    try
    {
        Check(argc==3,"Usage: inference_tests runtime.dll fixtures");const std::string dll=argv[1];const QDir fixtures(QString::fromUtf8(argv[2]));
        auto Path=[&](const char* name){return fixtures.filePath(QString::fromUtf8(name)+"/manifest.json").toStdString();};
        auto c=ReadManifest(Path("video"));Check(c->task=="video-field-v1"&&c->states.size()==1&&c->inputs.size()==4,"manifest contract");
        Check(c->field_rect[0]==-.5&&c->field_rect[2]==2,"screen-relative field domain");
        auto audio_manifest=ReadManifest(Path("audio"))->manifest;
        const auto boundary_path=fixtures.filePath("audio-boundary.json");
        auto WriteBoundary=[&]{QFile f(boundary_path);Check(f.open(QIODevice::WriteOnly|QIODevice::Truncate),"write synthetic boundary fixture");const auto data=audio_manifest.dump();Check(f.write(data.data(),qint64(data.size()))==qint64(data.size()),"complete boundary fixture");};
        audio_manifest["inputs"][0]["shape"][2]=240000;WriteBoundary();Check(ReadManifest(boundary_path.toStdString())->inputs[0].shape[2]==240000,"five-second causal audio metadata accepted");
        audio_manifest["inputs"][0]["shape"][2]=240001;WriteBoundary();bool invalid=false;try{ReadManifest(boundary_path.toStdString());}catch(...){invalid=true;}Check(invalid,"audio window above five seconds refused");
        audio_manifest["inputs"][0]["shape"][2]=480.5;WriteBoundary();invalid=false;try{ReadManifest(boundary_path.toStdString());}catch(...){invalid=true;}Check(invalid,"fractional tensor dimensions refused");
        Worker w;Check(w.Status().state=="stopped"&&!w.Config()&&!w.Latest(),"constructing unused worker does not activate it");
        auto Load=[&](const char* name){const auto g=w.Configure(dll,Path(name));Check(Wait([&]{return bool(w.Config())||w.Status().state=="error";}),"load bounded");if(!w.Config())throw std::runtime_error("Load "+std::string(name)+": "+w.Status().detail);return g;};
        auto g=Load("video");Check(w.Status().runtime_version=="1.30.0","real official runtime loaded");
        Check(w.Submit(Video(g,1)),"submit first");Check(Wait([&]{return bool(w.Latest());}),"real ORT Run returns");auto first=w.Latest();
        Check(std::abs(First(first)-.2f)<1e-6f&&first->outputs[1].values==std::vector<float>(4,1),"actual graph arithmetic/confidence");
        Check(first->Usable(Now())&&first->generation==g&&first->epoch==1,"stamps and lease");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));Check(w.Submit(Video(g,2)),"submit recurrent step");
        Check(Wait([&]{return w.Latest()&&w.Latest()->sequence==2;}),"second actual run");Check(std::abs(First(w.Latest())-.22f)<1e-6f,"named recurrent state is fed back");
        Check(std::abs(First(first)-.2f)<1e-6f,"old result remains immutable");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));auto reset=Video(g,3);reset.reset=true;Check(w.Submit(reset),"explicit reset");Check(Wait([&]{const auto r=w.Latest();return r&&r->sequence==3;}),"reset run");Check(std::abs(First(w.Latest())-.2f)<1e-6f,"reset zeroes recurrent state");
        Check(!w.Submit(reset),"duplicate sequence rejected");auto bad=Video(g,4);bad.inputs[0].shape[2]=3;Check(!w.Submit(bad),"bad input dimensions");
        bad=Video(g,4);bad.inputs[0].values[0]=std::numeric_limits<float>::quiet_NaN();Check(!w.Submit(bad),"NaN input rejected");
        bad=Video(g,4);bad.inputs[0].values[0]=1.1f;Check(!w.Submit(bad),"encoded or out-of-range input rejected");
        bad=Video(g,4);bad.source_time=Now()+1;Check(!w.Submit(bad),"future timestamp rejected");bad.source_time=Now()-2;Check(!w.Submit(bad),"stale input rejected");
        bad=Video(g,4);bad.inputs.push_back(bad.inputs.front());Check(!w.Submit(bad),"extra input rejected");
        auto epoch=Video(g,1,2);Check(w.Submit(epoch),"new epoch");Check(Wait([&]{const auto r=w.Latest();return r&&r->epoch==2;}),"epoch run");Check(std::abs(First(w.Latest())-.2f)<1e-6f,"epoch resets memory");Check(!w.Submit(Video(g,100,1)),"old epoch rejected");
        std::atomic<bool> prepared_called=false,off_main=false;const auto main_thread=std::this_thread::get_id();
        Check(w.SubmitPrepared(g,[&](const ModelConfig& cfg)->std::optional<Request>{prepared_called=true;off_main=std::this_thread::get_id()!=main_thread;Check(cfg.task=="video-field-v1","prepared gets immutable contract");return std::nullopt;}),"prepared accepted");
        Check(Wait([&]{return w.Status().state=="waiting-input";}),"empty preparation exposes waiting status");Check(prepared_called&&off_main,"preparation outside render thread");
        Check(w.SubmitPrepared(g,[g](const ModelConfig&)->std::optional<Request>{return Video(g,2,2);}),"prepared real request");Check(Wait([&]{const auto r=w.Latest();return r&&r->epoch==2&&r->sequence==2;}),"prepared gets real inference");
        // One bounded callback in flight; repeated offers replace the single mailbox.
        std::mutex latch_mutex;std::condition_variable latch_cv;bool release=false;std::atomic<bool> entered=false;std::atomic<int> chosen=0;
        Check(w.SubmitPrepared(g,[&](const ModelConfig&)->std::optional<Request>{entered=true;std::unique_lock<std::mutex> lock(latch_mutex);latch_cv.wait_for(lock,std::chrono::seconds(2),[&]{return release;});return std::nullopt;}),"latch accepted");Check(Wait([&]{return entered.load();}),"preparation entered");
        for(int i=1;i<=20;++i)Check(w.SubmitPrepared(g,[&,i](const ModelConfig&)->std::optional<Request>{chosen=i;return std::nullopt;}),"mailbox replacement accepted");
        {std::lock_guard<std::mutex> lock(latch_mutex);release=true;}latch_cv.notify_all();Check(Wait([&]{return chosen.load()==20;}),"only latest pending callback executes");Check(w.Status().replaced>=19,"replacement metric");
        entered=false;release=false;
        Check(w.SubmitPrepared(g,[&](const ModelConfig&)->std::optional<Request>{entered=true;std::unique_lock<std::mutex> lock(latch_mutex);latch_cv.wait_for(lock,std::chrono::seconds(2),[&]{return release;});return std::nullopt;}),"reset replacement latch");Check(Wait([&]{return entered.load();}),"reset latch entered");
        auto pending_reset=Video(g,3,2);pending_reset.reset=true;Check(w.Submit(pending_reset),"reset pending accepted");Check(!w.Latest(),"reset invalidates latest immediately");std::this_thread::sleep_for(std::chrono::milliseconds(2));Check(w.Submit(Video(g,4,2)),"newest packet replaces reset packet");
        {std::lock_guard<std::mutex> lock(latch_mutex);release=true;}latch_cv.notify_all();Check(Wait([&]{return w.Latest()&&w.Latest()->sequence==4;}),"replacement after reset executes");Check(std::abs(First(w.Latest())-.2f)<1e-6f,"reset survives pending replacement");
        // Real loading failures, no alternate backend or silently substituted model.
        for(const char* name:{"bad_hash","bad_shape","external","custom","truncated"})
        {w.Configure(dll,Path(name));Check(Wait([&]{return w.Status().state=="error";}),"invalid package fails");Check(!w.Config()&&!w.Latest(),"invalid load clears old result");}
        g=Load("nan_output");auto rejected=w.Status().rejected;Check(w.Submit(Video(g,1)),"bad-output graph runs");Check(Wait([&]{return w.Status().rejected>rejected;}),"NaN output rejected");Check(!w.Latest(),"NaN not published");
        g=Load("audio");Request pcm;pcm.generation=g;pcm.epoch=1;pcm.sequence=1;pcm.source_time=Now();Tensor samples;samples.name="audio_pcm";samples.shape={1,1,480};samples.values.assign(480,-.25f);pcm.inputs.push_back(samples);
        Check(w.Submit(pcm),"causal PCM accepted");Check(Wait([&]{return bool(w.Latest());}),"audio real graph");Check(std::abs(First(w.Latest())-.25f)<1e-6f,"audio mean absolute amplitude native inference");
        g=Load("stale");const auto stale=w.Status().stale;Check(w.Submit(Video(g,1)),"slow stale graph accepted");Check(Wait([&]{return w.Status().stale>stale;},10),"completed stale computation discarded");Check(!w.Latest(),"stale computation never visible");
        g=Load("slow");Check(w.Submit(Video(g,1)),"slow graph submitted");Check(Wait([&]{return w.Status().state=="running";}),"slow Run entered");const double stop=Now();w.RequestStop();Check(Now()-stop<.1,"RequestStop does not join or wait for a kernel");Check(!w.Config()&&!w.Latest()&&w.Status().state=="stopped","stop invalidates output immediately");
        g=Load("video");Check(w.Submit(Video(g,1)),"reload after cancellation");Check(Wait([&]{return bool(w.Latest());}),"reload returns result");
        Check(!w.Submit(Video(g-1,2)),"old generation rejected");Check(w.Latest()->generation==g,"only current package result visible");
        // Cancel during preparation; its later result must not start a stale Run.
        entered=false;release=false;const auto complete=w.Status().completed;
        Check(w.SubmitPrepared(g,[&](const ModelConfig&)->std::optional<Request>{entered=true;std::unique_lock<std::mutex> lock(latch_mutex);latch_cv.wait_for(lock,std::chrono::seconds(2),[&]{return release;});return Video(g,2);}),"late preparation submitted");
        Check(Wait([&]{return entered.load();}),"late preparation entered");w.RequestStop();{std::lock_guard<std::mutex> lock(latch_mutex);release=true;}latch_cv.notify_all();std::this_thread::sleep_for(std::chrono::milliseconds(30));Check(!w.Latest()&&w.Status().completed==complete,"late preparation after stop cannot publish or run");
        std::cout<<"PASS "<<checks.load()<<" inference checks; real ONNX Runtime "<<w.Status().runtime_version<<" CPU; no capture/hardware/trained models\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL after "<<checks.load()<<": "<<e.what()<<"\n";return 1;}
}
