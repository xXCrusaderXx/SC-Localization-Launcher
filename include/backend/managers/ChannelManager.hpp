#pragma once
#include <optional>

#include "backend/database/DataBase.hpp"
#include "logging/logging.h"
#include "protocol/ChannelPayload.hpp"
#include "protocol/Protocol.hpp"

class ChannelManager
{
   private:
    LoggerFramework::LogEx lg;

   public:
    ChannelManager ()
        : lg("ChannelManager")
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~ChannelManager () { LOG_DEBUG(lg) << "destructed"; }

    void initialSetup (Database::BackendData& backendData)
    {
        LOG_DEBUG(lg) << "initialSetup() - Start";

        for(auto& [channel, data] : backendData.channelData)
        {
            data.buttonEng.enabled = true;
            data.buttonEng.selected = true;
            data.buttonChannel.enabled = true;

            if(data.info.active)
            {
                if(!data.info.server1.empty()) data.buttonDe.enabled = true;
                if(!data.info.server2.empty()) data.buttonDeFull.enabled = true;
            }
        }

        LOG_DEBUG(lg) << "initialSetup() - Finished";
    }

    std::optional<Protocol::Response> processRequest (Database::BackendData& backendData, const Protocol::Request& msg)
    {
        LOG_DEBUG(lg) << "processRequest() - Start";

        std::string key = Protocol::toString(msg.type);

        if(key.empty())
        {
            LOG_ERROR(lg) << "processRequest() - Unknown channel type: " << key;
            return std::nullopt;
        }

        Protocol::ChannelPayload::Request req;
        req.from_json(msg.payload);

        backendData.channelData[key].buttonDe.selected = req.buttonDe.selected;
        backendData.channelData[key].buttonDe.enabled = req.buttonDe.enabled;
        backendData.channelData[key].buttonEng.selected = req.buttonEng.selected;
        backendData.channelData[key].buttonEng.enabled = req.buttonEng.enabled;
        backendData.channelData[key].buttonDeFull.selected = req.buttonDeFull.selected;
        backendData.channelData[key].buttonDeFull.enabled = req.buttonDeFull.enabled;
        backendData.channelData[key].installPath = req.newInstallPath;

        LOG_DEBUG(lg) << "processRequest() - Finished";

        Protocol::Response response;
        Protocol::ChannelPayload::Response channelResponse;
        channelResponse.installPathIsSet = !req.newInstallPath.empty();
        if(channelResponse.installPathIsSet)
        {
            channelResponse.buttonChannel.acitve = true;
        }
        channelResponse.buttonChannel.enabled = true;
        channelResponse.buttonEng.enabled = backendData.channelData[key].buttonEng.enabled;
        channelResponse.buttonEng.selected = backendData.channelData[key].buttonEng.selected;
        channelResponse.buttonDe.enabled = backendData.channelData[key].buttonDe.enabled;
        channelResponse.buttonDe.selected = backendData.channelData[key].buttonDe.selected;
        channelResponse.buttonDeFull.enabled = backendData.channelData[key].buttonDeFull.enabled;
        channelResponse.buttonDeFull.selected = backendData.channelData[key].buttonDeFull.selected;

        if(!backendData.channelData[key].installPath.empty())
        {
            channelResponse.buttonEng.enabled = true;
            channelResponse.buttonDe.enabled = true;
            channelResponse.buttonDeFull.enabled = true;
        }

        response.payload = channelResponse.to_json();
        response.type = msg.type;
        return response;
    }
};
