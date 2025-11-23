/*
 * Name        : main.c
 * Author      : Marica Cosola
 * Description : TCP Client - Computer Networks assignment
 *
 * This file contains the boilerplate code for a TCP client
 * portable across Windows, Linux and macOS.
 */

#if defined WIN32
#include <winsock.h>
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "protocol.h"

#define NO_ERROR 0

void clearwinsock() {
#if defined WIN32
	WSACleanup();
#endif
}

void errorhandler(char *errorMessage) {
	printf("%s", errorMessage);
}

// Displays the correct command-line syntax and supported weather request types to standard output
void print_usage(const char *prog) {
	printf("Utilizzo: %s [-s server] [-p port] -r \"type city\"\n", prog);
	printf("type: 't' temperatura, 'h' umidita, 'w' vento, 'p' pressione\n");
}

// Main
int main(int argc, char *argv[]) {

#if defined WIN32
	// Initialize Winsock
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
	if (result != NO_ERROR) {
		printf("Error at WSAStartup()\n");
		return 0;
	}
#endif

	// Defaults
	char server_addr_str[256] = "127.0.0.1";
	int port = SERVER_PORT;
	char request_str[BUFFER_SIZE] = { 0 };
	int have_request = 0;

	// Parse CLI arguments
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
			strncpy(server_addr_str, argv[++i], sizeof(server_addr_str) - 1);
		} else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
			port = atoi(argv[++i]);
			if (port <= 0)
				port = SERVER_PORT;
		} else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
			strncpy(request_str, argv[++i], sizeof(request_str) - 1);
			have_request = 1;
		} else {
			print_usage(argv[0]);
			clearwinsock();
			return 1;
		}
	}

	if (!have_request) {
		print_usage(argv[0]);
		clearwinsock();
		return 1;
	}

	// Parse "type city" from request_str
	char type = '\0';
	char city[64];
	memset(city, 0, sizeof(city));
	{
		char *p = request_str;

		while (*p == ' ')
			p++;

		if (*p == '\0') {
			print_usage(argv[0]);
			clearwinsock();
			return 1;
		}

		type = p[0];
		p++;

		while (*p == ' ')
			p++;

		if (*p == '\0') {
			print_usage(argv[0]);
			clearwinsock();
			return 1;
		}

		strncpy(city, p, 64 - 1);
		city[64 - 1] = '\0';

		for (int i = strlen(city) - 1; i >= 0 && city[i] == ' '; i--) {
			city[i] = '\0';
		}
	}

	// Create socket
	int c_socket;
	c_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (c_socket < 0) {
		errorhandler("Socket creation failed.\n");
		closesocket(c_socket);
		clearwinsock();
		return -1;
	}

	// Set connection settings
	struct sockaddr_in sad;
	memset(&sad, 0, sizeof(sad));
	sad.sin_family = AF_INET;
	sad.sin_addr.s_addr = inet_addr(server_addr_str);
	sad.sin_port = htons(port);

	// Connection
	if (connect(c_socket, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
		errorhandler("Failed to connect.\n");
		closesocket(c_socket);
		clearwinsock();
		return -1;
	}

	// Build request struct
	weather_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = type;
	strncpy(req.city, city, 64 - 1);

	// Send data to server
	if (send(c_socket, &req, sizeof(weather_request_t), 0)
			!= sizeof(weather_request_t)) {
		errorhandler("send() sent a different number of bytes than expected\n");
		closesocket(c_socket);
		clearwinsock();
		return -1;
	}

	// Get reply from server
	weather_response_t resp;
	memset(&resp, 0, sizeof(resp));
	int bytes_rcvd;
	int total_bytes_rcvd = 0;

	while (total_bytes_rcvd < sizeof(weather_response_t)) {
		if ((bytes_rcvd = recv(c_socket, ((char*) &resp) + total_bytes_rcvd,
				sizeof(weather_response_t) - total_bytes_rcvd, 0)) <= 0) {
			errorhandler("recv() failed or connection closed prematurely\n");
			closesocket(c_socket);
			clearwinsock();
			return -1;
		}
		total_bytes_rcvd += bytes_rcvd;
	}

	// Print output
	printf("Ricevuto risultato dal server ip %s. ", server_addr_str);

	if (resp.status == STATUS_SUCCESS) {
		// Format city name: first letter uppercase, rest lowercase
		char formatted_city[64];
		strncpy(formatted_city, city, sizeof(formatted_city) - 1);
		formatted_city[sizeof(formatted_city) - 1] = '\0';

		if (formatted_city[0] != '\0') {
			formatted_city[0] = toupper((unsigned char) formatted_city[0]);
			for (int i = 1; formatted_city[i] != '\0'; i++) {
				formatted_city[i] = tolower((unsigned char) formatted_city[i]);
			}
		}

		char t = (char) tolower((unsigned char) resp.type);
		if (t == 't') {
			printf("%s: Temperatura = %.1f°C\n", formatted_city, resp.value);
		} else if (t == 'h') {
			printf("%s: Umidità = %.1f%%\n", formatted_city, resp.value);
		} else if (t == 'w') {
			printf("%s: Vento = %.1f km/h\n", formatted_city, resp.value);
		} else if (t == 'p') {
			printf("%s: Pressione = %.1f hPa\n", formatted_city, resp.value);
		}
	} else if (resp.status == STATUS_CITY_NOT_AVAILABLE) {
		printf("Città non disponibile\n");
	} else if (resp.status == STATUS_INVALID_REQUEST) {
		printf("Richiesta non valida\n");
	}

	// Close socket
	closesocket(c_socket);

	printf("Client terminated.\n");

	clearwinsock();
	return 0;
} // Main end
