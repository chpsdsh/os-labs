#include "proxy.h"

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

    int rc = proxy_run(port, workers);

    return rc;
}
