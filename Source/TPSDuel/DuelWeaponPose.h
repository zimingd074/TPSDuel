#pragma once
#include "CoreMinimal.h"

// All targets are in character mesh space (forward +Y). Animation and weapon use
// the same transform, so neither hand follows a socket from the preceding frame.
struct FDuelWeaponPose
{
    FTransform Gun = FTransform::Identity;
    FVector RightHand = FVector::ZeroVector;
    FVector LeftHand = FVector::ZeroVector;
    FVector Magazine = FVector::ZeroVector;
    float CarryAlpha = 0.f;
    float ObstructionAlpha = 0.f;
    bool MagazineInHand = false;

    static FDuelWeaponPose Calculate(float Pitch, float Aim, float Reload, float Kick, float Carry=0.f, float Obstruction=0.f)
    {
        FDuelWeaponPose Pose;
        Pose.CarryAlpha=Carry;
        const float Tilt = Reload >= 0.f ? FMath::Sin(PI * Reload) : 0.f;
        const FQuat Ready = FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Pitch * (1.f - Tilt) + Kick * 3.f - Tilt * 20.f))
            * FQuat(FVector::UpVector, FMath::DegreesToRadians(Tilt * 18.f));
        // Low ready while moving: stock near the chest, muzzle diagonally down.
        // Both hands follow this transform; aim/fire/reload leave this stance.
        const FQuat Running=FQuat(FVector::UpVector,FMath::DegreesToRadians(-40.f))
            * FQuat(FVector::ForwardVector,FMath::DegreesToRadians(-32.f));
        const FQuat FreeRotation=FQuat::Slerp(Ready,Running,FMath::Clamp(Carry,0.f,1.f));
        // Fold beside the right shoulder when the barrel meets cover.
        Pose.ObstructionAlpha=FMath::Clamp(Obstruction,0.f,1.f);
        const FQuat Folded=FQuat(FVector::UpVector,FMath::DegreesToRadians(80.f))
            * FQuat(FVector::ForwardVector,FMath::DegreesToRadians(-25.f));
        const FQuat Rotation=FQuat::Slerp(FreeRotation,Folded,Pose.ObstructionAlpha);
        FVector Grip=FMath::Lerp(FMath::Lerp(FVector(-9,12,133),FVector(-7,13,142),Aim)+FVector(0,-Kick*2.f,-Tilt*12.f),FVector(-13,23,132),Carry);
        // Keep steep up/down aim outside the chest and face instead of rotating
        // the barrel down the middle of the body.
        Grip.X-=16.f*FMath::SmoothStep(15.f,65.f,FMath::Abs(Pitch))*(1.f-Carry);
        // Rotate about the stock contact instead of driving the stock through
        // the chest/head as the player looks up or down.
        const FVector Stock(0,-22,10);
        const FVector FreeGrip=Grip+Stock-FreeRotation.RotateVector(Stock);
        const FVector StockContact=FreeGrip+FreeRotation.RotateVector(Stock);
        Pose.Gun = FTransform(Rotation,StockContact-Rotation.RotateVector(Stock));
        Pose.RightHand = Pose.Gun.TransformPosition(FVector(0,-4,1));
        const float SupportY=30.f-6.f*FMath::SmoothStep(25.f,70.f,FMath::Abs(Pitch));
        const FVector Support = Pose.Gun.TransformPosition(FVector(6,SupportY,9));
        const FVector Well = Pose.Gun.TransformPosition(FVector(0,17,2));
        const FVector Belt(14,4,101);
        Pose.LeftHand = Support;
        Pose.Magazine = Well;
        if (Reload >= 0.f)
        {
            // Reach magazine, extract, move to belt, insert and return to forend.
            const float Times[] = {0.f,.15f,.30f,.48f,.62f,.82f,1.f};
            const FVector Points[] = {Support,Well,Well+FVector(0,0,-17),Belt,Belt,Well,Support};
            for (int32 Index=1; Index<7; ++Index)
                if (Reload <= Times[Index])
                {
                    float Alpha=(Reload-Times[Index-1])/(Times[Index]-Times[Index-1]);
                    Alpha=FMath::SmoothStep(0.f,1.f,Alpha);
                    Pose.LeftHand=FMath::Lerp(Points[Index-1],Points[Index],Alpha);
                    break;
                }
            Pose.MagazineInHand = Reload >= .15f && Reload < .82f;
            if (Pose.MagazineInHand) Pose.Magazine=Pose.LeftHand;
        }
        return Pose;
    }
};
