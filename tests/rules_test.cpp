#include "../Source/TPSDuel/DuelRules.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
    std::string Result;
    assert(DuelRules::ParseEndpoint("127.0.0.1", Result) && Result == "127.0.0.1:7777");
    assert(DuelRules::ParseEndpoint("192.168.001.100:65535", Result) && Result == "192.168.1.100:65535");
    for (const char* Bad : {"", "127.0.0", "256.1.2.3", "127.0.0.1:0", "127.0.0.1:65536", "127.0.0.1:",
         "127.0.0.1?listen", "127.0.0.1:7?duel=1", "127.0.0.1:7777/Map", "127.0.0.1:7777:8", "0.0.0.0", "224.0.0.1", "1.2.3.4 ", "1.2.3..4"})
        assert(!DuelRules::ParseEndpoint(Bad, Result) && Result.empty());
    assert(DuelRules::CanFire(true, true, false, 1, 10.1, 10.0, .1));
    assert(!DuelRules::CanFire(true, true, false, 30, 10.01, 10.0, .1));
    assert(!DuelRules::CanFire(false, true, false, 30, 11, 10, .1));
    assert(!DuelRules::CanFire(true, false, false, 30, 11, 10, .1));
    assert(!DuelRules::CanFire(true, true, true, 30, 11, 10, .1));
    assert(!DuelRules::CanFire(true, true, false, 0, 11, 10, .1));
    assert(!DuelRules::CanFire(true, true, false, 30, 11, 10, 0));
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    assert(!DuelRules::ValidAim(NaN, 0));
    assert(!DuelRules::ValidAim(0, NaN));
    assert(!DuelRules::ValidAim(81, 0));
    assert(!DuelRules::ValidAim(0, 1e30f));
    assert(DuelRules::ValidAim(-80, 360));
    float Pitch, Yaw;
    assert(DuelRules::DecodeAim(357, 183, Pitch, Yaw) && Pitch == -3 && Yaw == -177);
    assert(DuelRules::DecodeAim(279.995f, 360, Pitch, Yaw) && Pitch == -80 && Yaw == 0);
    assert(!DuelRules::DecodeAim(200, 0, Pitch, Yaw));
    assert(!DuelRules::DecodeAim(NaN, 0, Pitch, Yaw));
    assert(DuelRules::DamageResult(100, 25, true, true) == 100);
    assert(DuelRules::DamageResult(100, 25, false, false) == 100);
    assert(DuelRules::DamageResult(100, NaN, false, true) == 100);
    assert(DuelRules::DamageResult(100, -25, false, true) == 100);
    float Health = 100;
    for (int i = 0; i < 4; ++i) Health = DuelRules::DamageResult(Health, 25, false, true);
    assert(Health == 0);
    assert(DuelRules::DamageResult(Health, 25, false, true) == 0);
    assert(DuelRules::DamageResult(10, 1000, false, true) == 0);
    std::cout << "TPSDuel rules: all regression checks passed\n";
}
