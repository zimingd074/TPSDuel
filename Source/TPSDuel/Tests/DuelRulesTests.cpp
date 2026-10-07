#include "DuelRules.h"
#include "DuelWeaponPose.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelRulesTest, "TPSDuel.Rules.AuthorityAndAddress", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDuelRulesTest::RunTest(const FString&)
{
    std::string Address;
    TestTrue(TEXT("LAN address accepted"), DuelRules::ParseEndpoint("192.168.1.100:7777", Address));
    TestFalse(TEXT("URL injection rejected"), DuelRules::ParseEndpoint("127.0.0.1?listen", Address));
    TestFalse(TEXT("Rapid toggle cannot bypass cadence"), DuelRules::CanFire(true, true, false, 30, 1.01, 1, .1));
    TestFalse(TEXT("800 RPM cooldown rejects early shot"), DuelRules::CanFire(true,true,false,30,1.074,1,.075));
    TestTrue(TEXT("800 RPM shot is allowed at deadline"), DuelRules::CanFire(true,true,false,30,1.075,1,.075));
    const auto Carry=FDuelWeaponPose::Calculate(0.f,0.f,-1.f,0.f,1.f);
    TestTrue(TEXT("Moving low-ready muzzle points diagonally down"),Carry.Gun.GetRotation().GetAxisY().Z<-.4f);
    TestTrue(TEXT("Ready rifle returns to horizontal"),FMath::Abs(FDuelWeaponPose::Calculate(0.f,0.f,-1.f,0.f,0.f).Gun.GetRotation().GetAxisY().Z)<.01f);
    const FVector Stock(0,-22,10);
    const auto Level=FDuelWeaponPose::Calculate(0.f,1.f,-1.f,0.f);
    for (float Angle : {-60.f,60.f})
    {
        const FVector Contact=FDuelWeaponPose::Calculate(Angle,1.f,-1.f,0.f).Gun.TransformPosition(Stock);
        TestTrue(TEXT("Pitch preserves stock height and forward contact"),FMath::Abs(Contact.Y-Level.Gun.TransformPosition(Stock).Y)<.01f && FMath::Abs(Contact.Z-Level.Gun.TransformPosition(Stock).Z)<.01f);
        TestTrue(TEXT("Steep aim moves rifle outside torso"),Contact.X<Level.Gun.TransformPosition(Stock).X-10.f);
    }
    const auto Folded=FDuelWeaponPose::Calculate(0.f,1.f,-1.f,0.f,0.f,1.f);
    TestTrue(TEXT("Cover tuck preserves stock contact"),FVector::Dist(Level.Gun.TransformPosition(Stock),Folded.Gun.TransformPosition(Stock))<.01f);
    TestTrue(TEXT("Cover tuck reduces barrel forward reach"),Folded.Gun.TransformPosition(FVector(0,57,14)).Y<Level.Gun.TransformPosition(FVector(0,57,14)).Y*.5f);
    TestFalse(TEXT("Dead player cannot fire"), DuelRules::CanFire(true, false, false, 30, 2, 1, .1));
    TestFalse(TEXT("Reload blocks fire"), DuelRules::CanFire(true, true, true, 30, 2, 1, .1));
    TestEqual(TEXT("Spawn protection blocks damage"), DuelRules::DamageResult(100, 25, true, true), 100.f);
    TestEqual(TEXT("Finished match blocks damage"), DuelRules::DamageResult(100, 25, false, false), 100.f);
    TestEqual(TEXT("Damage clamps to zero"), DuelRules::DamageResult(10, 25, false, true), 0.f);
    float Pitch, Yaw;
    TestTrue(TEXT("Negative RPC pitch is decoded"), DuelRules::DecodeAim(357, 183, Pitch, Yaw));
    TestEqual(TEXT("Decoded pitch"), Pitch, -3.f);
    TestEqual(TEXT("Decoded yaw"), Yaw, -177.f);
    TestFalse(TEXT("Invalid wire pitch rejected"), DuelRules::DecodeAim(200, 0, Pitch, Yaw));
    return true;
}
#endif
