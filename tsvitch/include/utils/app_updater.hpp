#pragma once
#include <string>
namespace tsvitch {
void setApplicationPath(const std::string& path);
void checkForUpdates(bool showUpToDate = true);
void installLatestUpdate();
void applyPendingUpdate();
void shutdownUpdater();
}
