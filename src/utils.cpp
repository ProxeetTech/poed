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

#include "utils.h"
#include "logs.h"
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <dirent.h>

void create_default_config(const std::string& config_path) {
    std::ofstream config_file(config_path);
    if (!config_file.is_open()) {
        syslog(LOG_ERR, "Failed to create default config file at %s\n", config_path.c_str());
        return;
    }

    config_file << "# System settings for poed\n";
    config_file << "config general\n";
    config_file << "\toption log_level 'debug'                         # Log level: debug, info, notice, warning, err, crit, alert, emerg\n";
    config_file << "\toption unix_socket_enable '1'                    # Enable (1) or disable (0) the unix socket server\n";
    config_file << "\toption unix_socket_path '/var/run/poed.sock'     # Path to the Unix socket used for inter-process communication\n";
    config_file << "\n";

    config_file << "config controller\n";
    config_file << "\toption path '/sys/bus/i2c/devices/i2c-8/8-002c'  # Path to the PoE controller in /sys\n";
    config_file << "\toption ports '4'                                 # Number of ports on this controller\n";
    config_file << "\toption total_power_budget '120'                  # Total power budget for all ports (in watts)\n";
    config_file << "\n";

    config_file << "config controller\n";
    config_file << "\toption path '/sys/bus/i2c/devices/i2c-8/8-000c'  # Path to the PoE controller in /sys\n";
    config_file << "\toption ports '4'                                 # Number of ports on this controller\n";
    config_file << "\toption total_power_budget '120'                  # Total power budget for all ports (in watts)\n";
    config_file << "\n";

    config_file << "config port\n";
    config_file << "\toption name 'eth9'\n";
    config_file << "\toption controller '0'                            # Reference to the controller this port belongs to\n";
    config_file << "\toption port_number '0'                           # Port number on the controller\n";
    config_file << "\toption power_budget '15'                         # Power budget for this port (in watts)\n";
    config_file << "\toption mode 'AUTO'                               # Port operating mode: AUTO, 48V, 24V\n";
    config_file << "\toption priority '1'                              # Power priority of the port (1 = highest)\n";
    config_file << "\n";

    config_file << "config port\n";
    config_file << "\toption name 'eth10'\n";
    config_file << "\toption controller '0'\n";
    config_file << "\toption port_number '1'\n";
    config_file << "\toption power_budget '15'\n";
    config_file << "\toption mode 'AUTO'\n";
    config_file << "\toption priority '2'\n";
    config_file << "\n";

    config_file << "config port\n";
    config_file << "\toption name 'eth11'\n";
    config_file << "\toption controller '0'\n";
    config_file << "\toption port_number '2'\n";
    config_file << "\toption power_budget '15'\n";
    config_file << "\toption mode 'AUTO'\n";
    config_file << "\toption priority '2'\n";
    config_file << "\n";

    config_file << "config port\n";
    config_file << "\toption name 'eth12'\n";
    config_file << "\toption controller '0'\n";
    config_file << "\toption port_number '3'\n";
    config_file << "\toption power_budget '15'\n";
    config_file << "\toption mode 'AUTO'\n";
    config_file << "\toption priority '2'\n";
    config_file << "\n";

    config_file << "config port\n";
    config_file << "\toption name 'eth13'\n";
    config_file << "\toption controller '1'\n";
    config_file << "\toption port_number '0'\n";
    config_file << "\toption power_budget '15'\n";
    config_file << "\toption mode 'AUTO'\n";
    config_file << "\toption priority '2'\n";
    config_file << "\n";

    config_file << "config port\n";
    config_file << "\toption name 'eth14'\n";
    config_file << "\toption controller '1'\n";
    config_file << "\toption port_number '1'\n";
    config_file << "\toption power_budget '15'\n";
    config_file << "\toption mode 'AUTO'\n";
    config_file << "\toption priority '2'\n";
    config_file << "\n";

    config_file << "config port\n";
    config_file << "\toption name 'eth15'\n";
    config_file << "\toption controller '1'\n";
    config_file << "\toption port_number '2'\n";
    config_file << "\toption power_budget '15'\n";
    config_file << "\toption mode 'AUTO'\n";
    config_file << "\toption priority '2'\n";
    config_file << "\n";

    config_file << "config port\n";
    config_file << "\toption name 'eth16'\n";
    config_file << "\toption controller '1'\n";
    config_file << "\toption port_number '3'\n";
    config_file << "\toption power_budget '15'\n";
    config_file << "\toption mode 'AUTO'\n";
    config_file << "\toption priority '2'\n";
    config_file << "\n";

    config_file.close();
    syslog(LOG_INFO, "Default log file generated by path %s\n", config_path.c_str());

    // chmod 600 /etc/config/poed
}

std::string cat(const std::string& filePath) {
    std::ifstream file(filePath);

    /* Check if the file was successfully opened */
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filePath);
    }

    /* Use stringstream to read the entire content of the file into a string */
    std::ostringstream oss;
    oss << file.rdbuf();

    return oss.str();
}

void echo(const std::string& filePath, const std::string& content) {
    std::ofstream file(filePath);

    /* Check if the file was successfully opened */
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filePath);
    }

    /* Write the content to the file */
    file << content;

    /* Close the file */
    file.close();
}

