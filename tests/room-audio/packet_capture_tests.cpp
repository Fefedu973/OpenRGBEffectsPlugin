// SPDX-License-Identifier: GPL-2.0-or-later
// Production WASAPI packet publication, synthetic PCM only. No device opened.
#include <cstdio>
#define LOG_WARNING(...) ((void)0)
#include "Audio/AudioManager.h"
#include "Audio/AudioManagerWin.h"
#include <iostream>
#include <stdexcept>
#include <vector>

static unsigned checks=0;
static void Check(bool value,const char* why)
{ ++checks; if(!value)throw std::runtime_error(why); }

static void Fragmented(unsigned rate,unsigned channels)
{
    auto session=std::make_unique<AudioSession>();
    auto reference=std::make_unique<room_audio::RhythmTracker>();
    audio_pcm::Window window;
    const size_t count=size_t(rate)*3;
    std::vector<float> expected(count),pcm(count*channels);
    for(size_t i=0;i<count;++i)
    {
        // Identical channels make the expected downmix independent of encoding.
        const double t=double(i)/rate;
        expected[i]=float(std::sin(2*3.141592653589793*180*t)*
            ((i%(rate/2))<rate/40?0.7:0.015));
        for(unsigned c=0;c<channels;++c)pcm[i*channels+c]=expected[i];
    }
    for(size_t i=0;i<count;i+=rate)
        Check(reference->Push(expected.data()+i,rate,rate,20.0+double(i)/rate),"reference accepts one-second PCM");
    const audio_pcm::Format format{audio_pcm::Encoding::Float32,channels,channels*4};
    const size_t sizes[]={127,960,1,2048,511,4096,73};
    size_t offset=0,packet=0;
    while(offset<count)
    {
        const size_t frames=std::min(sizes[packet++%7],count-offset);
        std::vector<float> mono(frames);
        Check(audio_pcm::DecodeMono(reinterpret_cast<const uint8_t*>(pcm.data()+offset*channels),
            frames,format,false,mono.data(),mono.size()),"entire multichannel packet decoded");
        Check(std::abs(mono.front()-expected[offset])<1e-6 &&
              std::abs(mono.back()-expected[offset+frames-1])<1e-6,"first and last samples retained");
        Check(session->PublishPacket(window,mono.data(),frames,rate,20.0+double(offset)/rate,false),
              "fragmented packet publishes through actual capture path");
        offset+=frames;
    }
    const auto actual=session->rhythm_snapshot, direct=reference->Snapshot();
    Check(actual.sequence==direct.sequence && actual.sequence>250,"all PCM, not only the last 512 samples, reaches DSP");
    Check(actual.onset_sequence==direct.onset_sequence,"packet fragmentation preserves onset events");
    Check(std::abs(actual.audio_time-direct.audio_time)<1e-9,"audio timeline independent of packet size");
    for(unsigned b=0;b<3;++b)
        Check(std::abs(actual.band_flux[b]-direct.band_flux[b])<1e-5,"multichannel fragment spectral flux matches full mono stream");
    for(size_t i=0;i<512;++i)
        Check(std::abs(session->buffer[i]-expected[count-512+i])<1e-6,"legacy latest512 window unchanged");
    // A reset/reconnect publishes empty state immediately, without stale beats.
    const auto generation=actual.generation;
    session->PublishNoPacket();
    Check(session->rhythm_snapshot.generation==generation && !session->rhythm_snapshot.locked &&
          session->rhythm_snapshot.silent && session->rhythm_snapshot.onset_strength==0 &&
          session->rhythm_snapshot.bpm==0,"missing PCM suppresses published prediction before analysis reset");
    Check(session->rhythm.Snapshot().sequence==actual.sequence,"idle publication does not mutate capture-owned history");
    session->Silence();
    Check(session->rhythm_snapshot.generation>generation && !session->rhythm_snapshot.locked &&
          session->rhythm_snapshot.silent && session->rhythm_snapshot.onset_strength==0,
          "silence/reset clears previous connection tempo");
    Check(std::all_of(session->buffer.begin(),session->buffer.end(),[](float v){return v==0;}),
          "reset publishes zeros for legacy consumers");
    float sample=0.2f;
    Check(session->PublishPacket(window,&sample,1,rate,200.0,true),"first reconnect packet accepted");
    Check(session->rhythm_snapshot.generation>generation && !session->rhythm_snapshot.locked,
          "discontinuity does not inherit locked beat prediction");
    Check(!session->PublishPacket(window,nullptr,1,rate,200.0,false),"invalid packet fails and clears state");
}

int main()
{
    try
    {
        Fragmented(44100,2); Fragmented(48000,2); Fragmented(48000,8);
        Check(std::abs(AudioPacketSeconds(12345678900ULL,false,960,48000,900.0)-1234.56789)<1e-9,
              "WASAPI timestamp uses 100ns units, not raw QPC ticks");
        Check(std::abs(AudioPacketSeconds(0,true,960,48000,100.0)-99.98)<1e-9,
              "timestamp-error fallback identifies first sample, not packet receipt");
        LARGE_INTEGER counter{},frequency{};
        Check(QueryPerformanceFrequency(&frequency) && QueryPerformanceCounter(&counter),"Windows QPC clock available");
        Check(std::abs(AudioNowSeconds()-double(counter.QuadPart)/double(frequency.QuadPart))<0.050,
              "capture QPC and renderer steady clock share their seconds epoch");
        const audio_pcm::Format format{audio_pcm::Encoding::Float32,8,32};
        float mono[4]={1,1,1,1};
        Check(audio_pcm::DecodeMono(nullptr,4,format,true,mono,4),"SILENT packet does not require data pointer");
        Check(std::all_of(mono,mono+4,[](float v){return v==0;}),"all silent samples decoded");
        Check(!audio_pcm::DecodeMono(nullptr,4,format,false,mono,4),"null non-silent packet rejected");
        Check(!audio_pcm::DecodeMono(nullptr,5,format,true,mono,4),"output bounds checked before decode");
        auto high_rate=std::make_unique<AudioSession>();audio_pcm::Window window;
        const float legacy_pcm[]={0.1f,0.2f,0.3f};
        Check(high_rate->PublishPacket(window,legacy_pcm,3,384000,10.0,false),"unsupported rhythm rate preserves legacy capture");
        Check(high_rate->buffer.back()==0.3f && !high_rate->rhythm_snapshot.locked && high_rate->rhythm_snapshot.silent,
              "384kHz legacy window remains available with empty rhythm snapshot");
        std::cout<<"PASS "<<checks<<" complete-packet/timestamp/reset assertions; no audio endpoint opened\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<checks<<": "<<e.what()<<'\n';return 1;}
}
