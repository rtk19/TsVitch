#pragma once
#include <string>
namespace tsvitch {
void setApplicationPath(const std::string& path);
void installLatestUpdate();
void applyPendingUpdate();
void shutdownUpdater();
}
