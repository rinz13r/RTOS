#include <iostream>
#include <vector>
#include <unistd.h>
#include <signal.h>

std::vector<int> pids;

void signal_handler (int signum) {
    for (auto & pid : pids) {
        std::cout << pid << std::endl;
        kill (pid, SIGINT);
    }
}

int main (int argc, char ** argv) {
    int nclients = atoi (argv[1]);
    for (int i = 0; i < nclients; ++i) {
        int pid = fork ();
        if (pid == 0) {
            execl ("bin/client", "bin/client", "0.0.0.0", argv[2], "Vijay", "0", NULL);
        } else {
            pids.push_back (pid);
        }
        usleep (20000);
    }
    signal (SIGINT, signal_handler);
    sleep (120);
}
