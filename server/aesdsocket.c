#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define AESD_SOCKET_CONNECTION_BACKLOG 1U
#define AESD_BUFFER_SIZE 1024
#define AESDSOCKET_DATA_FILE "/var/tmp/aesdsocketdata"

static volatile sig_atomic_t caught_signal = 0;


struct aesdpacket_S{
	char *packet;
	size_t packet_len;
};

struct aesdsocket_info_S{
    struct addrinfo hints;
    struct sockaddr_storage client;
    socklen_t client_addrlen;
    struct addrinfo *result;
    int sockfd;
	int client_fd;
	int connection_backlog;
};

struct aesddaemon_info_S{
    pid_t pid;
    pid_t session_id;
};

static void signal_handler(int signo)
{
    if ((signo == SIGINT) || (signo == SIGTERM)) {
        caught_signal = 1;
    }
}

int closeClientConnection(struct aesdsocket_info_S * aesdsocket){
	int ret = 0;
     char client_ip[INET_ADDRSTRLEN];
	struct sockaddr_in *addr4;
	addr4 = (struct sockaddr_in *)&aesdsocket->client;
	inet_ntop(AF_INET, &addr4->sin_addr, client_ip, sizeof(client_ip));

	ret = close(aesdsocket->client_fd);
	if(ret == 0)
		syslog(LOG_INFO, "Closed connection from %s", client_ip);
	else
		syslog(LOG_ERR, "Failed to closed connection from %s", client_ip);
	
	return ret;
}

int sendFileToClient(struct aesdsocket_info_S * aesdsocket)
{
    int fd;
    char buffer[AESD_BUFFER_SIZE];
    ssize_t bytes_read;
	int ret= 0;

   char client_ip[INET_ADDRSTRLEN];
	struct sockaddr_in *addr4;
	addr4 = (struct sockaddr_in *)&aesdsocket->client;
	inet_ntop(AF_INET, &addr4->sin_addr, client_ip, sizeof(client_ip));

    fd = open("/var/tmp/aesdsocketdata", O_RDONLY);

    if (fd == -1)
    {
        syslog(LOG_ERR, "Failed to open file /var/tmp/aesdsocketdata!");
        ret = -1;
    }
    syslog(LOG_INFO, "Sending data to client %s", client_ip);
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
    {
        ssize_t total_sent = 0;
        while (total_sent < bytes_read)
        {
	    syslog(LOG_INFO, "Sending %zd bytes: [%.*s]", bytes_read, (int)bytes_read, buffer);
            ssize_t bytes_sent = send(aesdsocket->client_fd, buffer + total_sent, bytes_read - total_sent, 0);
            if (bytes_sent == -1)
            {
		syslog(LOG_ERR, "Failed to send data to client %s!", client_ip);
                ret = -1;
		break;
            }
            total_sent += bytes_sent;
	    syslog(LOG_INFO, "Total sent data to client = %ld", total_sent);
        }
    }

    if ((ret == 0) && (bytes_read == -1))
    {
        syslog(LOG_ERR, "Failed to read from file /var/tmp/aesdsocketdata!");
        ret = -1;
    }

    if ( fd != -1)
		ret = (ret == 0)? close(fd):ret;
	
    return ret;
}

int flushBuffer(struct aesdpacket_S *aesdpacket){
	int ret = 0;
	int file_fd;
	ssize_t written;
	
	file_fd = open("/var/tmp/aesdsocketdata", O_WRONLY | O_CREAT | O_APPEND, 0644);

	if (file_fd == -1) {
		syslog(LOG_ERR, "Failed to open for fush to file /var/tmp/aesdsocketdata!");
		ret = -1;
	}
	if(ret == 0){
	    written = write(file_fd, aesdpacket->packet, aesdpacket->packet_len);
	    if (written != (ssize_t)aesdpacket->packet_len) {
		syslog(LOG_ERR, "Failed to flush to file /var/tmp/aesdsocketdata!");
		ret = -1;
	    }
	}
	
	if (file_fd != -1)
		ret = (ret == 0)? close(file_fd):ret;
	
	return ret;
}

