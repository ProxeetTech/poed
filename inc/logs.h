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

#ifndef ROUTER_POED_LOGS_H
#define ROUTER_POED_LOGS_H

#include <syslog.h>

int get_syslog_level(const std::string& level);
void initialize_logging(const char* log_name, int log_level);

#endif //ROUTER_POED_LOGS_H
