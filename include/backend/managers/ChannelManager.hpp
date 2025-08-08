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

        for(auto& [channel, data] : backendData.channelData)
        {
            data.buttonEng.enabled = true;
            data.buttonChannel.enabled = true;
            if(!data.buttonDe.selected && !data.buttonDeFull.selected)
            {
                data.buttonEng.selected = true;
            }
            if(data.installPath.empty())
            {
                data.buttonChannel.active = false;
            }
            else
                data.buttonChannel.active = true;

            if(data.info.active)
            {
                if(!data.info.server1.empty()) data.buttonDe.enabled = true;
                if(!data.info.server2.empty()) data.buttonDeFull.enabled = true;
            }
        }
        for(const auto& [channel, data] : backendData.channelData)
        {
            if(!backendData.channelData[channel].installPath.empty()) backendData.anyChannelPathSet = true;
            break;
        }

        LOG_DEBUG(lg) << "initialSetup() - Finished";
    }

    void processRequest (const Protocol::Massage& request, Protocol::Massage& response)
    {
        LOG_DEBUG(lg) << "processRequest() - START";
        for(const auto& [channel, data] : backendData.channelData)
        {
            if(!request.moduleExist(channel))
            {
                continue;
            }
            Protocol::ChannelPayload channelReq(request.getModuleNode(channel));
            LOG_DEBUG(lg) << "processRequest() - [" << channel << "] request:\n" << channelReq.toJson().dump(4);
            if(channelReq.buttonDe.selected) backendData.channelData[channel].buttonDe.selected = channelReq.buttonDe.selected.value();
            if(channelReq.buttonDe.enabled) backendData.channelData[channel].buttonDe.enabled = channelReq.buttonDe.enabled.value();
            if(channelReq.buttonEng.selected) backendData.channelData[channel].buttonEng.selected = channelReq.buttonEng.selected.value();
            if(channelReq.buttonEng.enabled) backendData.channelData[channel].buttonEng.enabled = channelReq.buttonEng.enabled.value();
            if(channelReq.buttonDeFull.selected) backendData.channelData[channel].buttonDeFull.selected = channelReq.buttonDeFull.selected.value();
            if(channelReq.buttonDeFull.enabled) backendData.channelData[channel].buttonDeFull.enabled = channelReq.buttonDeFull.enabled.value();
            if(channelReq.newInstallPath) backendData.channelData[channel].installPath = channelReq.newInstallPath.value();

            Protocol::ChannelPayload channelResponse;
            channelResponse.installPathIsSet = !channelReq.newInstallPath.value().empty();
            if(channelResponse.installPathIsSet)
            {
                channelResponse.buttonChannel.active = channelResponse.installPathIsSet;
            }
            channelResponse.buttonChannel.enabled = true;
            channelResponse.buttonEng.enabled = backendData.channelData[channel].buttonEng.enabled;
            channelResponse.buttonEng.selected = backendData.channelData[channel].buttonEng.selected;
            channelResponse.buttonDe.enabled = backendData.channelData[channel].buttonDe.enabled;
            channelResponse.buttonDe.selected = backendData.channelData[channel].buttonDe.selected;
            channelResponse.buttonDeFull.enabled = backendData.channelData[channel].buttonDeFull.enabled;
            channelResponse.buttonDeFull.selected = backendData.channelData[channel].buttonDeFull.selected;

            if(!backendData.channelData[channel].installPath.empty())
            {
                channelResponse.buttonEng.enabled = true;
                channelResponse.buttonDe.enabled = true;
                channelResponse.buttonDeFull.enabled = true;
            }

            response.AddModuleNode(channel, channelResponse.toJson());
        }
        LOG_DEBUG(lg) << "processRequest() - FINISHED";
    }

    void updateChannelResponse (Protocol::Massage& response)
    {
        for(const auto& [channel, data] : backendData.channelData)
        {
            Protocol::ChannelPayload channelResponse;

            channelResponse.buttonChannel.enabled = backendData.channelData[channel].buttonChannel.enabled;
            channelResponse.buttonChannel.active = backendData.channelData[channel].buttonChannel.active;
            channelResponse.buttonEng.enabled = backendData.channelData[channel].buttonEng.enabled;
            channelResponse.buttonEng.selected = backendData.channelData[channel].buttonEng.selected;
            channelResponse.buttonDe.enabled = backendData.channelData[channel].buttonDe.enabled;
            channelResponse.buttonDe.selected = backendData.channelData[channel].buttonDe.selected;
            channelResponse.buttonDeFull.enabled = backendData.channelData[channel].buttonDeFull.enabled;
            channelResponse.buttonDeFull.selected = backendData.channelData[channel].buttonDeFull.selected;
            channelResponse.installPathIsSet = !backendData.channelData[channel].installPath.empty();
            if(channelResponse.installPathIsSet) backendData.anyChannelPathSet = true;
            response.AddModuleNode(channel, channelResponse.toJson());
        }
    }
};
