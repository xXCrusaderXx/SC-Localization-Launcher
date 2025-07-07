#pragma once

#include <functional>
#include <iostream>
#include <thread>
#include "GitRepoHandler/GitRepoHandler.h"

class GitChecker
{
   public:
    using Callback = std::function<void(bool)>;
    GitChecker (Callback cb)
        : callback(cb)
    {
    }

    void run ()
    {
        GitRepoHandler handler;
        callback(handler.isUpToDate());
    }

   private:
    Callback callback;
};
