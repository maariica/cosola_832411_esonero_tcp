/*
 * Name        : main.c
 * Author      : Marica Cosola
 * Description : TCP Server - Computer Networks assignment
 *
 * This file contains the boilerplate code for a TCP server
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
#include <time.h>
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

// Weather generation functions
float get_temperature(void) {
	// Range: -10.0 to 40.0
	float r = (float) rand() / (float) RAND_MAX;
	return -10.0f + r * (40.0f + 10.0f);
}

float get_humidity(void) {
	// Range: 20.0 to 100.0
	float r = (float) rand() / (float) RAND_MAX;
	return 20.0f + r * (100.0f - 20.0f);
}

float get_wind(void) {
	// Range: 0.0 to 100.0
	float r = (float) rand() / (float) RAND_MAX;
	return 0.0f + r * (100.0f - 0.0f);
}

float get_pressure(void) {
	// Range: 950.0 to 1050.0
	float r = (float) rand() / (float) RAND_MAX;
	return 950.0f + r * (1050.0f - 950.0f);
}

// Helper: safely copies a string to a buffer converting it to lowercase to ensure case-insensitive comparison
static void str_tolower_copy(char *dst, const char *src, int maxlen) {
	int i;
	for (i = 0; i < maxlen - 1 && src[i] != '\0'; i++) {
		dst[i] = tolower((unsigned char) src[i]);
	}
	dst[i] = '\0';
}

// Registry of supported cities
static const char *supported_cities[] = { "bari", "roma", "milano", "napoli",
		"torino", "palermo", "genova", "bologna", "firenze", "venezia" };

// Check if the requested city exists by normalizing the input to lowercase
static int is_supported_city(const char *city) {
	char lower[64];
	str_tolower_copy(lower, city, 64);
	for (size_t i = 0;
			i < sizeof(supported_cities) / sizeof(supported_cities[0]); i++) {
		if (strcmp(lower, supported_cities[i]) == 0)
			return 1;
	}
	return 0;
}

// Validates the request type char (checking for 't', 'h', 'w', 'p'), handling both uppercase and lowercase input
static int is_valid_type(char t) {
	char c = tolower((unsigned char) t);
	return (c == 't' || c == 'h' || c == 'w' || c == 'p');
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

	// Parse CLI arguments
	int port = SERVER_PORT;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
			port = atoi(argv[++i]);
			if (port <= 0)
				port = SERVER_PORT;
		}
	}

	// Seed RNG
	srand((unsigned int) time(NULL));

	// Create socket
	int my_socket;
	my_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (my_socket < 0) {
		errorhandler("Socket creation failed.\n");
		closesocket(my_socket);
		clearwinsock();
		return -1;
	}

	// Set connection settings
	struct sockaddr_in sad;
	memset(&sad, 0, sizeof(sad));
	sad.sin_family = AF_INET;
	sad.sin_addr.s_addr = inet_addr("127.0.0.1");
	sad.sin_port = htons(port);

	// Bind
	if (bind(my_socket, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
		errorhandler("bind() failed.\n");
		closesocket(my_socket);
		clearwinsock();
		return -1;
	}

	// Listen
	if (listen(my_socket, QUEUE_SIZE) < 0) {
		errorhandler("listen() failed.\n");
		closesocket(my_socket);
		clearwinsock();
		return -1;
	}

	printf("Server listening on port %d\n", port);

	struct sockaddr_in cad;
	int client_socket;

#if defined WIN32
    int client_len;
    #else
	socklen_t client_len;
#endif

	printf("Waiting for a client to connect...\n");

	// Accept new connection
	while (1) {
		client_len = sizeof(cad);
		// Accept
		if ((client_socket = accept(my_socket, (struct sockaddr*) &cad,
				&client_len)) < 0) {
			errorhandler("accept() failed.\n");
			closesocket(my_socket);
			clearwinsock();
			return -1;
		}

		printf("Handling client %s\n", inet_ntoa(cad.sin_addr));

		// Receive request
		weather_request_t req;
		memset(&req, 0, sizeof(req));
		int bytes_rcvd;
		int total_bytes_rcvd = 0;

		while (total_bytes_rcvd < sizeof(weather_request_t)) {
			if ((bytes_rcvd = recv(client_socket,
					((char*) &req) + total_bytes_rcvd,
					sizeof(weather_request_t) - total_bytes_rcvd, 0)) <= 0) {
				errorhandler(
						"recv() failed or connection closed prematurely\n");
				closesocket(client_socket);
				break;
			}
			total_bytes_rcvd += bytes_rcvd;
		}
		if (total_bytes_rcvd < sizeof(weather_request_t))
			continue;

		// Safety: Ensure null-termination
		req.city[64 - 1] = '\0';

		// Request
		printf("Richiesta '%c %s' dal client ip %s\n", req.type, req.city,
				inet_ntoa(cad.sin_addr));

		// Validate and generate response
		weather_response_t resp;
		memset(&resp, 0, sizeof(resp));

		if (!is_valid_type(req.type)) {
			resp.status = STATUS_INVALID_REQUEST;
			resp.type = '\0';
			resp.value = 0.0f;
		} else if (!is_supported_city(req.city)) {
			resp.status = STATUS_CITY_NOT_AVAILABLE;
			resp.type = '\0';
			resp.value = 0.0f;
		} else {
			resp.status = STATUS_SUCCESS;
			char t = (char) tolower((unsigned char) req.type);
			resp.type = t;

			switch (t) {
			case 't':
				resp.value = get_temperature();
				break;
			case 'h':
				resp.value = get_humidity();
				break;
			case 'w':
				resp.value = get_wind();
				break;
			case 'p':
				resp.value = get_pressure();
				break;
			default:
				resp.value = 0.0f;
				break;
			}
		}

		// Send response
		if (send(client_socket, &resp, sizeof(weather_response_t), 0)
				!= sizeof(weather_response_t)) {
			errorhandler(
					"send() sent a different number of bytes than expected\n");
		}

		// Close client
		closesocket(client_socket);
	} // End while

	printf("Server terminated.\n");

	closesocket(my_socket);
	clearwinsock();
	return 0;
} // Main end
