//Thanks for https://github.com/hoff-dot-world giving inspiration for this project. Core of this project was made using his tutorial on YouTube: https://www.youtube.com/watch?v=9nDYYc_7sKs

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/inotify.h>
#include <libnotify/notify.h>
#include <time.h>

#define EXIT_SUCCESFULL 0
#define EXIT_ARGS_MISSING 1
#define EXIT_PATH_NULL 2
#define EXIT_INOTIFY_INIT 3
#define EXIT_WD_CALLOC 4
#define EXIT_WD_ADD 5
#define EXIT_READ 6
#define EXIT_LIBNOTIFY_INIT 7
#define EXIT_LOGFILE_OPEN 8

static void event_handler(int fileDescriptor, int *watchDescriptor, int argc, char* argv[], char *pathToFile, FILE *logFile);
void signal_handler(int signal);
void send_logfile_via_ssh(const char *logFilePath, const char *remoteUser, const char *remoteHost, const char *remotePath);


int fileDescriptor;
int *watchDescriptor;
FILE *logFile;
const char *logFilePath = "";
const char *remoteUser = "";
const char *remoteHost = "";
const char *remotePath = "";


int main(int argc, char** argv) {
    char *pathToFile = NULL;
    char *pathToken = NULL;
    bool libnotifyStatus = false;
    char *program_name = "watcher";

    const uint32_t mask = IN_ACCESS | IN_CLOSE_WRITE | IN_CREATE | IN_DELETE |  IN_MODIFY | IN_MOVE_SELF;
    
    if (argc < 2){
        fprintf(stderr, "USAGE: %s PATH [PATH/TO/FILE/]\n", argv[0]);
        exit(EXIT_ARGS_MISSING);
    }

    logFile = fopen("../watcher_log.txt", "a");
    if (logFile == NULL) {
        fprintf(stderr, "Error opening logfile");
        exit(EXIT_LOGFILE_OPEN);
    }

    pathToFile = (char *)malloc(sizeof(char)*(strlen(argv[1]) + 1));
    strcpy(pathToFile, argv[1]);

    pathToken = strtok(pathToFile, "/");
    while(pathToken != NULL) {
        pathToFile = pathToken,
        pathToken = strtok(NULL, "/");
    }

    if (pathToFile == NULL) {
        fprintf(stderr, "Path is null");
        exit(EXIT_PATH_NULL);
    }

    libnotifyStatus = notify_init(program_name);
    if (!libnotifyStatus) {
        fprintf(stderr, "Error initializing libnotify\n");
        exit(EXIT_LIBNOTIFY_INIT);
    }

    fileDescriptor = inotify_init();
    if (fileDescriptor == -1) {
        fprintf(stderr, "Error initializing inotify\n");
        exit(EXIT_INOTIFY_INIT);
    }

    watchDescriptor = calloc(argc, sizeof(int));
    if (watchDescriptor == NULL) {
        fprintf(stderr, "Error allocating memory\n");
        exit(EXIT_WD_CALLOC);
    }

    for (int i = 1; i < argc; i++) {
        watchDescriptor[i] = inotify_add_watch(fileDescriptor, argv[i], mask);
        if (watchDescriptor[i] == -1) {
            fprintf(stderr, "Cannot watch or could not find %s\n", argv[i]);
            exit(EXIT_WD_ADD);
        }
    }

    signal(SIGABRT, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while(true) {
        printf("Listening for events... \n");
        event_handler(fileDescriptor, watchDescriptor, argc, argv, pathToFile, logFile);
    }
}

static void event_handler(int fileDescriptor, int *watchDescriptor, int argc, char* argv[], char *pathToFile, FILE *logFile){

    char buffer[4096];
    const struct inotify_event *event;
    int readLenght;
    char *notification;
    
    NotifyNotification *notificationHandle;

    uint32_t lastEventMask;
    char lastEventName[256];

    readLenght = read(fileDescriptor, buffer, sizeof(buffer));
    if (readLenght < 0) {
        fprintf(stderr, "Reader error");
        exit(EXIT_READ);
    }
    for(char *buffPtr = buffer; buffPtr < buffer + readLenght; buffPtr += sizeof(struct inotify_event) + event->len) {

        notification = NULL;
        event = (const struct inotify_event *) buffPtr;
        time_t current_time = time(NULL);
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&current_time));

        if (event->mask == lastEventMask && strcmp(event->name, lastEventName) == 0) {
            continue;
        }

        lastEventMask = event->mask;
        strncpy(lastEventName, event->name, sizeof(lastEventName) - 1);
        
        if ((event->mask & IN_ACCESS) && (event->len == 0)) {
            notification = "File directory was accessed";
            fprintf(logFile, "[%s] File directory was accessed\n", timeStr);
            continue;
        }

        if (event->mask & IN_ACCESS){
            notification = "File was accessed\n";
            fprintf(logFile, "[%s] File %s was accessed\n", timeStr, lastEventName);
        }

        if (event->mask & IN_CLOSE_WRITE){
            notification = "File was written and closed\n";
            fprintf(logFile, "[%s] File %s was written and closed\n", timeStr, lastEventName);

        }
        if (event->mask & IN_CREATE){
            notification = "File was created\n";
            fprintf(logFile, "[%s] File %s was created\n", timeStr, lastEventName);
        }

        if (event->mask & IN_DELETE){
            notification = "File was deleted\n";
            fprintf(logFile, "[%s] File %s was deleted\n", timeStr, lastEventName);
        }

        if (event->mask & IN_MODIFY){
            notification = "File was modified\n";
            fprintf(logFile, "[%s] File %s was modified\n", timeStr, lastEventName);
        }

        if (event->mask & IN_MOVE_SELF){
            notification = "File was moved\n";
            fprintf(logFile, "[%s] File %s was moved\n", timeStr, lastEventName);
        }

        if(notification == NULL) {
            continue;
        }
    
        notificationHandle = notify_notification_new(pathToFile, notification, "default");
        if(notificationHandle == NULL) {
            fprintf(stderr, "Error creating a notification");
            continue;
        }

        fflush(logFile);
        notify_notification_show(notificationHandle, NULL);
    }
}

void signal_handler(int signal) {
    
    printf("\nShutdown signal received, shutting down...\n");

    close(fileDescriptor);
    fclose(logFile);
    send_logfile_via_ssh(logFilePath, remoteUser, remoteHost, remotePath);
    exit(EXIT_SUCCESFULL);
}

void send_logfile_via_ssh(const char *logFilePath, const char *remoteUser, const char *remoteHost, const char *remotePath) {
    char ssh_command[512];

    snprintf(ssh_command, sizeof(ssh_command), "scp %s %s@%s:%s", logFilePath, remoteUser, remoteHost, remotePath);

    int result = system(ssh_command);
    if(result == 0) {
        printf("Log file succesfully sent to  %s@%s\n", remoteUser, remoteHost);
    } else {
        fprintf(stderr, "Failed to send log file! SCP command exited with status %d\n", result);
    }
}
