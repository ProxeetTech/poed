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

#include "poe_controller.h"
#include "poe_simulator.h"
#include <syslog.h>

static map<string, enum PoeState> states = {
        {"0(NONE)", PoeState::NONE},
        {"1(DCP)", PoeState::DCP},
        {"2(HIGH_CAP)", PoeState::HIGH_CAP},
        {"3(RLOW)", PoeState::RLOW},
        {"4(DET_OK)", PoeState::DET_OK},
        {"5(RHIGH)", PoeState::RHIGH},
        {"6(OPEN)", PoeState::OPEN},
        {"7(DCN)", PoeState::DCN}
};

PoePort::PoePort() {
    index = 0;
    priority = 0;
    voltage = 0.0;
    current = 0.0;
    power = 0.0;
    enable_flag = false;
    enable_perm = false;
    overbudget_flag = false;
    state = PoeState::NONE;
    mode = PoeMode::POE_OFF;
    budget = 0.0;
    test_mode = false;
}

bool PoePort::getData() {
    string portinfo_path = contr_path + string("/port_info");
    string portinfo;
    string port_params_str;
    string portstat_path = contr_path + string("/port_status");
    string portstat;
    string port_status_str;
    if (test_mode) {
        /* Get simulated PoE data */
        vector<string> sim_data = this->port_sim.getData();
        if (sim_data.empty()) {
            syslog(LOG_ERR, "There is no simulated data in test mode\n");
            return false;
        }
        port_params_str = sim_data.at(0);
        port_status_str = sim_data.at(1);
    } else {
        /* Read real values */
        try {
            portinfo = cat(portinfo_path);
        } catch (const exception& e) {
            syslog(LOG_ERR, "Path %s can not be opened\n", portinfo_path.c_str());
            return false;
        }
        try {
            portstat = cat(portstat_path);
        } catch (const exception& e) {
            syslog(LOG_ERR, "Path %s can not be opened\n", portstat_path.c_str());
            return false;
        }
        port_params_str = getLineByIndex(portinfo, index, '#'); //i.e: 0 eth14 auto 0.0 0.000
        port_status_str = getLineByIndex(portstat, index, '#'); //i.e: 0 eth14 6(OPEN) 0(Unknown)
    }

    mode_str = getSubstringByIndex(port_params_str, 2);
    voltage_str = getSubstringByIndex(port_params_str, 3);
    current_str = getSubstringByIndex(port_params_str, 4);
    state_str = getSubstringByIndex(port_status_str, 2);
    load_type_str = getSubstringByIndex(port_status_str, 3);

//    cout << this->name << ": " << port_params_str;
//    cout << this->name << ": " << port_status_str;

    state = states[state_str];
    voltage = stod(voltage_str);
    current = stod(current_str);
    power = voltage * current;
    return true;
}

bool PoePort::powerOff() {
    if (test_mode) {
        port_sim.turnOff();
        syslog(LOG_DEBUG, "Simulated PoE port %d power off, controller %s\n",
               index, contr_path.c_str());
        enable_flag = false;
        return true;
    }

    string poe_off_path = contr_path + string("/port_power_off");
    try {
        echo(poe_off_path, to_string(index));
        syslog(LOG_DEBUG, "PoE port %d power off, controller %s\n", index, contr_path.c_str());
    } catch (const exception& e) {
        syslog(LOG_ERR, "Path %s can not be opened\n", poe_off_path.c_str());
        return false;
    }
    enable_flag = false;
    return true;
}

bool PoePort::powerOn() {
    if (test_mode) {
        port_sim.turnOn();
        syslog(LOG_DEBUG, "Simulated PoE port %d power on, controller %s\n",
               index, contr_path.c_str());
        enable_flag = true;
        return true;
    }

    string poe_on_path = contr_path + string("/port_power_on");
    try {
        echo(poe_on_path, to_string(index));
        syslog(LOG_DEBUG, "PoE port %d power on, controller %s\n", index, contr_path.c_str());
    } catch (const exception& e) {
        syslog(LOG_ERR, "Path %s can not be opened\n", poe_on_path.c_str());
        return false;
    }
    enable_flag = true;
    return true;
}

