#include <unistd.h>
#include <string.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <sstream>
#include <thread>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <csignal>
#include <dirent.h>
#include <iomanip>
#include <pwd.h>
#include <regex>
#include <set>
#include <fcntl.h>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

const int PAGE_SIZE = 4096;
const int ARGS_SIZE = 30;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)));
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

Command::Command(const char* cmd_line) : line(string(cmd_line)){}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {}

ChpromptCommand::ChpromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line){
    istringstream iss((string(cmd_line)));
    string command, newPrompt;
    iss >> command >> newPrompt;
    if (newPrompt.empty()){
        this->prompt = "smash> ";
    }
    else{
        this->prompt = newPrompt + "> ";
    }
}

std::string ChangeDirCommand::lastPath;
ChangeDirCommand::ChangeDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}
ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}
GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}
JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line){}
ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line){}
QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line){}
KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line){}
AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}
UnAliasCommand::UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}
UnSetEnvCommand::UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}
WatchProcCommand::WatchProcCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}
ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line){}
DiskUsageCommand::DiskUsageCommand(const char *cmd_line) : Command(cmd_line) {}
WhoAmICommand::WhoAmICommand(const char *cmd_line) : Command(cmd_line) {}

void ChpromptCommand::execute(){
    SmallShell &smash = SmallShell::getInstance();
    smash.setPrompt(this->prompt);
}

void ShowPidCommand::execute() {
    cout << "smash pid is " << getpid() << endl;
}

void GetCurrDirCommand::execute() {
    char* cwd = getcwd(nullptr, 0);
    if (cwd == nullptr){
        perror("smash error: getcwd failed");
        return;
    }
    std::cout << cwd << std::endl;
    free(cwd);
}

void ChangeDirCommand::execute() {
    istringstream iss(this->line);
    string command, path, extra;
    iss >> command >> path >> extra;
    if (!extra.empty()){
        cerr << "smash error: cd: too many arguments" << endl;
    }
    else if (path == "-"){
        if (ChangeDirCommand::lastPath.empty()){
            cerr << "smash error: cd: OLDPWD not set" << endl;
        }
        else{
            string oldPath = string(getcwd(nullptr, 0));
            if (chdir(ChangeDirCommand::lastPath.c_str()) == -1){
                perror("smash error: chdir failed");
            }
            else {
                ChangeDirCommand::lastPath = oldPath;
            }
        }
    }
    else{
        string oldPath = string(getcwd(nullptr, 0));
        if (chdir(path.c_str()) == -1){
            perror("smash error: chdir failed");
        }
        else{
            ChangeDirCommand::lastPath = oldPath;
        }
    }
}

void QuitCommand::execute() {
    istringstream iss(this->line);
    string command, isKill;
    iss >> command >> isKill;

    if (!isKill.empty()){
        SmallShell &smash = SmallShell::getInstance();
        //call clean function
        cout << "smash: sending SIGKILL signal to " << smash.getJobsList().jobs.size() << " jobs:" << endl;
        for (const auto& pair : smash.getJobsList().jobs){
            cout << pair.second->getPID() << ": " << pair.second->getCommand() << endl;
            int success = kill(pair.second->getPID(), 9);
            if (success == -1){
                perror("smash error: kill failed");
                return;
            }
        }
    }
    exit(0);
}

void JobsList::printJobsList(){
    for (const auto& pair : this->jobs){
        cout << "[" << pair.first << "] " << pair.second->getCommand() << endl;
    }
}

void JobsList::removeFinishedJobs() {
    for (auto& pair : this->jobs){
        if (!pair.first || !pair.second){
            jobs.erase(pair.first);
            break;
        }
        int result = waitpid(pair.second->getPID(), nullptr, WNOHANG);
        if (result == -1){
            perror("smash error: waitpid failed");
        }
        else if (result > 0){
            this->jobs.erase(pair.first);
        }
    }
}

void JobsCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    smash.getJobsList().removeFinishedJobs();
    smash.getJobsList().printJobsList();
}

