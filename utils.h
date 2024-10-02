#ifndef ROUTER_POED_UTILS_H
#define ROUTER_POED_UTILS_H

extern "C" {
#include <uci.h>
}

#include <string>
#include "uci_config.h"

#define POE_PWR_HYSTERESIS       5

enum class PoeMode {
    POE_OFF = 0,
    POE_AUTO,
    POE_48V,
    POE_24V
};

enum class PoeState {
    NONE = 0,
    DCP,
    HIGH_CAP,
    RLOW,
    DET_OK,
    RHIGH,
    OPEN,
    DCN
};

void create_default_config(const std::string& config_path);
std::string cat(const std::string& filePath);
void echo(const std::string& filePath, const std::string& content);
int countLines(const std::string& content, char comment_char);
bool validateUciConfig(const UciConfig& config);
std::string getLineByIndex(const std::string& content, int index, char commentChar);
std::string getSubstringByIndex(const std::string& input, int index);

#endif //ROUTER_POED_UTILS_H
