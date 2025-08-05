#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace Protocol
{

enum class PayloadType
{
    ChannelLive,
    ChannelPTU,
    ChannelEPTU,
    ChannelHotfix,
    ChannelTechPeview,
    Settings,
    UpdateButton
};

inline std::string toString (PayloadType type)
{
    switch(type)
    {
        case PayloadType::ChannelLive:
            return "LIVE";
        case PayloadType::ChannelPTU:
            return "PTU";
        case PayloadType::ChannelEPTU:
            return "EPTU";
        case PayloadType::ChannelHotfix:
            return "HOTFIX";
        case PayloadType::ChannelTechPeview:
            return "TECH-PREVIEW";
        case PayloadType::Settings:
            return "Settings";
        case PayloadType::UpdateButton:
            return "UpdateButton";
    }
    return "unknown";
}

inline PayloadType payloadTypeFromString (const std::string& typeStr)
{
    if(typeStr == "LIVE") return PayloadType::ChannelLive;
    if(typeStr == "PTU") return PayloadType::ChannelPTU;
    if(typeStr == "EPTU") return PayloadType::ChannelEPTU;
    if(typeStr == "HOTFIX") return PayloadType::ChannelHotfix;
    if(typeStr == "TECH-PREVIEW") return PayloadType::ChannelTechPeview;
    if(typeStr == "Settings") return PayloadType::Settings;
    if(typeStr == "UpdateButton") return PayloadType::UpdateButton;
    throw std::runtime_error("Unknown message type: " + typeStr);
}

struct MessageBase
{
    PayloadType type;
    nlohmann::json payload;

    nlohmann::json toJson () const { return nlohmann::json{{"type", toString(type)}, {"payload", payload}}; }

    void fromJson (const nlohmann::json& j)
    {
        if(!j.contains("type") || !j.contains("payload")) throw std::runtime_error("Missing fields in message");
        type = payloadTypeFromString(j.at("type").get<std::string>());
        payload = j.at("payload");
    }
};

struct Request : public MessageBase
{
};
struct Response : public MessageBase
{
};

}  // namespace Protocol
