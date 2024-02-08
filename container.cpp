#include <sys/wait.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fstream>

#define STACK 8192
#define CONTAINER_FLAGS CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD
#define CGROUP_PROCS "/sys/fs/cgroup/pids/cgroup.procs"
#define CGROUP_LIMIT_PROCS "/sys/fs/cgroup/pids/pids.max"
#define NOTIFY_ON_RELEASE "/sys/fs/cgroup/pids/notify_on_release"
#define PERMISSIONS 0755

using namespace std;
void handle_namespaces(char *host_name, char* root);
void handle_cgroups(const char* num_proc);
void write_to_file(const char* path, const char* str);
int container_entry(void *args);
void delete_cgroups(char* root);
void* get_stack();

// ================================================================= //

void print_error_and_exit(const string &error_msg){
    cerr << "system error: " << error_msg << endl;
    exit(EXIT_FAILURE);
}

void delete_cgroups(char* root){
    string buf("rm -rf ");
    buf.append(root);
    buf.append("/sys/fs");
    system(buf.c_str());
}

void* get_stack(){
    void* stack = malloc(STACK);
    if (stack == nullptr){
        print_error_and_exit("failed to allocate memory for stack.");
    }
    return (char*)stack;
}

int container_entry(void *args)
{
    char** args_ = (char**) args;
    handle_namespaces(args_[1], args_[2]);
    handle_cgroups(args_[3]);
    if (mount("proc", "/proc", "proc", 0, nullptr) < 0){
        print_error_and_exit("failed to mount /proc directory.");
    }

    if ((execvp(args_[4], args_ + 4) < 0)) {
        print_error_and_exit("failed to execute given program.");
    }

    return EXIT_SUCCESS;
}

void handle_namespaces(char *host_name, char* root){
    if (sethostname(host_name, strlen(host_name)) < 0){
        print_error_and_exit("failed to set host name.");
    }
    if (chroot(root) < 0) {
        print_error_and_exit("failed to set root directory.");
    }
    if (chdir("/") < 0){
        print_error_and_exit("failed to define new root directory");
    }
}

void handle_cgroups(const char* num_proc){
    mkdir( "/sys/", PERMISSIONS);
    mkdir( "/sys/fs/", PERMISSIONS);
    mkdir( "/sys/fs/cgroup/", PERMISSIONS);
    mkdir( "/sys/fs/cgroup/pids/", PERMISSIONS);
    write_to_file(CGROUP_PROCS, to_string(getpid()).c_str());
    write_to_file(CGROUP_LIMIT_PROCS, num_proc);
    write_to_file(NOTIFY_ON_RELEASE, "1");
}

void write_to_file(const char* path, const char* str){
    ofstream file (path);
    if (file.is_open()){
        file << str;
        file.close();
        string command = "chmod 0755 ";
        command.append(path);
        system(command.c_str());
    } else {
        print_error_and_exit("failed to open file.");
    }
}

// ================================================================= //

int main(int argc, char **argv) {
    void* stack = get_stack();
    if (clone(container_entry, (char*)stack + STACK, CONTAINER_FLAGS, argv) < 0){
        print_error_and_exit("failed to clone container.");
    }
    wait(nullptr);
    delete_cgroups(argv[2]);
    string path(argv[2]);
    path.append("/proc");
    if (umount(path.c_str()) < 0){
        print_error_and_exit("failed to unmount /proc directory");
    }
    free( stack);
    return EXIT_SUCCESS;
}