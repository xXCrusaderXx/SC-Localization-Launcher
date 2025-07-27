#pragma once

#include "ChannelBase.hpp"

class PtuChannel : public ChannelBase
{
   public:
    explicit PtuChannel (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : ChannelBase("PTU", parent, grid, 2)
    {
        grid->addWidget(buttonChannel, 2, 0);
        grid->addWidget(buttonEng, 2, 1);
        grid->addWidget(buttonDe, 2, 2);
        grid->addWidget(buttonDeVoll, 2, 3);
    }
};
