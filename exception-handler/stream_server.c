#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define MAX_EXCEPTIONS 100
#define EXCEPTION_THRESHOLD 5
#define TIME_WINDOW_SECONDS 3600 

#define PORT "8080"
#define BUF_SIZE 4096
#define BACKLOG 10

time_t exception_times[MAX_EXCEPTIONS];
int exception_count = 0;

void add_exception_time(time_t now) {
    // Remove expired timestamps
    int i, j = 0;
    for (i = 0; i < exception_count; i++) {
        if (difftime(now, exception_times[i]) <= TIME_WINDOW_SECONDS) {
            exception_times[j++] = exception_times[i];
        }
    }
    exception_count = j;

    // Add new exception time
    if (exception_count < MAX_EXCEPTIONS) {
        exception_times[exception_count++] = now;
    }
}

int should_notify(time_t now) {
    add_exception_time(now);
    return exception_count >= EXCEPTION_THRESHOLD;
}

void escape_json_string(const char* input, char* output, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < max_len - 2; i++) {
        switch (input[i]) {
            case '\"': if (j + 2 < max_len) output[j++] = '\\', output[j++] = '\"'; break;
            case '\\': if (j + 2 < max_len) output[j++] = '\\', output[j++] = '\\'; break;
            case '\n': if (j + 2 < max_len) output[j++] = '\\', output[j++] = 'n'; break;
            default:
                if (j + 1 < max_len && (unsigned char)input[i] >= 32 && input[i] != '\r')
                    output[j++] = input[i];
                break;
        }
    }
    output[j] = '\0';
}

void send_slack_notification(const char* message) {
    const char *webhook_url = getenv("SLACK_WEBHOOK_URL");
    char escaped[BUF_SIZE];
    escape_json_string(message, escaped, sizeof(escaped));

    // Calculate needed size
    size_t estimated_len = strlen(escaped) + strlen(webhook_url) + 200;
    char *command = malloc(estimated_len);
    if (!command) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    snprintf(command, estimated_len,
        "curl -s -X POST -H 'Content-type: application/json' "
        "--data '{\"text\":\"%s\"}' \"%s\"",
        escaped, webhook_url);

    printf("Sending to Slack: %s\n", escaped);
    int result = system(command);
    if (result != 0) {
        fprintf(stderr, "Slack notification failed (return code: %d)\n", result);
    } else{
        printf("\nSlack alert sent successfully\n");
    }

    free(command);
}
// Function below taken from https://beej.us/guide/bgnet/html/split/client-server-background.html#a-simple-stream-server
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int socket_fd, new_fd; // Listen on socket_fd, new connection on new_fd
    struct addrinfo hints, *server_info, *p;
    struct sockaddr_storage their_addr; // connector's address info
    socklen_t sin_size;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    FILE *fptr;
    int warning_count = 0 , error_count = 0, critical_count = 0;
    int n;
    time_t now;
    struct tm *t;
    char timestamp[64];
    char final_message[BUF_SIZE];
    char buffer[BUF_SIZE] = { 0 };
    char *response = "Hello from C server\n";
   
    // Set and then apply the EST timezone
    setenv("TZ", "EST5EDT", 1);  
    tzset();

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = server_info; p != NULL; p = p->ai_next) {
        if ((socket_fd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_fd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(server_info); // all done with this structure

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {
        sin_size = sizeof(their_addr);
        new_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof(s));

        printf("server : got connection from %s\n", s);
        
        // -1 because of null terminator
        read(new_fd, buffer, BUF_SIZE - 1);
        // Remove newline characters
        buffer[strcspn(buffer, "\r\n")] = 0;
        printf("Received from Java: %s\n", buffer);
        send(new_fd, response, strlen(response), 0);

        now = time(NULL);
        t = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S %Z", t);

        n = snprintf(final_message, sizeof(final_message), "[%s] %s", timestamp, buffer);
        if (n >= sizeof(final_message)) {
            fprintf(stderr, "Warning: final_message was truncated\n");
        }

        // Open a file in append mode
        fptr = fopen("TESTdb.txt", "a");
        if (!fptr) {
            perror("fopen");
        } else {
            // Append some text to the file
            fprintf(fptr, "%s\n", final_message);
            fclose(fptr);
        }

        

        // Counting the specific category of exception for later changes
        if (strncmp(buffer, "WARNING:", 8) == 0) warning_count++;
        else if (strncmp(buffer, "ERROR:", 6) == 0) error_count++;
        else if (strncmp(buffer, "CRITICAL:", 9) == 0) critical_count++;

        printf("# of warnings : %d\n# of errors : %d\n# of critical failures : %d\n",warning_count, error_count, critical_count);

        send_slack_notification(final_message);

        if (should_notify(now)) {
            send_slack_notification("⚠️ High exception rate: 5+ exceptions in the last hour.");
        }

        close(new_fd);
    }

    close(socket_fd);
    return 0;
}