int savePackets(struct aesdpacket_S *aesdpacket, struct aesdsocket_info_S *aesdsocket){
	char *newline;
	int ret = 0;
	int file_fd;
	
	file_fd = open("/var/tmp/aesdsocketdata", O_WRONLY | O_CREAT | O_APPEND, 0644);

	if (file_fd == -1) {
		syslog(LOG_ERR, "Failed to open or create file /var/tmp/aesdsocketdata!");
		ret = -1;
	}
	while ((ret == 0) && ((newline = strchr(aesdpacket->packet, '\n')) != NULL)) {
		size_t complete_len, remaining =0;
		ssize_t written;
                syslog(LOG_INFO, "Writing data to file /var/tmp/aesdsocketdata!");	
		complete_len = newline - aesdpacket->packet + 1;
		syslog(LOG_INFO, "Writing data size = %zu", complete_len);
			written = write(file_fd, aesdpacket->packet, complete_len);
			if (written != (ssize_t)complete_len) {
				syslog(LOG_ERR, "Failed to write to file /var/tmp/aesdsocketdata!");
				ret = -1;
			}else{
				remaining = aesdpacket->packet_len - complete_len;
				memmove(aesdpacket->packet, aesdpacket->packet + complete_len, remaining + 1 );
				syslog(LOG_INFO, "Remaining data size = %zu", remaining);
				aesdpacket->packet_len = remaining;
				ret = sendFileToClient(aesdsocket);
			}
	}
	
	if (file_fd != -1)
		ret = close(file_fd);
	
	return ret;
}

int updatePacketBuffer(struct aesdpacket_S *aesdpacket, char buffer[], size_t bytes_received){
	int ret = 0;
	char *new_packet = realloc(aesdpacket->packet, aesdpacket->packet_len + bytes_received + 1);

	if (new_packet == NULL) {
		syslog(LOG_ERR, "realloc failed");
		free(aesdpacket->packet);
		aesdpacket->packet = NULL;
        aesdpacket->packet_len = 0;
		ret = -1;
	}else{

		aesdpacket->packet = new_packet;
		memcpy(aesdpacket->packet + aesdpacket->packet_len, buffer, bytes_received);
		aesdpacket->packet_len += bytes_received;
		aesdpacket->packet[aesdpacket->packet_len] = '\0';
	}
			
	return ret;	
};

int receiveData(struct aesdsocket_info_S *aesdsocket){
	struct aesdpacket_S aesdpacket = {NULL, 0};
	char buffer[AESD_BUFFER_SIZE];
	ssize_t bytes_received;
	int ret = 0;
	bool rx_done = false;
	
	while((ret == 0) && (!rx_done)){
		bytes_received = recv(aesdsocket->client_fd, buffer, sizeof(buffer) - 1, 0);
		if(bytes_received > 0){
			syslog(LOG_INFO, "Received bytes = %zu",bytes_received);
			ret = updatePacketBuffer(&aesdpacket, buffer, bytes_received);
			if((ret == 0) && (aesdpacket.packet != NULL)){
				ret = savePackets(&aesdpacket, aesdsocket);
			}
		}else if(bytes_received == 0){
			if(aesdpacket.packet_len > 0){
				syslog(LOG_INFO, "Remaining not written data size = %zu",aesdpacket.packet_len);
				ret = flushBuffer(&aesdpacket);
			}
			syslog(LOG_INFO, "Data reading is successfully completed!");
			rx_done = true;
		}else{
			syslog(LOG_ERR, "Failed to read from connection!");
			ret = -1;
		}
	}
	
	if(aesdpacket.packet != NULL){
		free(aesdpacket.packet);
	}
	
	return ret;
}

int waitForConnection(struct aesdsocket_info_S *aesdsocket){
	int ret = 0;
	
	aesdsocket->client_fd = accept(aesdsocket->sockfd, (struct sockaddr *)&aesdsocket->client, &aesdsocket->client_addrlen);
	if (aesdsocket->client_fd == -1) {
		syslog(LOG_ERR, "Connection Failed!");
		ret = -1;
	}else{
	    char client_ip[INET_ADDRSTRLEN];
	    struct sockaddr_in *addr4;
	    addr4 = (struct sockaddr_in *)&aesdsocket->client;
  	    inet_ntop(AF_INET, &addr4->sin_addr, client_ip, sizeof(client_ip));
		syslog(LOG_INFO, "Accepted connection from %s", client_ip);
	}
	return ret;
}

