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
