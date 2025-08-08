#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Protocol
{

struct ChannelButton
{
    std::optional<bool> enabled;
    std::optional<bool> active;
};

struct TranslationTargetButton
{
    std::optional<bool> enabled;
    std::optional<bool> selected;
};

struct ChannelPayload
{
    std::optional<bool> installPathIsSet;
    std::optional<std::filesystem::path> newInstallPath;
    ChannelButton buttonChannel;
    TranslationTargetButton buttonEng;
    TranslationTargetButton buttonDe;
    TranslationTargetButton buttonDeFull;

    ChannelPayload () = default;

    ChannelPayload (const nlohmann::json& j)
    {
        if(j.contains("installPathIsSet")) installPathIsSet = j.at("installPathIsSet").get<bool>();
        if(j.contains("newInstallPath")) newInstallPath = j.at("newInstallPath").get<std::string>();

        if(j.contains("buttonChannel"))
        {
            const auto& channel = j.at("buttonChannel");
            if(channel.contains("enabled")) buttonChannel.enabled = channel.at("enabled").get<bool>();
            if(channel.contains("active")) buttonChannel.active = channel.at("active").get<bool>();
        }
        if(j.contains("buttonEng"))
        {
            const auto& eng = j.at("buttonEng");
            if(eng.contains("enabled")) buttonEng.enabled = eng.at("enabled").get<bool>();
            if(eng.contains("selected")) buttonEng.selected = eng.at("selected").get<bool>();
        }
        if(j.contains("buttonDe"))
        {
            const auto& de = j.at("buttonDe");
            if(de.contains("enabled")) buttonDe.enabled = de.at("enabled").get<bool>();
            if(de.contains("selected")) buttonDe.selected = de.at("selected").get<bool>();
        }
        if(j.contains("buttonDeFull"))
        {
            const auto& deFull = j.at("buttonDeFull");
            if(deFull.contains("enabled")) buttonDeFull.enabled = deFull.at("enabled").get<bool>();
            if(deFull.contains("selected")) buttonDeFull.selected = deFull.at("selected").get<bool>();
        }
    }

    nlohmann::json toJson () const
    {
        nlohmann::json j;
        if(installPathIsSet) j["installPathIsSet"] = installPathIsSet.value();

        if(newInstallPath) j["newInstallPath"] = newInstallPath->string();

        if(buttonChannel.enabled || buttonChannel.active)
        {
            nlohmann::json channel;
            if(buttonChannel.enabled) channel["enabled"] = buttonChannel.enabled.value();
            if(buttonChannel.active) channel["active"] = buttonChannel.active.value();
            j["buttonChannel"] = channel;
        }
        if(buttonEng.enabled || buttonEng.selected)
        {
            nlohmann::json eng;
            if(buttonEng.enabled) eng["enabled"] = buttonEng.enabled.value();
            if(buttonEng.selected) eng["selected"] = buttonEng.selected.value();
            j["buttonEng"] = eng;
        }
        if(buttonDe.enabled || buttonDe.selected)
        {
            nlohmann::json de;
            if(buttonDe.enabled) de["enabled"] = buttonDe.enabled.value();
            if(buttonDe.selected) de["selected"] = buttonDe.selected.value();
            j["buttonDe"] = de;
        }
        if(buttonDeFull.enabled || buttonDeFull.selected)
        {
            nlohmann::json deFull;
            if(buttonDeFull.enabled) deFull["enabled"] = buttonDeFull.enabled.value();
            if(buttonDeFull.selected) deFull["selected"] = buttonDeFull.selected.value();
            j["buttonDeFull"] = deFull;
        }
        return j;
    }
};

}  // namespace Protocol
