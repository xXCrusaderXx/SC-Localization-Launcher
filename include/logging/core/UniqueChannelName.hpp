#pragma once

#include <atomic>
#include <string>

class UniqueChannelName
{
  public:
    inline static std::string create(const std::string& baseName)
    {
        int id = counter++;
        return baseName + "#" + std::to_string(id);
    }

    inline static void reset()
    {
        counter = 0;
    }

  private:
    inline static std::atomic<int> counter{0};
};
