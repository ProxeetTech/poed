#include "main_utils.h"
#include "logs.h"
#include "poe_controller.h"
#include "poe_simulator.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <nlohmann/json.hpp>
#include <unistd.h>

void controlBudgetsWithSleep(vector<PoeController>& controllers, int sleep_time_us) {
    while (controlBudgets(controllers) >= 0) {
        /* Sleep for some time */
        usleep(sleep_time_us);
    }
}

int controlBudgets(vector<PoeController>& controllers) {
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

        /* Workaround for turning on PoE ports in manual mode that turns off without load */
        for (auto& port: controller.ports) {
            if (port.enable_flag &&
                (port.mode == PoeMode::POE_48V || port.mode == PoeMode::POE_24V)) {
                if (!port.powerOn()) {
                    syslog(LOG_ERR, "Can't power on PoE port %d of controller %s\n",
                           port.index, port.contr_path.c_str());
                    return -1;
                }
            }
        }
    }
    return 0;
}

nlohmann::json getJsonFromControllers(vector<PoeController>& controllers) {
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
                    {"mode", poeModeToString(port.mode)},
                    {"load_class", port.load_type_str},
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

string getJsonFromControllersSer(vector<PoeController>& controllers) {
    return getJsonFromControllers(controllers).dump(4);  // "4" sets tabs for formatting output
}

void handleUnixSocketServer(const std::string& socket_path, vector<PoeController>& controllers) {
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
}

string requestFromUnixSocket(const string& socket_path, const string& message, int timeout_ms) {
    int sock;
    struct sockaddr_un server_addr{};

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
    struct pollfd pfd{};
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
        return buffer;  // Return the received response
    } else {
        close(sock);
        return "Failed to receive response";
    }
}