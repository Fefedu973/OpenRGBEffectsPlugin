// SPDX-License-Identifier: GPL-2.0-or-later
// Included by AudioManager.cpp only. All WASAPI capture interfaces belong to
// their MTA worker; GUI registration never opens an audio stream.
#include "Audio/AudioPcm.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <wrl/client.h>
#include <mmreg.h>
#include <ksmedia.h>

namespace
{
using Microsoft::WRL::ComPtr;
double AudioNowSeconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
double AudioPacketSeconds(UINT64 qpc_100ns, bool timestamp_error, unsigned frames,
                          unsigned rate, double now_seconds)
{
    // GetBuffer already converts QPC to 100 ns units (not raw counter ticks).
    return timestamp_error ? now_seconds-double(frames)/rate : double(qpc_100ns)/10000000.0;
}
struct AudioEndpoint { std::wstring id; bool capture=false, default_output=false; };
struct AudioSession
{
    std::atomic<bool> stop{false};
    std::mutex mutex;
    std::condition_variable wake;
    std::array<float,512> buffer{};
    // Only the capture worker touches the tracker. Readers copy the immutable
    // snapshot under mutex; no DSP or COM calls hold the publication lock.
    room_audio::RhythmTracker rhythm;
    room_audio::RhythmSnapshot rhythm_snapshot=rhythm.Snapshot();
    std::thread worker;
    void Silence()
    {
        rhythm.Reset(AudioNowSeconds());
        const auto snapshot=rhythm.Snapshot();
        std::lock_guard<std::mutex> lock(mutex);
        buffer.fill(0.f); rhythm_snapshot=snapshot;
    }
    void PublishNoPacket()
    {
        // Missing PCM is not a measured beat. Suppress the published grid at
        // once, while allowing the analysis owner a short idle grace period.
        std::lock_guard<std::mutex> lock(mutex);
        buffer.fill(0.f);
        rhythm_snapshot.silent=true; rhythm_snapshot.locked=false;
        rhythm_snapshot.onset_strength=0; rhythm_snapshot.band_flux.fill(0);
        rhythm_snapshot.spectrum.fill(0); rhythm_snapshot.power=0;
        rhythm_snapshot.confidence=rhythm_snapshot.phase=rhythm_snapshot.bpm=0;
    }
    bool PublishPacket(audio_pcm::Window& window,const float* mono,size_t frames,
                       unsigned rate,double first_sample,bool discontinuity)
    {
        if((frames && !mono) || !rate || !std::isfinite(first_sample)) { Silence(); return false; }
        if(rate>=8000 && rate<=192000)
        {
            if(!rhythm.Push(mono,frames,rate,first_sample,discontinuity)) { Silence(); return false; }
        }
        else rhythm.Reset(first_sample); // Unsupported DSP rate must not break legacy PCM consumers.
        if(discontinuity) window=audio_pcm::Window{};
        window.AppendMono(mono,frames);
        const auto next_buffer=window.Snapshot();
        const auto next_rhythm=rhythm.Snapshot();
        std::lock_guard<std::mutex> lock(mutex);
        buffer=next_buffer; rhythm_snapshot=next_rhythm;
        return true;
    }
    bool Wait(unsigned ms)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return wake.wait_for(lock,std::chrono::milliseconds(ms),[this]{return stop.load();});
    }
    void Stop()
    {
        { std::lock_guard<std::mutex> lock(mutex); stop.store(true); }
        wake.notify_all();
        if(worker.joinable()) worker.join();
    }
    ~AudioSession() { Stop(); }
};
struct ComScope
{
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~ComScope(){if(SUCCEEDED(result))CoUninitialize();}
    bool Valid()const{return SUCCEEDED(result)||result==RPC_E_CHANGED_MODE;}
};
std::string AudioUtf8(const wchar_t* value)
{
    int count=WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);
    if(count<=0)return {};
    std::string result(size_t(count),0);
    WideCharToMultiByte(CP_UTF8,0,value,-1,&result[0],count,nullptr,nullptr);
    result.pop_back();return result;
}
bool AudioFormat(const WAVEFORMATEX* wave, audio_pcm::Format& format)
{
    if(!wave || wave->nSamplesPerSec==0)return false;
    WORD tag=wave->wFormatTag;
    if(tag==WAVE_FORMAT_EXTENSIBLE)
    {
        if(wave->cbSize<sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX))return false;
        const auto* ext=reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wave);
        if(IsEqualGUID(ext->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))tag=WAVE_FORMAT_IEEE_FLOAT;
        else if(IsEqualGUID(ext->SubFormat,KSDATAFORMAT_SUBTYPE_PCM))tag=WAVE_FORMAT_PCM;
        else return false;
    }
    using E=audio_pcm::Encoding;
    if(tag==WAVE_FORMAT_IEEE_FLOAT && wave->wBitsPerSample==32)format.encoding=E::Float32;
    else if(tag==WAVE_FORMAT_IEEE_FLOAT && wave->wBitsPerSample==64)format.encoding=E::Float64;
    else if(tag==WAVE_FORMAT_PCM && wave->wBitsPerSample==8)format.encoding=E::Unsigned8;
    else if(tag==WAVE_FORMAT_PCM && wave->wBitsPerSample==16)format.encoding=E::Signed16;
    else if(tag==WAVE_FORMAT_PCM && wave->wBitsPerSample==24)format.encoding=E::Signed24;
    else if(tag==WAVE_FORMAT_PCM && wave->wBitsPerSample==32)format.encoding=E::Signed32;
    else return false;
    format.channels=wave->nChannels;format.block_align=wave->nBlockAlign;
    return audio_pcm::Valid(format);
}
HRESULT CaptureEndpoint(AudioSession& session, const AudioEndpoint& endpoint)
{
    session.Silence(); // Endpoint changes never inherit a previous stream's beat grid.
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
    if(FAILED(hr))return hr;
    ComPtr<IMMDevice> device;
    hr=endpoint.default_output ? enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device)
                               : enumerator->GetDevice(endpoint.id.c_str(),&device);
    if(FAILED(hr))return hr;
    LPWSTR active_id=nullptr;
    hr=device->GetId(&active_id);
    if(FAILED(hr))return hr;
    const std::wstring opened_id=active_id;
    CoTaskMemFree(active_id);
    ComPtr<IAudioClient> client;
    hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf()));
    if(FAILED(hr))return hr;
    WAVEFORMATEX* raw=nullptr;
    hr=client->GetMixFormat(&raw);
    if(FAILED(hr))return hr;
    std::unique_ptr<WAVEFORMATEX,decltype(&CoTaskMemFree)> wave(raw,&CoTaskMemFree);
    audio_pcm::Format format{};
    if(!AudioFormat(wave.get(),format))return AUDCLNT_E_UNSUPPORTED_FORMAT;
    const unsigned sample_rate=wave->nSamplesPerSec;
    hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,endpoint.capture?0:AUDCLNT_STREAMFLAGS_LOOPBACK,
                          0,0,wave.get(),nullptr);
    if(FAILED(hr))return hr;
    ComPtr<IAudioCaptureClient> capture;
    hr=client->GetService(IID_PPV_ARGS(&capture));
    if(FAILED(hr))return hr;
    hr=client->Start();
    if(FAILED(hr))return hr;
    audio_pcm::Window window;
    std::vector<float> mono;
    bool silence_published=false, silence_reset=false;
    auto last_packet=std::chrono::steady_clock::now();
    auto last_default_check=last_packet;
    while(!session.stop.load())
    {
        UINT32 available=0;
        hr=capture->GetNextPacketSize(&available);
        if(FAILED(hr))break;
        if(endpoint.default_output && std::chrono::steady_clock::now()-last_default_check>=std::chrono::seconds(1))
        {
            last_default_check=std::chrono::steady_clock::now();
            ComPtr<IMMDevice> selected;LPWSTR selected_id=nullptr;
            hr=enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&selected);
            if(SUCCEEDED(hr))hr=selected->GetId(&selected_id);
            const bool changed=FAILED(hr)||!selected_id||opened_id!=selected_id;
            CoTaskMemFree(selected_id);
            if(changed){hr=AUDCLNT_E_DEVICE_INVALIDATED;break;}
        }
        while(available && !session.stop.load())
        {
            BYTE* data=nullptr; UINT32 frames=0; DWORD flags=0; UINT64 packet_qpc=0;
            hr=capture->GetBuffer(&data,&frames,&flags,nullptr,&packet_qpc);
            if(FAILED(hr))break;
            if(hr==AUDCLNT_S_BUFFER_EMPTY){hr=S_OK;break;}
            const bool timestamp_error=(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)!=0;
            const double first_sample=AudioPacketSeconds(packet_qpc,timestamp_error,frames,sample_rate,AudioNowSeconds());
            bool valid=frames<=std::min<std::uint64_t>(384000,std::uint64_t(sample_rate)*2);
            if(valid)
            {
                try { mono.resize(frames); }
                catch(const std::bad_alloc&) { valid=false; }
            }
            if(valid) valid=audio_pcm::DecodeMono(data,frames,format,
                (flags&AUDCLNT_BUFFERFLAGS_SILENT)!=0,mono.data(),mono.size());
            const HRESULT released=capture->ReleaseBuffer(frames);
            if(!valid){hr=E_INVALIDARG;break;}
            if(FAILED(released)){hr=released;break;}
            // Analyze every sample only after releasing the WASAPI buffer.
            const bool discontinuity=timestamp_error||(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)!=0;
            if(!session.PublishPacket(window,mono.data(),mono.size(),sample_rate,first_sample,discontinuity))
            {hr=E_INVALIDARG;break;}
            silence_published=false;
            silence_reset=false;
            last_packet=std::chrono::steady_clock::now();
            hr=capture->GetNextPacketSize(&available);
            if(FAILED(hr))break;
        }
        if(FAILED(hr))break;
        // WASAPI loopback can provide no packet at all during silence.
        if(!silence_published && std::chrono::steady_clock::now()-last_packet>std::chrono::milliseconds(100))
        {
            session.PublishNoPacket();
            window=audio_pcm::Window{};
            silence_published=true;
        }
        if(!silence_reset && std::chrono::steady_clock::now()-last_packet>std::chrono::milliseconds(1100))
        {
            session.Silence();
            silence_reset=true;
        }
        if(session.Wait(5))break;
    }
    client->Stop();
    session.Silence();
    return hr;
}
void AudioWorker(const std::shared_ptr<AudioSession>& session, AudioEndpoint endpoint)
{
    ComScope com;
    if(!com.Valid()){LOG_WARNING("[Effects Audio] COM initialization failed: 0x%08lx",com.result);return;}
    HRESULT reported=S_OK;
    while(!session->stop.load())
    {
        const HRESULT result=CaptureEndpoint(*session,endpoint);
        session->Silence();
        if(session->stop.load())break;
        if(result!=reported){LOG_WARNING("[Effects Audio] Capture unavailable: 0x%08lx (retrying)",result);reported=result;}
        if(session->Wait(1000))break;
    }
}
}