bool PoePort::setMode(enum PoeMode new_mode) {
    syslog(LOG_INFO, "Set mode %s for PoE port %d, controller %s\n",
           poeModeToString(new_mode).c_str(), index, contr_path.c_str());

    string poe_mode_path = contr_path + string("/port_mode");
    if (!powerOff()) {
        return false;
    }
    syslog(LOG_DEBUG, "PoE port %d power off, controller %s\n", index, contr_path.c_str());

    switch (new_mode) {
        case PoeMode::POE_OFF:
            return true;
        case PoeMode::POE_AUTO:
            if (!test_mode) {
                try {
                    echo(poe_mode_path, to_string(index) + string("auto"));
                    syslog(LOG_DEBUG, "PoE port %d set mode auto, controller %s\n", index, contr_path.c_str());
                } catch (const exception& e) {
                    syslog(LOG_ERR, "Path %s can not be opened\n", poe_mode_path.c_str());
                    return false;
                }
            }
            break;
        case PoeMode::POE_48V:
            if (!test_mode) {
                try {
                    echo(poe_mode_path, to_string(index) + string("manual"));
                    syslog(LOG_DEBUG, "PoE port %d set mode manual, controller %s\n", index, contr_path.c_str());
                } catch (const exception &e) {
                    syslog(LOG_ERR, "Path %s can not be opened\n", poe_mode_path.c_str());
                    return false;
                }
            }
            break;
        case PoeMode::POE_24V:
            //TODO
            return false;
        default:
            return false;
    }

    if (!powerOn()) {
        return false;
    }
    syslog(LOG_DEBUG, "PoE port %d power on, controller %s\n", index, contr_path.c_str());
    this->mode = new_mode;

    return true;
}

void PoePort::initSim() {
    double working_voltage = 0.0;
    if (this->mode == PoeMode::POE_48V ||
            this->mode == PoeMode::POE_AUTO) {
        working_voltage = 48.0;
    } else {
        working_voltage = 24.0;
    }
    double voltage_var = PoeSimProfile::generateRandomDouble(0.0, 1.5);
    double current_init = PoeSimProfile::generateRandomDouble(0.1, 0.2);
    double current_var = PoeSimProfile::generateRandomDouble(0.0, 0.1);
    double current_inc = PoeSimProfile::generateRandomDouble(0.0, 0.04);

    port_sim.addProfile(PoeSimProfile(PoeSimProfile::generateRandomNumber(10, 30),
                                      working_voltage, voltage_var,
                                      current_init, current_var, current_inc,
                                      "4(DET_OK)", "6(0)"));
    port_sim.addProfile(PoeSimProfile(PoeSimProfile::generateRandomNumber(10, 30),
                                      0.0, 0.0,
                                      0.0, 0.0, 0.0,
                                      "6(OPEN)", "0(Unknown)"));
    port_sim.addProfile(PoeSimProfile(PoeSimProfile::generateRandomNumber(10, 30),
                                      working_voltage, voltage_var,
                                      current_init, current_var, current_inc,
                                      "4(DET_OK)", "6(0)"));

    if (this->mode == PoeMode::POE_OFF) {
        port_sim.turnOff();
    }
}


bool PoeController::getPortsData() {
    for (auto& port: ports) {
        if (!port.getData()) {
            return false;
        }
    }
    return true;
}

PoePort& PoeController::getLowesPrioPort() {
    int lowest_prio = 0;
    int lowest_prio_ind = 0;
    for (int i = 0; i < ports.size(); i++) {
        if (!ports.at(i).enable_flag) {
            continue;
        }
        if (ports.at(i).priority > lowest_prio) {
            lowest_prio_ind = i;
            lowest_prio = ports.at(i).priority;
        }
    }
    return ports.at(lowest_prio_ind);
}



string poeStateToString(PoeState state) {
    // Iterate over the map to find the corresponding string
    for (const auto& pair : states) {
        if (pair.second == state) {
            return pair.first;  // Return the corresponding string
        }
    }

    // Log an error to syslog if the state is not found
    syslog(LOG_ERR, "Invalid PoE state value. Returning 'UNKNOWN'.");
    return "UNKNOWN";  // Return default string if not found
}

enum PoeMode parsePoeMode(const string& mode) {
    map<string, enum PoeMode> modes = {
            {"AUTO", PoeMode::POE_AUTO},
            {"48V", PoeMode::POE_48V},
            {"24V", PoeMode::POE_24V},
            {"OFF", PoeMode::POE_OFF}
    };

    /* Check if the mode exists in the map */
    auto it = modes.find(mode);
    if (it == modes.end()) {
        /* Log an error to syslog if the mode is not found */
        syslog(LOG_ERR, "Invalid PoE mode: %s. Defaulting to POE_OFF.", mode.c_str());
        return PoeMode::POE_OFF;  // Return the default value POE_OFF
    }
    return it->second;  // Return the corresponding PoeMode value
}

string poeModeToString(PoeMode mode) {
    /* Map from PoeMode to string */
    map<PoeMode, std::string> modeToStringMap = {
            {PoeMode::POE_AUTO, "AUTO"},
            {PoeMode::POE_48V, "48V"},
            {PoeMode::POE_24V, "24V"},
            {PoeMode::POE_OFF, "OFF"}
    };

    /* Check if the mode exists in the map */
    auto it = modeToStringMap.find(mode);
    if (it == modeToStringMap.end()) {
        /* Log an error to syslog if the mode is not found */
        syslog(LOG_ERR, "Invalid PoE mode value. Returning 'UNKNOWN'.");
        return "UNKNOWN";  // Return default string "UNKNOWN"
    }

    return it->second;  // Return the corresponding string value
}