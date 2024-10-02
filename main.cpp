#include "utils.h"
#include "logs.h"
#include "clipp.h"
#include "poe_controller.h"
#include "poe_simulator.h"
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

bool test_mode = false;

static nlohmann::json getJsonFromControllers(vector<PoeController>& controllers);
static string getJsonFromControllersSer(vector<PoeController>& controllers);
static void handleUnixSocketServer(const std::string& socket_path, vector<PoeController>& controllers);
static int controlBudgets(vector<PoeController>& controllers);
static void controlBudgetsWithSleep(vector<PoeController>& controllers, int sleep_time_us);
static string requestFromUnixSocket(const string& socket_path, const string& message, int timeout_ms);

int main(int argc, char *argv[]) {
    /* Parse command line arguments */
    vector<string> wrong;
    bool show_help = false;
    bool get_data_flag = false;
    int monitor_period_us = 1000000;

    auto cli = (
            clipp::option("-p", "--monitor-period") &
            clipp::value("PoE ports monitoring period in us (default 1000000 us)", monitor_period_us),
            clipp::option("-t").set(test_mode).doc("Enable test mode that emulates PoE ports data"),
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
                p.initSim();

                /* Set current port with corresponded mode */
                if (!p.setMode(parsePoeMode(port.options["mode"]))) {
                    syslog(LOG_ERR, "Can't set mode %s to port %d, of controller %s\n",
                           port.options["mode"].c_str(), p.index, p.contr_path.c_str());
                    return -1;
                }

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

static nlohmann::json getJsonFromControllers(vector<PoeController>& controllers) {
    nlohmann::json j_controllers = nlohmann::json::array();

    for (const auto& controller : controllers) {
        nlohmann::json j_ports = nlohmann::json::array();

        double total_power = 0.0;

        for (const auto& port : controller.ports) {
            total_power += port.power;

            nlohmann::json j_port = {
                    {"name", port.name},
                    {"index", port.index},
                    {"priority", port.priority},
                    {"voltage", port.voltage},
                    {"current", port.current},
                    {"power", port.power},
                    {"budget", port.budget},
                    {"state", port.state_str},
                    {"mode", port.mode_str},
                    {"mode", port.load_type_str},
                    {"enable_flag", port.enable_flag},
                    {"overbudget_flag", port.overbudget_flag}
            };

            j_ports.push_back(j_port);
        }

        nlohmann::json j_controller = {
                {"total_budget", controller.total_budget},
                {"total_power", total_power},
                {"ports", j_ports}
        };

        j_controllers.push_back(j_controller);
    }

    return j_controllers;
}

static string getJsonFromControllersSer(vector<PoeController>& controllers) {
    return getJsonFromControllers(controllers).dump(4);  // "4" sets tabs for formatting output
}

static void handleUnixSocketServer(const std::string& socket_path, vector<PoeController>& controllers) {
    int server_sock, client_sock;
    struct sockaddr_un server_addr;

    /* Create a UNIX socket */
    if ((server_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        syslog(LOG_ERR, "Failed to create socket\n");
        return;
    }

    /* Remove existing socket file if it exists */
    unlink(socket_path.c_str());

    /* Set socket address parameters */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path.c_str(), sizeof(server_addr.sun_path) - 1);

    /* Bind the socket to the specified path */
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Failed to bind socket\n");
        close(server_sock);
        exit(-1);
    }

    /* Start listening for incoming connections (max queue: 1) */
    if (listen(server_sock, 1) == -1) {
        syslog(LOG_ERR, "Failed to listen on socket\n");
        close(server_sock);
        return;
    }

    syslog(LOG_INFO, "Listening on UNIX socket: %s\n", socket_path.c_str());

    for (;;) {
        /* Accept an incoming connection */
        syslog(LOG_INFO, "Waiting for new connection\n");
        if ((client_sock = accept(server_sock, NULL, NULL)) == -1) {
            syslog(LOG_ERR, "Failed to accept connection\n");
            close(server_sock);
            return;
        }

        /* Handle messages from the client */
        char buffer[1024];
        for (;;) {
            ssize_t num_bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (num_bytes > 0) {
                buffer[num_bytes] = '\0'; /* Add null terminator to the received string */

                string received_message(buffer);
                syslog(LOG_DEBUG, "Received %s\n", received_message.c_str());

                /* Parse received json */
                nlohmann::json msg;
                nlohmann::json j_response;
                try {
                    msg = nlohmann::json::parse(received_message);
                }
                catch (const nlohmann::json::exception& e) {
                    syslog(LOG_ERR, "Bad request, JSON parsing error: %s\n", e.what());
                    j_response = {
                            {"msg_type", "response"},
                            {"data", ""},
                            {"error_msg", "JSON parsing error"}
                    };
                    string response_message = j_response.dump(4);
                    send(client_sock, response_message.c_str(), response_message.size(), 0);
                    continue;
                }

                string msg_type;
                try {
                    msg_type = msg["msg_type"];
                }
                catch (const nlohmann::json::exception& e) {
                    syslog(LOG_ERR, "Bad request, msg doesn't have 'msg_type' field\n");
                    j_response = {
                            {"msg_type", "response"},
                            {"data", ""},
                            {"error_msg", "Field 'msg_type' wasn't found"}
                    };
                    string response_message = j_response.dump(4);
                    send(client_sock, response_message.c_str(), response_message.size(), 0);
                    continue;
                }

                if (msg_type != "request") {
                    syslog(LOG_ERR, "Bad request, received message with 'msg_type': %s\n",
                           msg_type.c_str());
                    j_response = {
                            {"msg_type", "response"},
                            {"data", ""},
                            {"error_msg", "Wrong 'msg_type'"}
                    };
                    string response_message = j_response.dump(4);
                    send(client_sock, response_message.c_str(), response_message.size(), 0);
                    continue;
                }

                string data;
                try {
                    data = msg["data"];
                }
                catch (const nlohmann::json::exception& e) {
                    syslog(LOG_ERR, "Bad request, msg doesn't have 'data' field\n");
                    j_response = {
                            {"msg_type", "response"},
                            {"data", ""},
                            {"error_msg", "Field 'data' wasn't found"}
                    };
                    string response_message = j_response.dump(4);
                    send(client_sock, response_message.c_str(), response_message.size(), 0);
                    continue;
                }

                /* Check the received command */
                if (data == "get_all") {
                    /* Send the response message */
                    nlohmann::json poe_data = getJsonFromControllers(controllers);
                    j_response = {
                            {"msg_type", "response"},
                            {"data", poe_data},
                            {"error_msg", ""}
                    };
                } else {
                    j_response = {
                            {"msg_type", "response"},
                            {"data", ""},
                            {"error_msg", "Unrecognized command"}
                    };
                }
                string response_message = j_response.dump(4);
                send(client_sock, response_message.c_str(), response_message.size(), 0);
            } else {
                syslog(LOG_ERR, "Failed to receive data\n");
                break;
            }
        }
    }

//    /* Close the client and server sockets */
//    close(client_sock);
//    close(server_sock);
//
//    /* Remove the socket file */
//    unlink(socket_path.c_str());
}

static string requestFromUnixSocket(const string& socket_path, const string& message, int timeout_ms) {
    int sock;
    struct sockaddr_un server_addr;

    /* Create a socket */
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return "Failed to create socket";
    }

    /* Set up the server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path.c_str(), sizeof(server_addr.sun_path) - 1);

    /* Connect to the server */
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        close(sock);
        return "Failed to connect to socket";
    }

    /* Send the message to the server */
    if (send(sock, message.c_str(), message.size(), 0) == -1) {
        close(sock);
        return "Failed to send message";
    }

    /* Wait for the response with a timeout */
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN;  // We are interested in reading

    int poll_result = poll(&pfd, 1, timeout_ms);

    if (poll_result == 0) {
        close(sock);
        return "Timeout waiting for response";
    } else if (poll_result == -1) {
        close(sock);
        return "Error while waiting for response";
    }

    /* Receive the response from the server */
    char buffer[8192];
    ssize_t num_bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (num_bytes > 0) {
        buffer[num_bytes] = '\0';  // Null-terminate the received string
        close(sock);
        return string(buffer);  // Return the received response
    } else {
        close(sock);
        return "Failed to receive response";
    }
}