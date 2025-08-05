#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Protocol
{
namespace ChannelPayload
{

struct ChannelButton
{
    bool enabled = false;
    bool acitve = false;
};

struct TranslationTargetButton
{
    bool enabled = false;
    bool selected = false;
};

struct Request
{
    std::filesystem::path newInstallPath = "";
    TranslationTargetButton buttonEng;
    TranslationTargetButton buttonDe;
    TranslationTargetButton buttonDeFull;

    nlohmann::json to_json () const
    {
        nlohmann::json j;
        j["newInstallPath"] = newInstallPath.string();
        j["buttonEng"] = {{"enabled", buttonEng.enabled}, {"selected", buttonEng.selected}};
        j["buttonDe"] = {{"enabled", buttonDe.enabled}, {"selected", buttonDe.selected}};
        j["buttonDeFull"] = {{"enabled", buttonDeFull.enabled}, {"selected", buttonDeFull.selected}};
        return j;
    }

    void from_json (const nlohmann::json& j)
    {
        newInstallPath = j.at("newInstallPath").get<std::filesystem::path>();
        buttonEng.enabled = j.at("buttonEng").at("enabled").get<bool>();
        buttonEng.selected = j.at("buttonEng").at("selected").get<bool>();
        buttonDe.enabled = j.at("buttonDe").at("enabled").get<bool>();
        buttonDe.selected = j.at("buttonDe").at("selected").get<bool>();
        buttonDeFull.enabled = j.at("buttonDeFull").at("enabled").get<bool>();
        buttonDeFull.selected = j.at("buttonDeFull").at("selected").get<bool>();
    }
};

struct Response
{
    bool installPathIsSet = false;
    ChannelButton buttonChannel;
    TranslationTargetButton buttonEng;
    TranslationTargetButton buttonDe;
    TranslationTargetButton buttonDeFull;

    nlohmann::json to_json () const
    {
        nlohmann::json j;
        j["installPathIsSet"] = installPathIsSet;
        j["buttonChannel"] = {{"enabled", buttonChannel.enabled}, {"acitve", buttonChannel.acitve}};
        j["buttonEng"] = {{"enabled", buttonEng.enabled}, {"selected", buttonEng.selected}};
        j["buttonDe"] = {{"enabled", buttonDe.enabled}, {"selected", buttonDe.selected}};
        j["buttonDeFull"] = {{"enabled", buttonDeFull.enabled}, {"selected", buttonDeFull.selected}};
        return j;
    }

    void from_json (const nlohmann::json& j)
    {
        installPathIsSet = j.at("installPathIsSet").get<bool>();
        buttonChannel.enabled = j.at("buttonChannel").at("enabled").get<bool>();
        buttonChannel.acitve = j.at("buttonChannel").at("acitve").get<bool>();
        buttonEng.enabled = j.at("buttonEng").at("enabled").get<bool>();
        buttonEng.selected = j.at("buttonEng").at("selected").get<bool>();
        buttonDe.enabled = j.at("buttonDe").at("enabled").get<bool>();
        buttonDe.selected = j.at("buttonDe").at("selected").get<bool>();
        buttonDeFull.enabled = j.at("buttonDeFull").at("enabled").get<bool>();
        buttonDeFull.selected = j.at("buttonDeFull").at("selected").get<bool>();
    }
};

}  // namespace ChannelPayload
}  // namespace Protocol