void ForegroundCommand::execute() {
    istringstream iss(this->line);
    string command, jobIdString, extra;
    iss >> command >> jobIdString >> extra;
    if (!extra.empty()){
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }
    for (char c : jobIdString){
        if (!isdigit(c)){
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }
    }
    SmallShell &smash = SmallShell::getInstance();
    if (jobIdString.empty()){
        if (smash.getJobsList().jobs.empty()){
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
        else{
            cout << prev(smash.getJobsList().jobs.end())->second->getCommand() << " " <<
                    prev(smash.getJobsList().jobs.end())->second->getPID() << endl;
            smash.setForeGroundPID(prev(smash.getJobsList().jobs.end())->second->getPID());
            int result = waitpid(prev(smash.getJobsList().jobs.end())->second->getPID(), nullptr, 0);
            smash.setForeGroundPID(-1);
            if (result == -1){
                perror("smash error: waitpid failed");
                return;
            }
            smash.getJobsList().jobs.erase(prev(smash.getJobsList().jobs.end())->second->getJobID());
        }
    }
    else{
        int currentJob = stoi(jobIdString);
        if (smash.getJobsList().jobs.find(currentJob) == smash.getJobsList().jobs.end()){
            cerr << "smash error: fg: job-id " << currentJob << " does not exist" << endl;
            return;
        }
        else{
            cout << smash.getJobsList().jobs[currentJob]->getCommand() << " " <<
                 smash.getJobsList().jobs[currentJob]->getPID() << endl;
            smash.setForeGroundPID(smash.getJobsList().jobs[currentJob]->getPID());
            int result = waitpid(smash.getJobsList().jobs[currentJob]->getPID(), nullptr, 0);
            smash.setForeGroundPID(-1);
            if (result == -1){
                perror("smash error: waitpid failed");
                return;
            }
            smash.getJobsList().jobs.erase(currentJob);
        }
    }
}

void KillCommand::execute() {
    istringstream iss(this->line);
    string command, sigType, jobID, extra;
    iss >> command >> sigType >> jobID >> extra;

    if (!extra.empty() || sigType.empty() || jobID.empty()){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    if (sigType[0] != '-'){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    for (int i = 1; i < sigType.size(); i++){
        if (!isdigit(sigType[i])){
            cerr << "smash error: kill: invalid arguments" << endl;
            return;
        }
    }
    for (char c : jobID){
        if (!isdigit(c)){
            cerr << "smash error: kill: invalid arguments" << endl;
            return;
        }
    }
    SmallShell &smash = SmallShell::getInstance();
    if (smash.getJobsList().jobs.count(stoi(jobID)) == 0){
        cerr << "smash error: kill: job-id " << jobID << " does not exist" << endl;
        return;
    }
    int success = kill(smash.getJobsList().jobs[stoi(jobID)]->getPID(), -stoi(sigType));
    cout << "signal number " << -stoi(sigType) << " was sent to pid "
         << smash.getJobsList().jobs[stoi(jobID)]->getPID() << endl;
    if (success == -1){
        perror("smash error: kill failed");
    }
}

string getAlias(map<int, vector<string>>& aliases, string name){
    for (auto& pair: aliases){
        if (pair.second[0] == name){
            return pair.second[1];
        }
    }
    return "";
}

void AliasCommand::execute() {
    istringstream iss(this->line);
    string command, rest, more, name, commandType;
    iss >> command >> rest;
    getline(iss, more);
    rest += more;

    SmallShell &smash = SmallShell::getInstance();
    if (rest.empty()){
        for (const auto& pair : smash.getAliases()){
            cout << pair.second[0] << "=" << pair.second[1] << endl;
        }
        return;
    }

    line = _trim(string(line));

    regex pattern("^alias [a-zA-Z0-9_]+='[^']*'$");
    if (!regex_match(line, pattern)){
        cerr << "smash error: alias: invalid alias format" <<  endl;
        return;
    }
    else{
        set<string> reservedKeywords = {"chprompt", "showpid", "pwd", "cd",
                                        "jobs", "fg", "quit", "kill", "alias",
                                        "unalias", "unsetenv", "watchproc",
                                        "du", "whoami", "netinfo"};

        size_t equal_pos = rest.find('=');
        name = rest.substr(0, equal_pos);
        commandType = rest.substr(equal_pos + 1);

        //getline(iss, commandType);
        if (commandType.empty() || commandType[0] != '\'' || commandType[commandType.size()-1] != '\''){
            cerr << "smash error: alias: invalid alias format" <<  endl;
            return;
        }
        commandType = commandType.substr(1, commandType.size()-2);
        string value = getAlias(smash.getAliases(), name);
        if (reservedKeywords.count(name) || !value.empty()){
            cerr << "smash error: alias: " << name <<
                 " already exists or is a reserved command" <<  endl;
            return;
        }
        else{
            smash.getAliases()[smash.getAliasID()] = {name, commandType};
            smash.incAliasID();
        }
    }
}

void UnAliasCommand::execute() {

    SmallShell &smash = SmallShell::getInstance();
    istringstream iss(this->line);
    string command, alias;
    iss >> command >> alias;
    if (alias.empty()){
        cerr << "smash error: unalias: not enough arguments" << endl;
        return;
    }
    else{
        while (!alias.empty()){
            string value = getAlias(smash.getAliases(), alias);
            if (value.empty()){
                cerr << "smash error: unalias: " << alias << " alias does not exist" << endl;
                return;
            }
            for (auto& pair : smash.getAliases()){
                if (!pair.first || pair.second.empty()){
                    smash.getAliases().erase(pair.first);
                    break;
                }
                if (pair.second[0] == alias){
                    smash.getAliases().erase(pair.first);
                }
            }
            if (!(iss >> alias)) {
                alias.clear();
            }
        }
    }
}

void UnSetEnvCommand::execute() {
    istringstream iss(this->line);
    string command, variable;
    iss >> command >> variable;
    if (variable.empty()){
        cerr << "smash error: unsetenv: not enough arguments" << endl;
        return;
    }
    else{
        string path = "/proc/" + to_string(getpid()) + "/environ";
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1){
            perror("smash error: open failed");
            return;
        }
        char buffer[8192];
        int len = read(fd, buffer, 8192);
        if (len == -1){
            perror("smash error: read failed");
            return;
        }
        if (close(fd) == -1){
            perror("smash error: close failed");
            return;
        }
        while (!variable.empty()){
            string target = variable + "=";
            int i = 0;
            while (i < len){
                char* word  = buffer + i;
                int wordLength = strlen(word);
                if (strncmp(word, target.c_str(), target.size()) == 0){
                    extern char** environ;
                    for (char **var = environ; *var != nullptr; var++){
                        if (strncmp(*var, target.c_str(), target.size()) == 0){
                            char **next;
                            for (next = var; *(next + 1) != nullptr; next++){
                                *next = *(next + 1);
                            }
                            *next = nullptr;
                        }
                    }
                    break;
                }
                i += wordLength + 1;
            }
            if (i >= len){
                cerr << "smash error: unsetenv: " << variable << " does not exist" << endl;
                return;
            }

            if (!(iss >> variable)) {
                variable.clear();
            }
        }
    }
}

string read_file(string& path){
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1){
        perror("smash error: open failed");
        return "";
    }
    char buffer[8192];
    int len = read(fd, buffer, sizeof(buffer));
    if (close(fd) == -1){
        perror("smash error: close failed");
        return "";
    }
    buffer[len] = '\0';
    return buffer;
}

void getProcUsage(pid_t pid, int& utime, int& stime){
    string path = "/proc/" + to_string(pid) + "/stat";
    string data = read_file(path);

    istringstream ss(data);
    string word;
    for (int i = 0; i < 13; i++){
        ss >> word;
    }
    ss >> utime >> stime;
}

int getTotalUsage(){
    string path = "/proc/stat";
    string data = read_file(path);

    istringstream ss(data);
    string cpu;
    ss >> cpu;
    int element, sum;
    while (ss >> element){
        sum += element;
    }

    return sum;

}

double getCPUPercentage(pid_t pid){

    int utime1, stime1;
    getProcUsage(pid, utime1, stime1);
    int total1 = getTotalUsage();

    this_thread::sleep_for(chrono::milliseconds(1000));

    int utime2, stime2;
    getProcUsage(pid, utime2, stime2);
    int total2 = getTotalUsage();
    return 100.0 * ((utime2 + stime2) - (utime1 + stime1))/(total2 - total1);
}

double getMemoryUsage(pid_t pid){
    string path = "/proc/" + to_string(pid) + "/statm";
    string data = read_file(path);

    istringstream ss(data);
    int total, resident;
    ss >> total >> resident;

    return (resident * PAGE_SIZE) / (1024.0 * 1024.0);
}

void WatchProcCommand::execute() {
    istringstream iss(this->line);
    string command, id, extra;
    iss >> command >> id >> extra;
    if (!extra.empty() || id.empty()){
        cerr << "smash error: watchproc: invalid arguments" << endl;
        return;
    }
    for (char c : id){
        if (!isdigit(c)){
            cerr << "smash error: watchproc: invalid arguments" << endl;
            return;
        }
    }

    string path = "/proc/" + id + "/status";
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1){
        cerr << "smash error: watchproc: pid " << id << " does not exist" << endl;
        return;
    }
    if (close(fd) == -1){
        perror("smash error: close failed");
        return;
    }

    string CPUPercentage, MemoryUsage;
    ostringstream stream1, stream2;
    stream1 << fixed << setprecision(1) << getCPUPercentage(stoi(id));
    stream2 << fixed << setprecision(1) << getMemoryUsage(stoi(id));

    cout << "PID: " << id << " | CPU Usage: " << stream1.str() <<
                "% | Memory Usage: " << stream2.str()  << " MB" << endl;

}

