#pragma once

#include "ChannelBase.hpp"

class HotfixChannel : public ChannelBase
{
   public:
    explicit HotfixChannel (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : ChannelBase("HOTFIX", parent, grid, 2)
    {
        grid->addWidget(buttonChannel, 4, 0);
        grid->addWidget(buttonEng, 4, 1);
        grid->addWidget(buttonDe, 4, 2);
        grid->addWidget(buttonDeVoll, 4, 3);
    }
};
