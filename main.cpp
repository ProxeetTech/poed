#include "utils.h"
#include "logs.h"
#include "clipp.h"
#include "poe_controller.h"
#include "poe_simulator.h"
#include <unistd.h>
#include <thread>
#include <iostream>

bool test_mode = false;

static int controlBudgets(vector<PoeController>& controllers);
static void controlBudgetsWithSleep(vector<PoeController>& controllers, int sleep_time_us);

int main(int argc, char *argv[]) {
    /* Parse command line arguments */
    vector<string> wrong;
    bool show_help = false;
    int monitor_period_us = 1000000;


    auto cli = (
            clipp::option("-p", "--monitor-period") &
            clipp::value("PoE ports monitoring period in us (default 1000000 us)", monitor_period_us),
            clipp::option("-t").set(test_mode).doc("Enable test mode that emulates PoE ports data"),
            clipp::option("-h", "--help").set(show_help).doc("Show help information"),
            clipp::any_other(wrong)
    );

    if (clipp::parse(argc, argv, cli) && wrong.empty()) {
        if (show_help) {
            cout << "Usage:\n" << clipp::make_man_page(cli, argv[0]) << '\n';
            return 0;
        }

        cout << "PoE daemon started, with monitor period: " << monitor_period_us << " us" << endl;
        if (test_mode) {
            cout << "Test mode enabled." << endl;
        }
    } else {
        for (const auto &arg : wrong) {
            cout << "'" << arg << "' is not a valid command line argument\n";
        }
        cout << "Usage:\n" << clipp::make_man_page(cli, argv[0]) << '\n';
        return -1;
    }

    /* Process UCI config */
    const char* config_name = "poed";
    const string config_path = "/etc/config/poed";

    initialize_logging(config_name, get_syslog_level("debug"));
    UciConfig config(config_name);
    if (!config.exists()) {
        syslog(LOG_ERR, "Configuration doesn't exist, create default one\n");
        create_default_config(config_path);
    }

    if (!config.import()) {
        syslog(LOG_ERR, "Configuration import error\n");
    }

    if (test_mode) {
        cout << "PoE daemon working in test mode, skip UCI config validation\n";
    } else {
        /* Check config */
        if (!validateUciConfig(config)) {
            syslog(LOG_ERR, "Configuration is not valid\n");
            return -1;
        }
    }

    auto sections = config.getSections();

    /* Reinitialize logger with proper log level */
    string log_level_str = sections["general"].at(0).options["log_level"];
    int log_level = get_syslog_level(log_level_str);

    closelog();
    initialize_logging(config_name, log_level);

//    cout << "Imported config:\n";
//    config.dump();

    syslog(LOG_INFO, "Daemon started with log level: %s", log_level_str.c_str());

    /* Parse uci config class into binary poe structures */
    syslog(LOG_INFO, "Parse UCI config into binary structures\n");
    int contr_ind = 0;
    vector<PoeController> controllers;
    for (auto& controller: sections["controller"]) {
        /* Get controller properties */
        PoeController c;
        c.path = controller.options["path"];
        c.total_budget = stod(controller.options["total_power_budget"]);
        c.ports.resize(stoi(controller.options["ports"]));

        /* Fill controller's ports vector with corresponded ports */
        for (auto port: sections["port"]) {
            if (stoi(port.options["controller"]) == contr_ind) {
                PoePort p;
                p.contr_path = c.path;
                p.name = port.options["name"];
                p.index = stoi(port.options["port_number"]);
                p.budget = stod(port.options["power_budget"]);
                p.mode = parsePoeMode(port.options["mode"]);
                p.priority = stoi(port.options["priority"]);

                /* Set system test mode flag to port */
                p.test_mode = test_mode;
                p.initSim();

                c.ports.at(p.index) = p;

                /* Set current port with corresponded mode */
                if (!p.setMode(p.mode)) {
                    syslog(LOG_ERR, "Can't set mode %s to port %d, of controller %s\n",
                           port.options["mode"].c_str(), p.index, p.contr_path.c_str());
                    return -1;
                }
            }
        }
        controllers.push_back(c);
        contr_ind++;
    }

    /* Controlling budgets */
    thread budgetThread(controlBudgetsWithSleep, std::ref(controllers), monitor_period_us);


    syslog(LOG_INFO, "Daemon is shutting down");
    closelog();

    budgetThread.join();
    return 0;
}

