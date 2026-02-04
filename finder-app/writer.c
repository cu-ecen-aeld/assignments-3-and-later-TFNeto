#include <stdio.h>
#include <syslog.h>

int main(int argc, char *argv[]) {

    // Setup syslog with LOG_USER facility
    openlog("writer_app", 0, LOG_USER);
    syslog(LOG_INFO, "Writer application started.");

    //Check for correct number of arguments
    if (argc != 3) {
        syslog(LOG_ERR, "Wrong number of arguments. Expected 2, got %d.", argc - 1);
        closelog();
        return 1;

    }

    //write to file and Debug log
    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
    
    FILE *file = fopen(argv[1], "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Failed to open file %s for writing.", argv[1]);
        closelog();
        return 1;
    }
    fprintf(file, "%s", argv[2]);
    fclose(file);

    closelog();
    return 0;
}