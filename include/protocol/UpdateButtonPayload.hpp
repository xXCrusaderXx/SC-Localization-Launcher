#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>

namespace Protocol
{
struct UpdateButton
{
    std::optional<bool> enabled = false;
    std::optional<bool> busy = false;
    std::optional<bool> updateButtonClicked = false;
    std::optional<bool> LaunchScAfterTranslation = false;
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
            if(j.at("updateButton").contains("busy")) updateButton.busy = j.at("updateButton").at("busy").get<bool>();
            if(j.at("updateButton").contains("LaunchScAfterTranslation"))
                updateButton.LaunchScAfterTranslation = j.at("updateButton").at("LaunchScAfterTranslation").get<bool>();
            if(j.at("updateButton").contains("updateButtonClicked"))
                updateButton.updateButtonClicked = j.at("updateButton").at("updateButtonClicked").get<bool>();
        }
    }

    nlohmann::json toJson () const
    {
        nlohmann::json j;
        if(updateButton.enabled) j["updateButton"]["enabled"] = updateButton.enabled.value();
        if(updateButton.busy) j["updateButton"]["busy"] = updateButton.busy.value();
        if(updateButton.LaunchScAfterTranslation) j["updateButton"]["LaunchScAfterTranslation"] = updateButton.LaunchScAfterTranslation.value();
        if(updateButton.updateButtonClicked) j["updateButton"]["updateButtonClicked"] = updateButton.updateButtonClicked.value();
        return j;
    }
};

}  // namespace Protocol
