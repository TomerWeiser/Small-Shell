#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    // TODO: Add your implementation
    cout << "smash: got ctrl-C" << endl;
    SmallShell &smash = SmallShell::getInstance();
    if (smash.getForeGroundPID() != -1){
        int success = kill(smash.getForeGroundPID(), 9);
        if (success == 0){
            cout << "smash: process " << smash.getForeGroundPID() << " was killed" << endl;
        }
        else {
            perror("smash error: kill failed");
        }
    }
}
