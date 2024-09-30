#include <string>
#include "logs.h"

int get_syslog_level(const std::string& level) {
    if (level == "debug") return LOG_DEBUG;
    else if (level == "info") return LOG_INFO;
    else if (level == "notice") return LOG_NOTICE;
    else if (level == "warning") return LOG_WARNING;
    else if (level == "err") return LOG_ERR;
    else if (level == "crit") return LOG_CRIT;
    else if (level == "alert") return LOG_ALERT;
    else if (level == "emerg") return LOG_EMERG;
    else return LOG_INFO;
}

void initialize_logging(const char* log_name, int log_level) {
    openlog(log_name, LOG_PID | LOG_CONS, LOG_DAEMON);
    setlogmask(LOG_UPTO(log_level));
}
