#pragma once

#include "ChannelBase.hpp"

class LiveChannel : public ChannelBase
{
   public:
    explicit LiveChannel (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : ChannelBase("LIVE", parent, grid, 2)
    {
        grid->addWidget(buttonChannel, 1, 0);
        grid->addWidget(buttonEng, 1, 1);
        grid->addWidget(buttonDe, 1, 2);
        grid->addWidget(buttonDeVoll, 1, 3);
    }
};
