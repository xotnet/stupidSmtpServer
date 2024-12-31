#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "net.h"

int PACKAGE_LEN = 1024;

int char_count(char *str) {
  int count = 0;
  while (*str != '\0') {
    count++;
    str++;
  }
  return count;
}

unsigned short int isIn(char* buf, char* keyword) {
	if (buf == NULL || keyword == NULL) {
    return 0;
  }
	int bufLen = char_count(buf);
	int keywordLen = char_count(keyword);
	if (bufLen < keywordLen) {
    return 0;
  }
	int g = 0;
	for (int i = 0; i<bufLen; i++) {
		if (g == keywordLen-1) {
			return 1;
		}
		if (*(buf+i) == *(keyword+g)) {
			g++;
		} else {
			g = 0;
		}
	}
	return 0;
}

const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
unsigned char* base64_decode(const char *input, size_t *output_length) {
    size_t input_length = strlen(input);
    if (input_length % 4 != 0) {
        return NULL;
    }
    *output_length = (3 * input_length) / 4;
    if (input[input_length - 1] == '=') {
        (*output_length)--;
    }
    if (input[input_length - 2] == '=') {
        (*output_length)--;
    }
    unsigned char *decoded_data = (unsigned char *)malloc(*output_length);
    if (decoded_data == NULL) {
        return NULL;
    }
    size_t j = 0;
    uint32_t sextet_bits = 0;
    int sextet_count = 0;
    for (size_t i = 0; i < input_length; i++) {
        // Convert Base64 character to a 6-bit value
        uint32_t base64_value = 0;
        if (input[i] == '=') {
            base64_value = 0;
        } else {
            const char *char_pointer = strchr(base64_chars, input[i]);
            if (char_pointer == NULL) {
                free(decoded_data);
                return NULL;
            }
            base64_value = char_pointer - base64_chars;
        }
        sextet_bits = (sextet_bits << 6) | base64_value;
        sextet_count++;
        if (sextet_count == 4) {
            decoded_data[j++] = (sextet_bits >> 16) & 0xFF;
            decoded_data[j++] = (sextet_bits >> 8) & 0xFF;
            decoded_data[j++] = sextet_bits & 0xFF;
            sextet_bits = 0;
            sextet_count = 0;
        }
    }
    return decoded_data;
}

void kickClient(int sock, short int outConsole) {
	close_net(sock);
	if (outConsole == 1) {printf("[%d] Client disconnected!\n", sock);}
}

unsigned short int cmpWL(char* msg, char* msg2, int from, int to) {
	unsigned int msgLen = char_count(msg);
	unsigned int msg2Len = char_count(msg2);
	if (msgLen == 0 || char_count(msg2) == 0 || msgLen<msg2Len) {return 0;}
	int p = 0;
	while (from<msgLen && from<to) {
		if (*(msg+from) != *(msg2+p)) {return 0;}
		p++;
		from++;
	}
	return 1;
}

void getCleanMessageText(char* buf, char* msg) {
	int bufLen = char_count(buf);
	int g = 0;
	unsigned short int skipSegment = 0;
	for (int i = 0; i<bufLen; i++) {
		if (*(buf+i) == '<') {
			skipSegment = 1;
			if (*(buf+i+1) == 'B' && *(buf+i+2) == 'R') {
				msg[g] = '\n';
				g++;
			}
		}
		if (*(buf+i) == '>') {skipSegment = 0; continue;}
		if (skipSegment == 0) {
			msg[g] = buf[i];
			g++;
		}
	}
}

void messageHandler(int socket) {
	printf("Client connected!\n");
	char buf[PACKAGE_LEN];
	char msg[PACKAGE_LEN];
	char recvMail[64] = "";
	char fromMail[64] = "";
	strcpy(buf, "220 Welcome to Simple SMTP Server\r\n");
	send_net(socket, buf, char_count(buf));
	while (1) {
		memset(buf, 0, sizeof(buf));
		int status = recv_net(socket, buf, PACKAGE_LEN);
		strcpy(msg, buf);
		if (char_count(msg) == 0 || status == 0) {kickClient(socket, 1); return;}
		else if (cmpWL(buf, "HELO", 0, 4) == 1) {
			strcpy(buf, "250 Hello\r\n");
			send_net(socket, buf, char_count(buf));
		} else if (cmpWL(buf, "MAIL FROM:", 0, 10) == 1) {
			strcpy(fromMail, buf+11);
			fromMail[status-1] = '\0';
			strcpy(buf, "250 OK\r\n");
			send_net(socket, buf, char_count(buf));
		} else if (cmpWL(buf, "RCPT TO:", 0, 8) == 1) {
			strcpy(recvMail, buf+9);
			recvMail[status-1] = '\0';
			strcpy(buf, "250 OK\r\n");
			send_net(socket, buf, char_count(buf));
		} else if (cmpWL(buf, "QUIT", 0, 4) == 1) {
			strcpy(buf, "251 Bye\r\n");
			send_net(socket, buf, char_count(buf));
			kickClient(socket, 1); break;
		} else if (cmpWL(buf, "DATA", 0, 4) == 1) {
			printf("-1");
			strcpy(buf, "354 End data with <CR><LF>.<CR><LF>\r\n");
			send_net(socket, buf, char_count(buf));
			recv_net(socket, buf, PACKAGE_LEN); // first data block include subject

			strcpy(buf, "250 OK: Message received\r\n");
			send_net(socket, buf, char_count(buf));
			recv_net(socket, buf, PACKAGE_LEN); // get content of message
			printf("0");
			if (isIn(buf, "base64")) { // base64 decode
				printf("1");
				size_t decodedLen;
				unsigned char* decoded = base64_decode(buf, &decodedLen);
				printf("2");
				getCleanMessageText((char*)decoded, msg);
				printf("3");
				free(decoded);
			} else {
				getCleanMessageText(buf, msg);
			}

			strcpy(buf, "250 OK: Message received\r\n");
			send_net(socket, buf, char_count(buf));

			printf("Mail message: %s\n", msg);
		} else {
			strcpy(buf, "500 Unknown command\r\n");
			send_net(socket, buf, char_count(buf));
		}
	}
}

void* accepter(void* server) {
	int socket = accept_net(*((int*)server));
	
	pthread_t id;
	pthread_create(&id, NULL, accepter, server);
	pthread_detach(id);
	
	messageHandler(socket);

	pthread_t* idP = &id;
	return idP;
}

int main() {
	int server = listen_net("0.0.0.0", "25", 0);
	pthread_t id;
	pthread_create(&id, NULL, accepter, (void*)&server);
	pthread_detach(id);
	printf("Server started!\n");
	while(1) {
		sleep(100);
	}
}