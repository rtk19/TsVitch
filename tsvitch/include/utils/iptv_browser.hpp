#pragma once
#include "tsvitch/result/home_live_result.h"
namespace brls { class View; }
namespace tsvitch {
brls::View* createIptvLibrary();
void openIptvBrowser();
void showProgrammeInfo(const LiveM3u8& channel);
}