int openListenSocket(struct aesdsocket_info_S *aesdsocket){
	int ret = 0;
    memset(&aesdsocket->hints, 0, sizeof(aesdsocket->hints));

    aesdsocket->hints.ai_family = AF_INET;        // IPv4
    aesdsocket->hints.ai_socktype = SOCK_STREAM;  // TCP
    aesdsocket->hints.ai_flags = AI_PASSIVE;      // For bind()

    ret = getaddrinfo(NULL, "9000", &aesdsocket->hints, &aesdsocket->result);

    if (ret != 0) {
		syslog(LOG_ERR, "Failed to get addrinfo!");
        ret = -1;
    }else{
	
		aesdsocket->sockfd = socket(aesdsocket->result->ai_family,
						aesdsocket->result->ai_socktype,
						aesdsocket->result->ai_protocol);
		if (aesdsocket->sockfd == -1){
			syslog(LOG_ERR, "Failed to open socket!");
			ret = -1;
		}else{
			if(bind(aesdsocket->sockfd, aesdsocket->result->ai_addr, aesdsocket->result->ai_addrlen) != 0){
				syslog(LOG_ERR, "Failed to bind to socket!");
				close(aesdsocket->sockfd);
				ret = -1;
			}
		}

		if(ret == 0){
			if (listen(aesdsocket->sockfd, aesdsocket->connection_backlog) != 0) {
				syslog(LOG_ERR, "Failed to listen to socket!");
				ret = -1;
			}
		}
	}
	
	return ret;
}

int startDaemon(struct aesddaemon_info_S * aesddaemon){
    int ret = 0;
    syslog(LOG_INFO, "Application runs as Daemon");

    /* Fork off the parent */
    aesddaemon->pid = fork();

    if (aesddaemon->pid < 0){
        syslog(LOG_ERR, "Failed to fork child process");
        exit(EXIT_FAILURE);
    }

    if (aesddaemon->pid > 0) {
        exit(EXIT_SUCCESS);
    }

    aesddaemon->session_id = setsid();
    /* Create new session */
    if (aesddaemon->session_id < 0){
        syslog(LOG_ERR, "Failed to detach daemon to separate session");
        ret = -1;
    }else{
	aesddaemon->pid = getpid();
        printf("PID: %d\n", aesddaemon->pid);
        printf("Session ID: %d\n", aesddaemon->session_id);

        /* Reset file permissions */
        umask(0);

        /* Change working directory */
        chdir("/");

        /* Close standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        /* Redirect stdin/stdout/stderr to /dev/null */
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_RDWR);
    }
    return ret;
}

void exitGracefully(struct aesdsocket_info_S *aesdsocket){
	if (caught_signal) {
		syslog(LOG_INFO, "Caught signal, exiting");
	}
	
	freeaddrinfo(aesdsocket->result);
	if (aesdsocket->client_fd >= 0) {
		close(aesdsocket->client_fd);
	}

	if (aesdsocket->sockfd >= 0) {
		close(aesdsocket->sockfd);
	}

	remove(AESDSOCKET_DATA_FILE);

	closelog();
}

void configAesdSocket(struct aesdsocket_info_S *aesdsocket){
	memset(aesdsocket, 0, sizeof(struct aesdsocket_info_S));
	aesdsocket->connection_backlog = AESD_SOCKET_CONNECTION_BACKLOG;
	aesdsocket->client_addrlen = sizeof(aesdsocket->client);
}

int setup_signal_handlers(void)
{
	int ret = 0;
    struct sigaction sa = {0};

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

	ret = sigaction(SIGINT, &sa, NULL);
    if (ret == 0) {
        ret = sigaction(SIGTERM, &sa, NULL);
    }

    return ret;
}

int main(int argc, char *argv[]){
    int opt;
	int ret = 0;
    bool daemon_mode = false;
	struct aesddaemon_info_S aesddaemon;
    struct aesdsocket_info_S aesdsocket;

    openlog(NULL, 0, LOG_USER);
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
        case 'd':
            daemon_mode = true;
			break;

        default:
			syslog(LOG_ERR, "Invalid options provided. Application terminates.");
            ret = -1;
        }
    }

    if ((ret != -1) && daemon_mode) {
        ret = startDaemon(&aesddaemon);
    }

	if (setup_signal_handlers() != 0) {
		syslog(LOG_ERR, "Failed to register signal handlers");
		ret = -1;
	}
	
	if (ret == 0) {
		configAesdSocket(&aesdsocket);
        ret = openListenSocket(&aesdsocket);
    }
	
	while ((ret == 0) && !caught_signal) {
		ret = waitForConnection(&aesdsocket);
		if(ret == 0)
			ret = receiveData(&aesdsocket);
		
		if(aesdsocket.client_fd >= 0)
			ret = (ret == 0)?closeClientConnection(&aesdsocket):ret ;
		
	}
	
	exitGracefully(&aesdsocket);
	if(caught_signal)
		ret = 0;

    return ret;
}

