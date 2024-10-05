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
#include "clipp.h"
#include "poe_controller.h"
#include "main_utils.h"
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <thread>
#include <iostream>


bool test_mode = false;

static void daemonize();

int main(int argc, char *argv[]) {
    /* Parse command line arguments */
    vector<string> wrong;
    bool show_help = false;
    bool get_data_flag = false;
    bool daemonize_flag = false;
    int monitor_period_us = 1000000;

    auto cli = (
            clipp::option("-p", "--monitor-period") &
            clipp::value("PoE ports monitoring period in us (default 1000000 us)", monitor_period_us),
            clipp::option("-t").set(test_mode).doc("Enable test mode that emulates PoE ports data"),
            clipp::option("-d").set(daemonize_flag).doc("Run in background as a daemon"),
            clipp::option("-g", "--get-all").set(get_data_flag).doc("Print all PoE data to stdout in "
                                                                "JSON format. The daemon must be "
                                                                "running to have this option workable"),
            clipp::option("-h", "--help").set(show_help).doc("Show help information"),
            clipp::any_other(wrong)
    );

    if (clipp::parse(argc, argv, cli) && wrong.empty()) {
        if (show_help) {
            cout << "Usage:\n" << clipp::make_man_page(cli, argv[0]) << '\n';
            return 0;
        }

        if (daemonize_flag) {
            daemonize();
        }

        if (!get_data_flag) cout << "PoE daemon started, with monitor period: " << monitor_period_us << " us" << endl;
        if (test_mode) {
            syslog(LOG_INFO, "Test mode enabled\n");
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
        syslog(LOG_INFO, "PoE daemon working in test mode, skip UCI config validation\n");
    } else {
        /* Check config */
        if (!validateUciConfig(config)) {
            syslog(LOG_ERR, "Configuration is not valid\n");
            return -1;
        }
    }

    auto sections = config.getSections();

    /* Get unix socket server options */
    string unix_socket_enable = sections["general"].at(0).options["unix_socket_enable"];
    string unix_socket_path = sections["general"].at(0).options["unix_socket_path"];

    /* Reinitialize logger with proper log level */
    string log_level_str = sections["general"].at(0).options["log_level"];
    int log_level = get_syslog_level(log_level_str);

    closelog();
    initialize_logging(config_name, log_level);

    /* Check if the daemon is already running */
    vector<pid_t> procd_pids = getProcessIdsByName("procd");
    if (procd_pids.empty()) {
        syslog(LOG_INFO, "procd pid wasn't found\n");
    } else {
        syslog(LOG_INFO, "procd pid: %d\n", procd_pids.at(0));
    }

    vector<pid_t> pids = getProcessIdsByName("poed");
    syslog(LOG_DEBUG, "Detected poed pids:\n");
    for (auto pid: pids) {
        pid_t ppid = getppid();
        syslog(LOG_DEBUG, "pid: %d, ppid: %d, ppid name: %s\n", pid, ppid, getProcessName(ppid).c_str());
    }

    string command = "pgrep -x poed | grep -v " + to_string(getpid()) + " > /dev/null 2>&1";
    syslog(LOG_DEBUG, "Run: %s to check if the daemon is already running\n", command.c_str());
    int result = system(command.c_str());
    if (!result) {
        /* Check if new process was started with -g flag */
        syslog(LOG_DEBUG, "Running instance found in the processes: %d\n", result);
        if (get_data_flag) {
            syslog(LOG_DEBUG, "This instance started with flag '--get-all'\n");
            if (unix_socket_enable != "1") {
                syslog(LOG_ERR, "Using of unix socket server is disabled in config file, exiting\n");
                cerr << "Using of unix socket server is disabled in config file, exiting\n";
                return -1;
            }
            nlohmann::json msg = {
                    {"msg_type", "request"},
                    {"data", "get_all"}
            };

            string response = requestFromUnixSocket(unix_socket_path, msg.dump(4), 2000);
            syslog(LOG_DEBUG, "Requested data result: %s\n", response.c_str());
            cout << response << "\n";
            return 0;
        } else {
            syslog(LOG_ERR, "Attempt to start the instance of the daemon while it's already working\n");
            cerr << "Attempt to start the instance of the daemon while it's already working\n";
            return -1;
        }
    }
    if (get_data_flag) {
        syslog(LOG_ERR, "Running instance of daemon was not found, start it before request PoE data\n");
        cerr << "Running instance of daemon was not found, start it before request PoE data\n";
        return -1;
    }

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
                p.priority = stoi(port.options["priority"]);

                /* Set system test mode flag to port */
                p.test_mode = test_mode;

                /* Set current port with corresponded mode */
                if (!p.setMode(parsePoeMode(port.options["mode"]))) {
                    syslog(LOG_ERR, "Can't set mode %s to port %d, of controller %s\n",
                           port.options["mode"].c_str(), p.index, p.contr_path.c_str());
                    return -1;
                }

                p.initSim();

                c.ports.at(p.index) = p;
            }
        }
        controllers.push_back(c);
        contr_ind++;
    }

    /* Controlling budgets */
    thread budgetThread(controlBudgetsWithSleep, std::ref(controllers), monitor_period_us);
    if (unix_socket_enable == "1") {
        thread unixSocketServerThread(handleUnixSocketServer, unix_socket_path, std::ref(controllers));
        unixSocketServerThread.join();
    }
    budgetThread.join();

    syslog(LOG_INFO, "Daemon is shutting down");
    closelog();

    return 0;
}

static void daemonize() {
    /* Fork off the parent process */
    pid_t pid = fork();

    /* If fork failed, exit the parent process */
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    /* If we got a good PID, terminate the parent */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* Create a new session and set the child as session leader */
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    /* Fork again to ensure the daemon cannot acquire a controlling terminal */
    pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    /* Terminate the second parent */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* Change the working directory to root */
    if (chdir("/") < 0) {
        exit(EXIT_FAILURE);
    }

    /* Close standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}