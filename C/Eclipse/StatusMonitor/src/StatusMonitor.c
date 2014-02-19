/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    John Colgrave - initial contribution
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <MQTTClient.h>

MQTTClient client;

typedef struct {
	char *serverURI;
	char *id;
} context_t;

context_t context;

int connectToMQTT(context_t *contextp);

// connectionLost callback that just tries to connect again
void connectionLost(void *context, char *cause) {
	int rc;
	context_t *contextp = (context_t *) context;

	printf("In connectionLost, cause: %s\n", cause);

	rc = connectToMQTT(contextp);
	if (rc == MQTTCLIENT_SUCCESS) {
		printf("Have reconnected\n");
	} else {
		fprintf(stderr, "Failed to reconnect, rc = %d\n", rc);
	}
}

// messageArrived callback
int messageArrived(void *context, char *topicName, int topicLen,
		MQTTClient_message *message) {

	// In this application we know that there are no embedded NULL characters in topicName

	printf("message arrived on topic %s: %.*s\n", topicName,
			message->payloadlen, (char *) (message->payload));

	return 1;
}

int connectToMQTT(context_t *contextp) {
	int rc = -1;
	MQTTClient_connectOptions connectOptions =
	MQTTClient_connectOptions_initializer;
	char *clientId = NULL;
	size_t clientIdLen = 0;

	if (contextp->id) {
		clientIdLen = strlen(contextp->id);
		if (clientIdLen < 10) {
			clientId = malloc(15 + strlen(contextp->id));
			sprintf(clientId, "StatusMonitor_%s", contextp->id);
		} else {
			fprintf(stderr, "Supplied clientId is too long\n");
		}
	} else {
		clientId = "StatusMonitor";
	}

	if (clientId) {
		rc = MQTTClient_create(&client, contextp->serverURI, clientId,
		MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
		if (rc == MQTTCLIENT_SUCCESS) {
			// Put the MQTT client into multi-threaded mode so we don't have to call receive() or yield() to process keepAlive
			rc = MQTTClient_setCallbacks(client, contextp->serverURI,
					connectionLost, messageArrived, NULL);
			if (rc == MQTTCLIENT_SUCCESS) {
				rc = MQTTClient_connect(client, &connectOptions);
				if (rc != MQTTCLIENT_SUCCESS) {
					fprintf(stderr, "Failed to connect, rc = %d\n", rc);
				}
			} else {
				fprintf(stderr, "Failed to set callbacks, rc = %d\n", rc);
			}
		} else {
			fprintf(stderr, "Failed to create MQTTClient, rc = %d\n", rc);
		}
	}

	return rc;
}

void disconnectFromMQTT() {
	int rc = -1;

	if (client) {
		if (MQTTClient_isConnected(client)) {
			rc = MQTTClient_disconnect(client, 5000); // Allow 5 seconds for DISCONNECTED message to complete
			if (rc == MQTTCLIENT_SUCCESS) {
				MQTTClient_destroy(&client);
			}
		}
	}
}

int main(int argc, char *argv[]) {
	int rc = -1;
	char *line = NULL;
	size_t size = 0;
	ssize_t read;
	char *id = NULL;
	char *serverURI = NULL;
	char *topic = NULL;

	// Arguments are an optional ID to monitor and the server URL, which must include the port
	if ((argc < 2) || (argc > 3)) {
		printf("usage: DeviceSimulator [<id>] <URL>");
	} else {
		if (argc == 2) {
			serverURI = argv[1];
			topic = "device/+/status";
			context.serverURI = serverURI;
		} else {
			id = argv[1];
			serverURI = argv[2];
			topic = malloc(15 + strlen(id));
			sprintf(topic, "device/%s/status", id);
			context.serverURI = serverURI;
			context.id = id;
		}
		rc = connectToMQTT(&context);
		if (rc == MQTTCLIENT_SUCCESS) {
			rc = MQTTClient_subscribe(client, topic, 2);
			if (rc == MQTTCLIENT_SUCCESS) {
				printf(
						"Have connected and subscribed. Press <Enter> to terminate\n");
				read = getline(&line, &size, stdin);
				if (read == -1) {
					exit(-1);
				} else {
					disconnectFromMQTT();
					exit(0);
				}
			} else {
				fprintf(stderr, "Failed to subscribe, rc = %d\n", rc);
			}
		} else {
			fprintf(stderr, "Failed to connect, rc = %d\n", rc);
			exit(-2);
		}
	}

	return (EXIT_SUCCESS);
}
