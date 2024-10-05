/*
 * © 2024 Proxeet Limited
 *
 * The poed packet is licensed under the GNU Lesser General Public License version 3,
 * published by the Free Software Foundation.
 *
 * You may use, distribute, and modify this code under the terms of the LGPLv3.
 *
 * The project is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License version 3 for more details.
 *
 * Author: Aleksey Vasilenko (a.vasilenko@proxeet.com)
 */

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
string requestFromUnixSocket(const string& socket_path, const string& message, int timeout_ms);
std::vector<pid_t> getProcessIdsByName(const string& processName);
std::string getProcessName(pid_t pid);

#endif //ROUTER_POED_UTILS_H