void ExternalCommand::execute() {

    SmallShell &smash = SmallShell::getInstance();

    bool isBackground = _isBackgroundComamnd(this->line.c_str());
    char *newLine = new char[this->line.length() + 1];
    strcpy(newLine, this->line.c_str());
    if (isBackground){
        _removeBackgroundSign(newLine);
    }
    string newLineStr = newLine;
    char **args = new char*[ARGS_SIZE];
    int length = _parseCommandLine(newLineStr.c_str(), args);

    int pid = fork();
    if (pid == -1){
        perror("smash error: fork failed");
        return;
    }
    if (pid == 0) {
        setpgrp();
        if (this->line.find('*') != string::npos || this->line.find('?') != string::npos){
            //string bashLine = "\"" + this->line + "\"";
            char* bashArgs[] = {"/bin/bash","-c", const_cast<char *>(this->line.c_str()), nullptr};
            execvp("/bin/bash", bashArgs);
            perror("smash error: execvp failed");
            exit(1);
        }
        else{
            execvp(args[0], args);
            perror("smash error: execvp failed");
            exit(1);
        }
    }
    else{
        if (isBackground){
            smash.getJobsList().removeFinishedJobs();
            int maxJobID = 0;
            for (const auto& pair : smash.getJobsList().jobs){
                if (pair.first > maxJobID){
                    maxJobID = pair.first;
                }
            }
            string commandName;
            if (this->command.empty()){
                commandName = this->line;
            }
            else{
                commandName = this->command;
            }
            auto* newJob = new JobsList::JobEntry(maxJobID + 1, pid, commandName);
            smash.getJobsList().getJobs().insert(make_pair(maxJobID + 1, newJob));
        }
        else{
            smash.setForeGroundPID(pid);
            int result = waitpid(pid, nullptr, 0);
            smash.setForeGroundPID(-1);
            if (result == -1){
                perror("smash error: waitpid failed");
                return;
            }
        }
    }
}