struct AudioManager::WindowsState
{
    std::mutex mutex;
    std::vector<std::string> names;
    std::vector<AudioEndpoint> endpoints;
    std::map<int,std::set<void*>> clients;
    std::map<int,std::shared_ptr<AudioSession>> sessions;
};
AudioManager* AudioManager::get(){static AudioManager singleton;return &singleton;}
AudioManager::AudioManager():windows(new WindowsState)
{
    ComScope com;
    if(!com.Valid())return;
    ComPtr<IMMDeviceEnumerator> enumerator;
    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator))))return;
    for(EDataFlow flow:{eRender,eCapture})
    {
        ComPtr<IMMDeviceCollection> devices;
        if(FAILED(enumerator->EnumAudioEndpoints(flow,DEVICE_STATE_ACTIVE,&devices)))continue;
        UINT count=0;if(FAILED(devices->GetCount(&count)))continue;
        for(UINT i=0;i<count;++i)
        {
            ComPtr<IMMDevice> device;ComPtr<IPropertyStore> props;
            if(FAILED(devices->Item(i,&device))||FAILED(device->OpenPropertyStore(STGM_READ,&props)))continue;
            PROPVARIANT name;PropVariantInit(&name);LPWSTR id=nullptr;
            const HRESULT named=props->GetValue(PKEY_Device_FriendlyName,&name);
            const HRESULT identified=device->GetId(&id);
            if(SUCCEEDED(named)&&name.vt==VT_LPWSTR&&name.pwszVal&&SUCCEEDED(identified)&&id)
            {
                windows->names.push_back(AudioUtf8(name.pwszVal)+(flow==eRender?" (Loopback)":""));
                windows->endpoints.push_back({id,flow==eCapture,false});
            }
            CoTaskMemFree(id);PropVariantClear(&name);
        }
    }
    // Append rather than prepend: persisted physical endpoint indices retain
    // their existing ordering. The new entry resolves the system output on open.
    windows->names.push_back("System default output (Loopback)");
    windows->endpoints.push_back({L"",false,true});
}
AudioManager::~AudioManager()
{
    for(auto& item:windows->sessions)item.second->Stop();
}
std::vector<char*> AudioManager::GetAudioDevices()
{
    std::vector<char*> result;for(auto& name:windows->names)result.push_back(&name[0]);return result;
}
void AudioManager::Capture(int index,float* output)
{
    if(!output)return;
    std::fill_n(output,512,0.f);
    std::shared_ptr<AudioSession> session;
    {std::lock_guard<std::mutex> lock(windows->mutex);auto it=windows->sessions.find(index);if(it==windows->sessions.end())return;session=it->second;}
    std::lock_guard<std::mutex> lock(session->mutex);
    std::copy(session->buffer.begin(),session->buffer.end(),output);
}
room_audio::RhythmSnapshot AudioManager::CaptureRhythm(int index)
{
    std::shared_ptr<AudioSession> session;
    {std::lock_guard<std::mutex> lock(windows->mutex);auto it=windows->sessions.find(index);if(it==windows->sessions.end())return {};session=it->second;}
    std::lock_guard<std::mutex> lock(session->mutex);
    return session->rhythm_snapshot;
}
void AudioManager::RegisterClient(int index,void* client)
{
    std::lock_guard<std::mutex> lock(windows->mutex);
    const int endpoint_index = index==DEFAULT_OUTPUT_DEVICE ? int(windows->endpoints.size())-1 : index;
    if(endpoint_index<0||size_t(endpoint_index)>=windows->endpoints.size()||!client)return;
    windows->clients[index].insert(client);
    if(windows->sessions.count(index))return;
    auto session=std::make_shared<AudioSession>();
    windows->sessions[index]=session;
    session->worker=std::thread(AudioWorker,session,windows->endpoints[endpoint_index]);
}
void AudioManager::UnRegisterClient(int index,void* client)
{
    std::lock_guard<std::mutex> lock(windows->mutex);
    auto found=windows->clients.find(index);if(found==windows->clients.end())return;
    found->second.erase(client);if(!found->second.empty())return;
    windows->clients.erase(found);
    auto session=windows->sessions.find(index);
    if(session!=windows->sessions.end()){session->second->Stop();windows->sessions.erase(session);}
}
