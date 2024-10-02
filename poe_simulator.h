#ifndef POED_POE_SIMULATOR_H
#define POED_POE_SIMULATOR_H

#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include "utils.h"

class PoeSimProfile {
private:
    int steps_total;
    int current_step;
    double voltage;
    double current_init;
    double current;
    double volt_variation;
    double curr_variation;
    double curr_increment;
    string state;
    string poe_class;
    bool enabled;

    string to_string(double value, int precision);

public:
    PoeSimProfile(int steps, double volt, double volt_var,
                  double curr_init, double curr_var, double curr_inc,
                  string state, string poe_class);

    string getPortInfo();
    string getPortStatus();
    vector<string> getData();
    void reset();
    void turnOn();
    void turnOff();

    static double generateRandomDouble(double min, double max);
    static int generateRandomNumber(int min, int max);
};

class PoePortSim {
private:
    vector<PoeSimProfile> sim_profiles;
    int curr_profile;

public:
    PoePortSim(vector<PoeSimProfile> profiles);
    PoePortSim();

    void addProfile(PoeSimProfile profile);
    vector<string> getData();
    void reset();

    void turnOff();
    void turnOn();
};

class PoeDataSim {
private:
    vector<PoePortSim> ports_data;

public:
    void addPortSim(PoePortSim port_sim);
    vector<string> getData();
};


#endif //POED_POE_SIMULATOR_H
