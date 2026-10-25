#pragma once
#include <functional>
#include <string>
namespace tsvitch {
void openSearchInput(const std::string &title, const std::string &initial,
                     std::function<void(std::string)> submit);
}
