#include "app/AppInfo.h"

#include <iostream>

int main()
{
    static_assert(__cplusplus >= 202002L);

    if (c2paseq::appInfo::name != "C2PA Creative Sequencer")
        return 1;

    if (c2paseq::appInfo::version != "0.1.0")
        return 2;

    if (c2paseq::appInfo::defaultWindowWidth < 760
        || c2paseq::appInfo::defaultWindowHeight < 480)
        return 3;

    std::cout << "application skeleton metadata: ok\n";
    return 0;
}
