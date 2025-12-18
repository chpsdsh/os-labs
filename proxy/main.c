#include "proxy.h"
#include "logger.h"

#include <stdlib.h>
#include <signal.h>



int main(int argc, char **argv)
{
    int port    = 4242;
    int workers = 8;

    if (argc > 1)
        port = atoi(argv[1]);
    if (argc > 2)
        workers = atoi(argv[2]);

    signal(SIGPIPE, SIG_IGN);

    logger_init(LOG_DEBUG);
    

    log_info("proxy starting");
    log_info("listen port=%d, workers=%d", port, workers);

    int rc = proxy_run(port, workers);

    log_info("proxy shutting down, rc=%d", rc);

    logger_finalize();
    return rc;
}
