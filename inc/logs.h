#ifndef ROUTER_POED_LOGS_H
#define ROUTER_POED_LOGS_H

#include <syslog.h>

int get_syslog_level(const std::string& level);
void initialize_logging(const char* log_name, int log_level);

#endif //ROUTER_POED_LOGS_H
