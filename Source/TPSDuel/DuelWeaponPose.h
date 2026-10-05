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
    bool MagazineInHand = false;

    static FDuelWeaponPose Calculate(float Pitch, float Aim, float Reload, float Kick, float Carry=0.f)
    {
        FDuelWeaponPose Pose;
        Pose.CarryAlpha=Carry;
        const float Tilt = Reload >= 0.f ? FMath::Sin(PI * Reload) : 0.f;
        const FQuat Ready = FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Pitch * (1.f - Tilt) + Kick * 3.f - Tilt * 20.f))
            * FQuat(FVector::UpVector, FMath::DegreesToRadians(Tilt * 18.f));
        // Stock at the right waist, muzzle diagonally across the chest toward
        // the left shoulder. Both hands follow this same transform.
        const FQuat Running=FQuat(FVector::UpVector,FMath::DegreesToRadians(-55.f))
            * FQuat(FVector::ForwardVector,FMath::DegreesToRadians(35.f));
        const FQuat Rotation=FQuat::Slerp(Ready,Running,FMath::Clamp(Carry,0.f,1.f));
        const FVector Grip=FMath::Lerp(FMath::Lerp(FVector(-9,12,133),FVector(-7,13,142),Aim)+FVector(0,-Kick*2.f,-Tilt*12.f),FVector(-17,30,118),Carry);
        Pose.Gun = FTransform(Rotation, Grip);
        Pose.RightHand = Pose.Gun.TransformPosition(FVector(0,-4,1));
        const FVector Support = Pose.Gun.TransformPosition(FVector(6,30,9));
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
