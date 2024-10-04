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

#ifndef POED_POE_CONTROLLER_H
#define POED_POE_CONTROLLER_H

#include <string>
#include <utility>
#include <vector>
#include "utils.h"
#include "poe_simulator.h"

struct PoePort {
    std::string contr_path;
    std::string name;
    int index;
    int priority;
    double voltage;
    double current;
    double power;
    bool enable_flag;
    bool enable_perm;
    bool overbudget_flag;
    enum PoeState state;
    enum PoeMode mode;
    double budget;
    bool test_mode;
    PoePortSim port_sim;
    string mode_str;
    string voltage_str;
    string current_str;
    string state_str;
    string load_type_str;

    PoePort();

    bool getData();
    bool powerOff();
    bool powerOn();
    bool setMode(enum PoeMode mode);
    void initSim();
};

struct PoeController {
    std::string path;
    double total_budget{};
    std::vector<PoePort> ports;

    bool getPortsData();
    PoePort& getLowesPrioPort();
};

enum PoeMode parsePoeMode(const string& mode);
string poeModeToString(PoeMode mode);
string poeStateToString(PoeState state);

#endif //POED_POE_CONTROLLER_H
