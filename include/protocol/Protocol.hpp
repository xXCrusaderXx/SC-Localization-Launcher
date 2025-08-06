#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace Protocol
{

enum class MassageType
{
    Request,
    Response
};

class Massage
{
   private:
    nlohmann::json j;
    std::string sMassageType;

    std::string MassageTypeToString (const MassageType& massageType)
    {
        if(massageType == MassageType::Request) return "Request";
        if(massageType == MassageType::Response) return "Response";

        return "Unknown";
    }

    std::string MassageTypeFromJson (const nlohmann::json& json)
    {
        if(json.contains("Request")) return "Request";
        if(json.contains("Response")) return "Response";
        return "Unknown";
    }

   public:
    Massage (const MassageType& massageType) {}

    Massage (const nlohmann::json& msg)
        : j(msg)
        , sMassageType(MassageTypeFromJson(msg))
    {
    }

    virtual ~Massage () {};

    void AddModuleNode (const std::string& moduleNodeName, const nlohmann::json& targetNode) { j[moduleNodeName] = targetNode; }

    bool moduleExist (const std::string& moduleNodeName) const
    {
        if(j.contains(moduleNodeName)) return true;
        return false;
    }

    nlohmann::json getModuleNode (const std::string& moduleNodeName) const
    {
        if(j.contains(moduleNodeName)) return j.at(moduleNodeName);
        throw std::runtime_error("Module node not found: " + moduleNodeName);
    }

    nlohmann::json getJson () const { return j; }
};

}  // namespace Protocol
