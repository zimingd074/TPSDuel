#pragma once
#include <cmath>
#include <string>

// Pure rules shared by the actual game and executable regression tests.
namespace DuelRules
{
    inline bool CanFire(bool Playing, bool Alive, bool Reloading, int Ammo,
                        double Now, double LastShot, double Interval)
    {
        return Playing && Alive && !Reloading && Ammo > 0 &&
            std::isfinite(Now) && std::isfinite(LastShot) && Interval > 0 &&
            Now + 0.0001 >= LastShot + Interval;
    }
    inline float DamageResult(float Health, float Damage, bool Protected, bool Playing)
    {
        if (!Playing || Protected || Health <= 0 || !std::isfinite(Damage) || Damage <= 0)
            return Health;
        return Damage >= Health ? 0.0f : Health - Damage;
    }
    inline bool ValidAim(float Pitch, float Yaw)
    {
        return std::isfinite(Pitch) && std::isfinite(Yaw) && Pitch >= -80 && Pitch <= 80 && std::fabs(Yaw) <= 360;
    }
    inline bool DecodeAim(float WirePitch, float WireYaw, float& Pitch, float& Yaw)
    {
        if (!std::isfinite(WirePitch) || !std::isfinite(WireYaw) ||
            std::fabs(WirePitch) > 360 || std::fabs(WireYaw) > 360) return false;
        Pitch = std::remainder(WirePitch, 360.f);
        Yaw = std::remainder(WireYaw, 360.f);
        // UE serializes negative rotator axes as 0..360, with short quantization.
        if (Pitch < -80.01f || Pitch > 80.01f) return false;
        if (Pitch < -80.f) Pitch = -80.f;
        if (Pitch > 80.f) Pitch = 80.f;
        return ValidAim(Pitch, Yaw);
    }
    inline bool ParseEndpoint(const std::string& Input, std::string& Endpoint)
    {
        Endpoint.clear();
        if (Input.empty() || Input.size() > 21) return false;
        const auto Colon = Input.find(':');
        const std::string IP = Input.substr(0, Colon);
        unsigned Octets[4] = {};
        size_t Pos = 0;
        for (int Part = 0; Part < 4; ++Part)
        {
            int Digits = 0;
            while (Pos < IP.size() && IP[Pos] >= '0' && IP[Pos] <= '9')
            {
                Octets[Part] = Octets[Part] * 10 + static_cast<unsigned>(IP[Pos++] - '0');
                if (++Digits > 3 || Octets[Part] > 255) return false;
            }
            if (Digits == 0) return false;
            if (Part < 3 && (Pos >= IP.size() || IP[Pos++] != '.')) return false;
        }
        if (Pos != IP.size() || Octets[0] == 0 || Octets[0] >= 224 ||
            (Octets[0] == 255 && Octets[1] == 255 && Octets[2] == 255 && Octets[3] == 255)) return false;
        unsigned Port = 7777;
        if (Colon != std::string::npos)
        {
            const std::string PortText = Input.substr(Colon + 1);
            if (PortText.empty() || PortText.size() > 5) return false;
            Port = 0;
            for (char C : PortText)
            {
                if (C < '0' || C > '9') return false;
                Port = Port * 10 + static_cast<unsigned>(C - '0');
            }
            if (Port == 0 || Port > 65535) return false;
        }
        Endpoint = std::to_string(Octets[0]) + "." + std::to_string(Octets[1]) + "." +
            std::to_string(Octets[2]) + "." + std::to_string(Octets[3]) + ":" + std::to_string(Port);
        return true;
    }
}
