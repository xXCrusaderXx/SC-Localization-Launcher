#pragma once

#include <functional>
#include <iostream>
#include <thread>
#include "GitRepoHandler/GitRepoHandler.h"

class GitCloner
{
   public:
    using Callback = std::function<void(bool)>;
    GitCloner (Callback cb)
        : callback(cb)
    {
    }

    void run ()
    {
        GitRepoHandler handler;
        callback(handler.cloneRepo());
    }

   private:
    Callback callback;
};
