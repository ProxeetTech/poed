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

#include "poe_simulator.h"

double PoeSimProfile::generateRandomDouble(double min, double max) {
    // Create a random number generator using random_device as a seed
    random_device rd;    // Obtain a random number from hardware
    mt19937 gen(rd());   // Seed the generator with the random device
    uniform_real_distribution<> dis(min, max); // Define the range for double

    return dis(gen); // Generate the random number within the specified range
}

int PoeSimProfile::generateRandomNumber(int min, int max) {
    // Create a random number generator using random_device as a seed
    random_device rd;  // Obtain a random number from hardware
    mt19937 gen(rd()); // Seed the generator with the random device
    uniform_int_distribution<> dis(min, max); // Define the range

    return dis(gen); // Generate the random number within the specified range
}

string PoeSimProfile::to_string(double value, int precision) {
    ostringstream out;
    out.precision(precision);       // Set the desired precision
    out << fixed << value;     // Use fixed-point notation
    return out.str();
}

PoeSimProfile::PoeSimProfile(int steps, double volt, double volt_var,
              double curr_init, double curr_var, double curr_inc,
              string state, string poe_class) {
    this->steps_total = steps;
    this->voltage = volt;
    this->volt_variation = volt_var;
    this->current = curr_init;
    this->current_init = curr_init;
    this->curr_variation = curr_var;
    this->curr_increment = curr_inc;
    this->state = std::move(state);
    this->poe_class = std::move(poe_class);
    this->current_step = 0;
    this->enabled = true;
}

string PoeSimProfile::getPortInfo() { //i.e.: "0 eth14 auto 0.0 0.000"
    double v = 0.0;
    double c = 0.0;
    if (enabled) {
        v = generateRandomDouble(voltage - volt_variation, voltage + volt_variation);
        c = generateRandomDouble(current - curr_variation, current + curr_variation);
        current += curr_increment;
    } else {
        v = 0.0;
        c = 0.0;
    }
    string result = string("N ethX mode ") + to_string(v, 1) + string(" ") +
            to_string(c, 3) + string(" \n");
    return result;
}

string PoeSimProfile::getPortStatus() { //i.e.: "0 eth14 6(OPEN) 0(Unknown)"
    string result = string("N ethX ") + state + string(" ") + poe_class + string(" \n");
    return result;
}

vector<string> PoeSimProfile::getData() {
    vector<string> result;
    if (current_step >= steps_total) {
        return result;
    }
    result.push_back(getPortInfo());
    result.push_back(getPortStatus());
    current_step++;
    return result;
}

void PoeSimProfile::reset() {
    current_step = 0;
    current = current_init;
}

void PoeSimProfile::turnOn() {
    enabled = true;
}

void PoeSimProfile::turnOff() {
    enabled = false;
}


PoePortSim::PoePortSim(vector<PoeSimProfile> profiles) {
    sim_profiles = std::move(profiles);
    curr_profile = 0;
}

PoePortSim::PoePortSim() {
    curr_profile = 0;
}

void PoePortSim::addProfile(const PoeSimProfile& profile) {
    sim_profiles.push_back(profile);
}

vector<string> PoePortSim::getData() {
    if (sim_profiles.empty()) {
        return vector<string>{};
    }
    vector<string> result = sim_profiles.at(curr_profile).getData();
    if (result.empty()) {
        curr_profile++;
        if (curr_profile >= sim_profiles.size()) {
            reset();
        }
        return getData();
    }
    return result;
}

void PoePortSim::reset() {
    curr_profile = 0;
    for (auto& profile: sim_profiles) {
        profile.reset();
    }
}

void PoePortSim::turnOff() {
    for (auto& profile: sim_profiles) {
        profile.turnOff();
    }
}

void PoePortSim::turnOn() {
    for (auto& profile: sim_profiles) {
        profile.turnOn();
    }
    reset();
}


void PoeDataSim::addPortSim(const PoePortSim& port_sim) {
    ports_data.push_back(port_sim);
}

vector<string> PoeDataSim::getData() {
    string ports_info = string("# name mode voltage current\n");
    string ports_status = string("# name det_st class\n");
    for (auto& port_sim: ports_data) {
        vector<string> port_data = port_sim.getData();
        ports_info += port_data.at(0);
        ports_status += port_data.at(1);
    }
    vector<string> result = {ports_info, ports_status};
    return result;
}