#pragma once

#include "ChannelBase.hpp"

class EptuChannel : public ChannelBase
{
   public:
    explicit EptuChannel (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : ChannelBase("EPTU", parent, grid, 2)
    {
        grid->addWidget(buttonChannel, 3, 0);
        grid->addWidget(buttonEng, 3, 1);
        grid->addWidget(buttonDe, 3, 2);
        grid->addWidget(buttonDeVoll, 3, 3);
    }
};