int Redirect(const char *line) {

    size_t redirect_pos = string(line).find('>');
    if (redirect_pos == string::npos){
        return -1;
    }

    string command, file;
    //command = string(line).substr(0, redirect_pos);
    int savedOut = dup(1);
    if (savedOut == -1){
        perror("smash error: dup failed");
        return -1;
    }
    int fd;

    if (string(line)[int(redirect_pos) + 1] != '>'){
        file = string(line).substr(redirect_pos + 1);
        file = _trim(file);
        fd = open(file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if (fd == -1){
            perror("smash error: open failed");
            return -1;
        }
    }
    else{
        file = string(line).substr(redirect_pos + 2);
        file = _trim(file);
        fd = open(file.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (fd == -1){
            perror("smash error: open failed");
            return -1;
        }
    }

    if (dup2(fd, 1) == -1){
        perror("smash error: dup2 failed");
        return -1;
    }
    if (close(fd) == -1){
        perror("smash error: close failed");
    }
    return savedOut;
}

int Piped(const char *line, int* myPipe){

    size_t pipe_pos = string(line).find('|');
    if (pipe_pos == string::npos){
        return -1;
    }

    if (pipe(myPipe) == -1){
        perror("smash error: pipe failed");
        return -1;
    }

    if (pipe_pos+1 != string(line).find('&')){
        return 1;
    }
    else{
        return 2;
    }
}

bool pathExists(string path){
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

long long getSize(string path){
    long long total = 0;

    struct stat dirStatus;
    if (stat(path.c_str(), &dirStatus) == 0){
        total += ((dirStatus.st_blocks * 512) / 1024);
    }

    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd == -1){
        perror("smash error: open failed");
        return 0;
    }

    char buffer[4096];
    int seeker;

    while ((seeker = syscall(SYS_getdents64, fd, buffer, 4096)) > 0){
        if (seeker == -1){
            perror("smash error: getdents64 failed");
            return 0;
        }
        int pos = 0;
        while (pos < seeker){
            struct dirent64 *dir = (struct dirent64 *)(buffer + pos);

            string name = dir->d_name;
            if (name == "." || name == ".."){
                pos += dir->d_reclen;
                continue;
            }

            string fullPath = path + "/" + name;
            struct stat status;
            if (stat(fullPath.c_str(), &status) == 0){
                if (S_ISDIR(status.st_mode)){
                    total += getSize(fullPath);
                }
                else if (S_ISREG(status.st_mode)){
                    total += ((status.st_blocks * 512) / 1024);
                }
            }
            pos += dir->d_reclen;
        }
    }

    if (close(fd) == -1){
        perror("smash error: close failed");
    }
    return total;
}

void DiskUsageCommand::execute() {
    istringstream iss(this->line);
    string command, path, extra;
    iss >> command >> path >> extra;
    if (!extra.empty()){
        cerr << "smash error: du: too many arguments" << endl;
        return;
    }

    long long totalKB;
    if (path.empty()){
        totalKB = getSize(".");
    }
    else{
        if (!pathExists(path)){
            cerr << "smash error: du: directory " << path << " does not exist" << endl;
            return;
        }
        totalKB = getSize(path);
    }
    cout << "Total disk usage: " << totalKB << " KB" << endl;
}

int getUID(){
    pid_t pid = getpid();
    string path = "/proc/" + to_string(pid) + "/status";
    string data = read_file(path);

    istringstream iss(data);
    string line;

    while(getline(iss, line)){
        if (line.find("Uid:") == 0){
            istringstream uidLine(line.substr(4));
            int uid;
            uidLine >> uid;
            return uid;
        }
    }
    return -1;
}

void WhoAmICommand::execute() {
    uid_t uid = getUID();

    string path = "/etc/passwd";
    string password = read_file(path);

    istringstream iss(password);
    string line;
    while(getline(iss, line)){
        istringstream lineStream(line);
        string user, x, uid2, gid, comment, home;
        getline(lineStream, user, ':');
        getline(lineStream, x, ':');
        getline(lineStream, uid2, ':');
        getline(lineStream, gid, ':');
        getline(lineStream, comment, ':');
        getline(lineStream, home, ':');

        if (stoi(uid2) == uid){
            cout << user << " " << home << endl;
            return;
        }
    }
}

SmallShell::SmallShell() {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {

    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    cmd_line = cmd_s.c_str();

    string realCommand;
    for (auto& pair : this->aliases){
        if (firstWord == pair.second[0]){
            realCommand = pair.second[1];
        }
    }
    string newCommand;
    if (!realCommand.empty()){
        newCommand = realCommand;
        istringstream iss((string(cmd_line)));
        string old, relevant;
        iss >> old;
        getline(iss, relevant);
        newCommand +=  (" " + relevant);

    }
    else{
        newCommand = cmd_line;
    }

    cmd_s = _trim(string(newCommand));
    firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    cmd_line = cmd_s.c_str();

    if ((firstWord == "chprompt") || (firstWord == "chprompt&")) {
        return new ChpromptCommand(cmd_line);
    }
    else if ((firstWord == "showpid") || (firstWord == "showpid&")) {
        return new ShowPidCommand(cmd_line);
    }
    else if ((firstWord == "pwd") || (firstWord == "pwd&")) {
        return new GetCurrDirCommand(cmd_line);
    }
    else if ((firstWord == "cd") || (firstWord == "cd&")) {
        return new ChangeDirCommand(cmd_line);
    }
    else if ((firstWord == "jobs") || (firstWord == "jobs&")) {
        return new JobsCommand(cmd_line, &this->jobsList);
    }
    else if ((firstWord == "fg") || (firstWord == "fg&")) {
        return new ForegroundCommand(cmd_line, &this->jobsList);
    }
    else if ((firstWord == "quit") || (firstWord == "quit&")) {
        return new QuitCommand(cmd_line, &this->jobsList);
    }
    else if ((firstWord == "kill") || (firstWord == "kill&")) {
        return new KillCommand(cmd_line, &this->jobsList);
    }
    else if ((firstWord == "alias") || (firstWord == "alias&")) {
        return new AliasCommand(cmd_line);
    }
    else if ((firstWord == "unalias") || (firstWord == "unalias&")) {
        return new UnAliasCommand(cmd_line);
    }
    else if ((firstWord == "unsetenv") || (firstWord == "unsetenv&")) {
        return new UnSetEnvCommand(cmd_line);
    }
    else if ((firstWord == "watchproc") || (firstWord == "watchproc&")) {
        return new WatchProcCommand(cmd_line);
    }
    else if (firstWord == "du") {
        return new DiskUsageCommand(cmd_line);
    }
    else if (firstWord == "whoami") {
        return new WhoAmICommand(cmd_line);
    }
    else {
      ExternalCommand* command = new ExternalCommand(cmd_line);
      return command;
    }
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here

    string original = cmd_line;
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    cmd_line = cmd_s.c_str();

    regex pattern("^alias [a-zA-Z0-9_]+='[^']*'$");
    if (regex_match(string(cmd_line), pattern)){
        auto* cmd = new AliasCommand(cmd_line);
        this->getJobsList().removeFinishedJobs();
        cmd->execute();
        return;
    }

    string realCommand;
    //string aliasName = "";
    for (auto& pair : this->aliases){
        if (firstWord == pair.second[0]){
            realCommand = pair.second[1];
            //aliasName = pair.second[0];
        }
    }
    string newCommand;
    if (!realCommand.empty()){
        newCommand = realCommand;
        istringstream iss((string(cmd_line)));
        string old, relevant;
        iss >> old;
        getline(iss, relevant);
        newCommand +=  (" " + relevant);
    }
    else{
        newCommand = cmd_line;
    }

    cmd_s = _trim(string(newCommand));
    firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    cmd_line = cmd_s.c_str();

    int myPipe[2];
    int pipeType = Piped(cmd_line, myPipe);

    if (pipeType != -1){
        string command1, command2;
        size_t pipe_pos = string(cmd_line).find('|');
        command1 = string(cmd_line).substr(0, pipe_pos);
        if (pipeType == 1){
            command2 = string(cmd_line).substr(pipe_pos + 1);
        }
        else{
            command2 = string(cmd_line).substr(pipe_pos + 2);
        }

        //First Command

        this->getJobsList().removeFinishedJobs();

        int pid1 = fork();
        if (pid1 == -1){
            perror("smash error: fork failed");
            return;
        }
        if (pid1 == 0){
            setpgrp();
            if (close(myPipe[0]) == -1){
                perror("smash error: close failed");
            }
            if (pipeType == 1){
                if (dup2(myPipe[1], 1) == -1){
                    perror("smash error: dup2 failed");
                }
            }
            else{
                if (dup2(myPipe[1], 2) == -1){
                    perror("smash error: dup2 failed");
                }
            }

            Command* cmd = CreateCommand(command1.c_str());
            cmd->execute();
            exit(0);
        }

        //Second Command

        this->getJobsList().removeFinishedJobs();

        int pid2 = fork();
        if (pid2 == -1){
            perror("smash error: fork failed");
            return;
        }
        if (pid2 == 0){
            setpgrp();
            if (close(myPipe[1]) == -1){
                perror("smash error: close failed");
            }

            if (dup2(myPipe[0], 0) == -1){
                perror("smash error: dup2 failed");
            }

            Command* cmd = CreateCommand(command2.c_str());
            cmd->execute();
            exit(0);
        }

        if (close(myPipe[0]) == -1){
            perror("smash error: close failed");
        }
        if (close(myPipe[1]) == -1){
            perror("smash error: close failed");
        }
        int result = waitpid(pid1, nullptr, 0);
        if (result == -1){
            perror("smash error: waitpid failed");
        }
        result = waitpid(pid2, nullptr, 0);
        if (result == -1){
            perror("smash error: waitpid failed");
        }

        return;
    }

    //Redirected

    int fdToClose = Redirect(cmd_line);
    string command = string(cmd_line);
    if (fdToClose != -1){
        size_t redirect_pos = command.find('>');
        command = command.substr(0, redirect_pos);
        cmd_line = command.c_str();
    }

    //Execute

    Command* cmd = CreateCommand(cmd_line);
    this->getJobsList().removeFinishedJobs();
    cmd->command = original;
    cmd->execute();

    if (fdToClose != -1){
        if (dup2(fdToClose, 1) == -1){
            perror("smash error: dup2 failed");
        }
        if (close(fdToClose) == -1){
            perror("smash error: close failed");
        }
    }
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}