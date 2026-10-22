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
    Database::BackendData& backendData;

    // Enables the translation buttons depending on the install path and the servers the channel offers
    void updateAvailability (Database::ChannelData& data)
    {
        bool pathSet = !data.installPath.empty();
        bool serverActive = data.info.active != 0;

        data.buttonChannel.enabled = true;
        data.buttonChannel.active = pathSet;
        data.buttonEng.enabled = pathSet;
        data.buttonDe.enabled = pathSet && serverActive && !data.info.server1.empty();
        data.buttonDeFull.enabled = pathSet && serverActive && !data.info.server3.empty();

        // A selection the server no longer offers falls back to English
        if((data.buttonDe.selected && !data.buttonDe.enabled) || (data.buttonDeFull.selected && !data.buttonDeFull.enabled))
        {
            data.buttonDe.selected = false;
            data.buttonDeFull.selected = false;
        }
        if(!data.buttonDe.selected && !data.buttonDeFull.selected) data.buttonEng.selected = true;
    }

    void updateAnyChannelPathSet ()
    {
        backendData.anyChannelPathSet = false;
        for(const auto& [channel, data] : backendData.channelData)
        {
            if(!data.installPath.empty())
            {
                backendData.anyChannelPathSet = true;
                break;
            }
        }
    }

    Protocol::ChannelPayload toPayload (const Database::ChannelData& data)
    {
        Protocol::ChannelPayload channelResponse;
        channelResponse.installPathIsSet = !data.installPath.empty();
        channelResponse.buttonChannel.enabled = data.buttonChannel.enabled;
        channelResponse.buttonChannel.active = data.buttonChannel.active;
        channelResponse.buttonEng.enabled = data.buttonEng.enabled;
        channelResponse.buttonEng.selected = data.buttonEng.selected;
        channelResponse.buttonDe.enabled = data.buttonDe.enabled;
        channelResponse.buttonDe.selected = data.buttonDe.selected;
        channelResponse.buttonDeFull.enabled = data.buttonDeFull.enabled;
        channelResponse.buttonDeFull.selected = data.buttonDeFull.selected;
        return channelResponse;
    }

   public:
    ChannelManager (Database::BackendData& backendData)
        : lg("ChannelManager")
        , backendData(backendData)
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~ChannelManager () { LOG_DEBUG(lg) << "destructed"; }

    void initialSetup ()
    {
        LOG_DEBUG(lg) << "initialSetup() - Start";

        for(auto& [channel, data] : backendData.channelData) updateAvailability(data);
        updateAnyChannelPathSet();

        LOG_DEBUG(lg) << "initialSetup() - Finished";
    }

    void processRequest (const Protocol::Massage& request, Protocol::Massage& response)
    {
        LOG_DEBUG(lg) << "processRequest() - START";
        for(auto& [channel, data] : backendData.channelData)
        {
            if(!request.moduleExist(channel))
            {
                continue;
            }
            Protocol::ChannelPayload channelReq(request.getModuleNode(channel));
            LOG_DEBUG(lg) << "processRequest() - [" << channel << "] request:\n" << channelReq.toJson().dump(4);
            if(channelReq.buttonDe.selected) data.buttonDe.selected = channelReq.buttonDe.selected.value();
            if(channelReq.buttonEng.selected) data.buttonEng.selected = channelReq.buttonEng.selected.value();
            if(channelReq.buttonDeFull.selected) data.buttonDeFull.selected = channelReq.buttonDeFull.selected.value();
            if(channelReq.newInstallPath) data.installPath = channelReq.newInstallPath.value();

            updateAvailability(data);
            response.AddModuleNode(channel, toPayload(data).toJson());
        }
        updateAnyChannelPathSet();
        LOG_DEBUG(lg) << "processRequest() - FINISHED";
    }

    void updateChannelResponse (Protocol::Massage& response)
    {
        for(const auto& [channel, data] : backendData.channelData) response.AddModuleNode(channel, toPayload(data).toJson());
    }
};
