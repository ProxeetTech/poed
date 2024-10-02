#ifndef POED_MAIN_UTILS_H
#define POED_MAIN_UTILS_H

#include "nlohmann/json.hpp"
#include "utils.h"
#include "poe_controller.h"

nlohmann::json getJsonFromControllers(vector<PoeController>& controllers);
string getJsonFromControllersSer(vector<PoeController>& controllers);
void handleUnixSocketServer(const std::string& socket_path, vector<PoeController>& controllers);
int controlBudgets(vector<PoeController>& controllers);
void controlBudgetsWithSleep(vector<PoeController>& controllers, int sleep_time_us);

#endif //POED_MAIN_UTILS_H
