#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Protocol
{
namespace UpdateButtonPayload
{
struct UpdateButton
{
    bool enabled = false;
    bool busy = false;
    bool LaunchScAfterTranslation = false;
};

struct Request
{
    bool updateButtonClicked = false;

    nlohmann::json to_json () const
    {
        nlohmann::json j;
        j["updateButtonClicked"] = updateButtonClicked;
        return j;
    }
    void from_json (const nlohmann::json& j) { updateButtonClicked = j.at("updateButtonClicked").get<bool>(); }
};

struct Response
{
    UpdateButton updateButton;

    nlohmann::json to_json () const
    {
        nlohmann::json j;
        j["UpdateButton"] = {
            {"enabled", updateButton.enabled}, {"busy", updateButton.busy}, {"LaunchScAfterTranslation", updateButton.LaunchScAfterTranslation}};
        return j;
    }

    void from_json (const nlohmann::json& j)
    {
        updateButton.enabled = j.at("button").at("enabled").get<bool>();
        updateButton.busy = j.at("button").at("busy").get<bool>();
        updateButton.LaunchScAfterTranslation = j.at("button").at("LaunchScAfterTranslation").get<bool>();
    }
};

}  // namespace UpdateButtonPayload
}  // namespace Protocol
