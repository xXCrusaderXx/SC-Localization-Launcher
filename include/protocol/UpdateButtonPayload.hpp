#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>

namespace Protocol
{
struct UpdateButton
{
    std::optional<bool> enabled;
    std::optional<bool> busy;
    std::optional<bool> active;
    std::optional<bool> updateButtonClicked;
    std::optional<bool> LaunchScAfterTranslation;
};

struct UpdateButtonPayload
{
    UpdateButton updateButton;

    UpdateButtonPayload () = default;

    UpdateButtonPayload (const nlohmann::json& j)
    {
        if(j.contains("updateButton"))
        {
            if(j.at("updateButton").contains("enabled")) updateButton.enabled = j.at("updateButton").at("enabled").get<bool>();
            if(j.at("updateButton").contains("active")) updateButton.active = j.at("updateButton").at("active").get<bool>();
            if(j.at("updateButton").contains("busy")) updateButton.busy = j.at("updateButton").at("busy").get<bool>();
            if(j.at("updateButton").contains("updateButtonClicked"))
                updateButton.updateButtonClicked = j.at("updateButton").at("updateButtonClicked").get<bool>();
            if(j.at("updateButton").contains("LaunchScAfterTranslation"))
                updateButton.LaunchScAfterTranslation = j.at("updateButton").at("LaunchScAfterTranslation").get<bool>();
        }
    }

    nlohmann::json toJson () const
    {
        nlohmann::json j;
        if(updateButton.enabled) j["updateButton"]["enabled"] = updateButton.enabled.value();
        if(updateButton.active) j["updateButton"]["active"] = updateButton.active.value();
        if(updateButton.busy) j["updateButton"]["busy"] = updateButton.busy.value();
        if(updateButton.updateButtonClicked) j["updateButton"]["updateButtonClicked"] = updateButton.updateButtonClicked.value();
        if(updateButton.LaunchScAfterTranslation) j["updateButton"]["LaunchScAfterTranslation"] = updateButton.LaunchScAfterTranslation.value();
        return j;
    }
};

}  // namespace Protocol
