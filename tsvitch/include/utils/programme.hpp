#pragma once
#include "api/tsvitch/result/home_live_result.h"
#include <functional>
namespace tsvitch {
struct Programme { std::string title, description, time; int64_t start=0,end=0; };
void chooseArchiveTime(const LiveM3u8& channel);
void presentProgrammeGuide(const LiveM3u8& channel,const std::vector<Programme>& programmes);
void primeProgrammeGuide(const LiveM3u8& channel);
Programme currentProgrammeData(const LiveM3u8& channel);
std::string currentProgramme(const LiveM3u8& channel);
}
