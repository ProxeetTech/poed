#ifndef POED_POE_CONTROLLER_H
#define POED_POE_CONTROLLER_H

#include <string>
#include <vector>
#include "utils.h"

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

    PoePort();

    bool getData();
    bool powerOff();
    bool powerOn();
    bool setMode(enum PoeMode mode);
};

struct PoeController {
    std::string path;
    double total_budget{};
    std::vector<PoePort> ports;

    bool getPortsData();
    PoePort& getLowesPrioPort();
};

#endif //POED_POE_CONTROLLER_H
