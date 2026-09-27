// SPDX-License-Identifier: GPL-2.0-or-later
#include "TapHistory.h"
#include "PhysicalKeys.h"
#include "KeyboardDeduplication.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(#x);}while(0)
int main()
{
    native_taps::History history;
    history.Add(10,20,0,30);history.Advance(1,30);
    CHECK(history.Events().size()==1);CHECK(history.Events()[0].travel==30);
    const auto seed=history.Events()[0].seed;
    history.Advance(1,0);CHECK(history.Events()[0].travel==30);CHECK(history.Events()[0].age==2);
    history.Advance(.5,100);CHECK(history.Events()[0].travel==80);CHECK(history.Events()[0].seed==seed);
    history.Add(30,40,.1,10);CHECK(history.Events()[1].travel==1);
    history.Add(30,40,.3,10);CHECK(history.Events().size()==2);
    history.Add(-1,0,0,1);history.Add(1,1,std::numeric_limits<double>::quiet_NaN(),1);
    CHECK(history.Events().size()==2);
    history.Advance(10,1);CHECK(history.Events().empty());
    for(unsigned i=0;i<1000;++i)history.Add(i%320,i%200,0,0);
    CHECK(history.Events().size()==64);history.Clear();CHECK(history.Events().empty());
    CHECK(native_taps::PhysicalScan("Key: Q")==0x10); // physical AZERTY A
    CHECK(native_taps::PhysicalScan("Key: A")==0x1e); // physical AZERTY Q
    CHECK(native_taps::PhysicalScan("Key: W")==0x11);
    CHECK(native_taps::PhysicalScan("Key: Z")==0x2c);
    CHECK(native_taps::PhysicalScan("Key: /")==0x35); // physical AZERTY !
    CHECK(native_taps::PhysicalScan("Key: \\ (ISO)")==0x56); // ISO <>
    CHECK(native_taps::PhysicalScan("Key: #")==0x2b); // ISO *
    CHECK(native_taps::PhysicalScan("Key: Enter (ISO)")==0x1c);
    CHECK(native_taps::PhysicalScan("Key: Number Pad Enter")==0xe01c);
    CHECK(native_taps::PhysicalScan("Key: Right Control")==0xe01d);
    CHECK(native_taps::PhysicalScan("Key: Right Alt")==0xe038);
    CHECK(native_taps::PhysicalScan("Key: Right Fn")==0);
    CHECK(native_taps::PhysicalScan("Not a keyboard")==0);
    native_taps::KeyboardDeduplication dedup;
    CHECK(dedup.Accept("first keyboard","boot",0x10,1));
    CHECK(!dedup.Accept("first keyboard","nkro",0x10,1.004));
    CHECK(dedup.Accept("second keyboard","boot",0x10,1.004));
    CHECK(dedup.Accept("first keyboard","boot",0x11,1.004));
    CHECK(dedup.Accept("first keyboard","boot",0x10,1.005));
    CHECK(dedup.Accept("first keyboard","nkro",0x10,1.1));
    dedup.Clear();CHECK(dedup.Accept("first keyboard","boot",0x10,1.1));
    std::cout<<checks<<" tap and physical-position assertions passed\n";
}