int countLines(const std::string& content, char comment_char) {
    std::istringstream stream(content);
    std::string line;
    int count = 0;

    /* Iterate through each line in the content */
    while (std::getline(stream, line)) {
        /* Find the first non-whitespace character */
        std::string::size_type start = line.find_first_not_of(" \t");

        /* Check if the line is not empty and does not start with the comment character */
        if (start != std::string::npos && line[start] != comment_char) {
            ++count;
        }
    }

    return count;
}

/* Function to return the line at a specified index, ignoring lines that start with the comment character */
std::string getLineByIndex(const std::string& content, int index, char commentChar) {
    std::istringstream stream(content);
    std::string line;
    int currentIndex = 0;

    /* Iterate through each line in the content */
    while (std::getline(stream, line)) {
        /* Find the first non-whitespace character */
        std::string::size_type start = line.find_first_not_of(" \t");

        /* Skip lines that are empty or start with the comment character */
        if (start != std::string::npos && line[start] != commentChar) {
            if (currentIndex == index) {
                return line;
            }
            ++currentIndex;
        }
    }

    /* If the specified index is out of range, return an empty string */
    return "";
}

/* Function to extract a substring by index from a space-separated string */
std::string getSubstringByIndex(const std::string& input, int index) {
    std::istringstream stream(input);
    std::string token;
    int currentIndex = 0;

    /* Iterate through each token in the input string separated by spaces */
    while (std::getline(stream, token, ' ')) {
        /* Skip empty tokens caused by consecutive spaces */
        if (token.empty()) {
            continue;
        }

        /* Check if the current index matches the desired index */
        if (currentIndex == index) {
            return token;
        }

        ++currentIndex;
    }

    /* Return an empty string if the index is out of range */
    return "";
}

bool validateUciConfig(const UciConfig& config) {
    /* Validate the 'general' section */
    std::vector<std::string> general_options = {
            "log_level", "unix_socket_enable", "unix_socket_path"
    };
    if (!config.validateSection("general", 1, general_options)) {
        return false;
    }

    /* Validate the 'controller' section */
    std::vector<std::string> controller_options = {
            "path", "ports", "total_power_budget"
    };
    if (!config.validateSection("controller", 0, controller_options)) {
        return false;
    }

    /* Validate the 'port' section */
    std::vector<std::string> port_options = {
            "name", "controller", "port_number", "power_budget", "mode", "priority"
    };
    if (!config.validateSection("port", 0, port_options)) {
        return false;
    }

    /* Check poe controller ports number */
    auto sections = config.getSections();
    for (auto controller: sections["controller"]) {
        string controller_path = controller.options["path"];
        string portinfo_path = controller_path + string("/port_info");

        /* Get port info */
        syslog(LOG_DEBUG, "Check PoE controller paths...\n");
        string portinfo;
        try {
            portinfo = cat(portinfo_path);
            syslog(LOG_DEBUG, "Port info: %s\n", portinfo.c_str());
        } catch (const exception& e) {
            syslog(LOG_ERR, "Path %s can not be opened\n", portinfo_path.c_str());
            return false;
        }

        int cnt_lines = countLines(portinfo, '#');
        int cnt_lines_cfg = stoi(controller.options["ports"]);
        if (cnt_lines != cnt_lines_cfg) {
            syslog(LOG_ERR, "Controller %s has wrong ports number, in config: %d, in fact: %d\n",
                   controller_path.c_str(), cnt_lines_cfg, cnt_lines);
            return false;
        }
    }

    /* Check ports-to-controller correspondence */
    auto ports = sections["port"];
    auto controllers = sections["controller"];
    int cnt = 0;
    for (auto port: ports) {
        /* Get controller index corresponded to port */
        uint8_t controller_ind = stoi(port.options["controller"]);
        uint8_t port_ind = stoi(port.options["port_number"]);

        if (controller_ind >= controllers.size()) {
            syslog(LOG_ERR, "Port %d has wrong controller index\n", cnt);
            return false;
        }

        uint8_t controller_ports = stoi(controllers.at(controller_ind).options["ports"]);
        if (port_ind >= controller_ports) {
            syslog(LOG_ERR, "Port %d has wrong index in controller %d\n", cnt, controller_ind);
            return false;
        }

        cnt++;
    }

    /* Configuration is valid */
    syslog(LOG_INFO, "Configuration is valid");
    return true;
}

vector<pid_t> getProcessIdsByName(const std::string& processName) {
    vector<pid_t> processIds;
    DIR* dir = opendir("/proc");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            string d_name = entry->d_name;
            pid_t pid = 0;
            try {
                pid = stoi(d_name);
                if (pid > 0) {
                    ifstream cmdline("/proc/" + to_string(pid) + "/comm");
                    if (cmdline.is_open()) {
                        string name;
                        if (getline(cmdline, name)) {
                            if (!name.empty() && name.back() == '\n') {
                                name.pop_back();
                            }
                            if (name == processName && pid != getpid()) {
                                processIds.push_back(pid);
                            }
                        }
                    }
                }
            } catch (const invalid_argument& e) {
                continue;
            } catch (const out_of_range& e) {
                continue;
            }
        }
        closedir(dir);
    }
    return processIds;
}

string getProcessName(pid_t pid) {
    ifstream comm_file("/proc/" + to_string(pid) + "/comm");
    if (comm_file.is_open()) {
        string name;
        getline(comm_file, name);
        comm_file.close();
        return name;
    } else {
        return "";
    }
}