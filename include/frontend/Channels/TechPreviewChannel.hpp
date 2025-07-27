#pragma once

#include "ChannelBase.hpp"

class TechPreviewChannel : public ChannelBase
{
   public:
    explicit TechPreviewChannel (QWidget *parent = nullptr, QGridLayout *grid = nullptr)
        : ChannelBase("TECH-PREVIEW", parent, grid, 2)
    {
        grid->addWidget(buttonChannel, 5, 0);
        grid->addWidget(buttonEng, 5, 1);
        grid->addWidget(buttonDe, 5, 2);
        grid->addWidget(buttonDeVoll, 5, 3);
    }
};