static void controlBudgetsWithSleep(vector<PoeController>& controllers, int sleep_time_us) {
    while (controlBudgets(controllers) >= 0) {
        /* Sleep for some time */
        usleep(sleep_time_us);
    }
}

static int controlBudgets(vector<PoeController>& controllers) {
    for (PoeController& controller: controllers) {
        if (!controller.getPortsData()) {
            syslog(LOG_ERR, "Can't acquire ports data\n");
            return -1;
        }

        /* Check ports budgets */
        double total_power = 0.0;
        for (auto& port: controller.ports) {
            if (port.power > port.budget) {
                syslog(LOG_INFO, "Port %d of controller %s has overbudget: %.2lf W, while %.2lf W is max. Turn off.\n",
                       port.index, port.contr_path.c_str(), port.power, port.budget);
                port.overbudget_flag = true;
                if (!port.powerOff()) {
                    syslog(LOG_ERR, "Can't power off PoE port %d of controller %s\n",
                           port.index, port.contr_path.c_str());
                    return -1;
                }
                continue;
            }
            total_power += port.power;
        }
        if (total_power > controller.total_budget) {
            /* Handle overbudget */
            syslog(LOG_INFO, "Ports of controller %s has overbudget: %.2lf, while %.2lf is max\n",
                   controller.path.c_str(), total_power, controller.total_budget);
            /* Turn off port with the lowest priority */
            PoePort lowest_prio_port = controller.getLowesPrioPort();
            syslog(LOG_INFO, "Port %d of controller %s has the lowest priority, turn it off\n",
                   lowest_prio_port.index, controller.path.c_str());
            lowest_prio_port.enable_perm = false;
            lowest_prio_port.overbudget_flag = true;
            if (!lowest_prio_port.powerOff()) {
                syslog(LOG_ERR, "Can't power off PoE port %d of controller %s\n",
                       lowest_prio_port.index, lowest_prio_port.contr_path.c_str());
                return -1;
            }
        } else if (total_power + POE_PWR_HYSTERESIS <= controller.total_budget) {
            /* Mark overbudget ports as permitted to enable */
            vector<PoePort*> overbudget_ports;
            for (auto& port: controller.ports) {
                if (port.state == PoeState::OPEN) {
                    if (port.overbudget_flag) {
                        port.enable_perm = true;
                    }
                } else {
                    if (port.overbudget_flag && port.enable_perm) {
                        overbudget_ports.push_back(&port);
                    }
                }
            }
            /* Check if some ports can be enabled, enable only 1 per cycle with highest prio */
            int max_prio = INT32_MAX;
            int max_prio_ind = -1;
            int ind_cnt = 0;
            for (const auto& port: overbudget_ports) {
                if (port->priority < max_prio) {
                    max_prio = port->priority;
                    max_prio_ind = ind_cnt;
                }
                ind_cnt++;
            }
            if (max_prio_ind >= 0) {
                syslog(LOG_INFO, "Enable %d port of controller %s\n",
                       overbudget_ports.at(max_prio_ind)->index,
                       overbudget_ports.at(max_prio_ind)->contr_path.c_str());
                if (!overbudget_ports.at(max_prio_ind)->powerOn()) {
                    syslog(LOG_ERR, "Can't power on PoE port %d of controller %s\n",
                           overbudget_ports.at(max_prio_ind)->index,
                           overbudget_ports.at(max_prio_ind)->contr_path.c_str());
                    return -1;
                }
                overbudget_ports.at(max_prio_ind)->overbudget_flag = false;
            }
        }
    }
    return 0;
}

static void getJsonFromControllers(vector<PoeController>& controllers) {

}