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

MQTTClient client = NULL;
char *clientId = NULL;
char *topicName = NULL;
char *id = NULL;
MQTTClient_willOptions willOptions = MQTTClient_willOptions_initializer;

initializeGlobals(char *device_id) {
	id = device_id;

	clientId = (char *) malloc(24);
	sprintf(clientId, "Device_%s", id);

	topicName = (char *) (malloc(15 + strlen(id)));
	sprintf(topicName, "device/%s/status", id);
}

int connectToMQTT(char *argv[]);

// connectionLost callback that just tries to connect again
void connectionLost(void *context, char *cause) {
	int rc;

	printf("In connectionLost, cause: %s\n", cause);

	rc = connectToMQTT(context);
	if (rc == MQTTCLIENT_SUCCESS) {
		printf("Have reconnected\n");
	} else {
		fprintf(stderr, "Failed to reconnect, rc = %d\n", rc);
	}
}

// messageArrived callback - should never be called in this scenario but must be supplied
int messageArrived(void *context, char *topicName, int topicLen,
		MQTTClient_message *message) {
	fprintf(stderr, "Unexpected call of messageArrived callback\n");

	return 1;
}

int publishConnectedMessage() {
	int rc = -1;
	MQTTClient_deliveryToken dt;

	rc = MQTTClient_publish(client, topicName, 9, "CONNECTED", 2, 1, &dt);
	if (rc == MQTTCLIENT_SUCCESS) {
		rc = MQTTClient_waitForCompletion(client, dt, 10000L);
	}

	return rc;
}

void addWillOptions(MQTTClient_connectOptions *connectOptions) {
	willOptions.topicName = topicName;
	willOptions.message = "LOST CONNECTION";
	willOptions.retained = 1;
	willOptions.qos = 2;

	connectOptions->will = &willOptions;
}

int connectToMQTT(char *argv[]) {
	int rc = -1;
	MQTTClient_connectOptions connectOptions =
	MQTTClient_connectOptions_initializer;

	initializeGlobals(argv[1]);

	rc = MQTTClient_create(&client, argv[2], clientId,
	MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	if (rc == MQTTCLIENT_SUCCESS) {
		// Put the MQTT client into multi-threaded mode so we don't have to call receive() or yield() to process keepAlive
		rc = MQTTClient_setCallbacks(client, argv, connectionLost,
				messageArrived, NULL);
		if (rc == MQTTCLIENT_SUCCESS) {
			addWillOptions(&connectOptions);
			rc = MQTTClient_connect(client, &connectOptions);
			if (rc != MQTTCLIENT_SUCCESS) {
				fprintf(stderr, "Failed to connect, rc = %d\n", rc);
			} else {
				rc = publishConnectedMessage(clientId);
			}
		} else {
			fprintf(stderr, "Failed to set callbacks, rc = %d\n", rc);
		}
	} else {
		fprintf(stderr, "Failed to create MQTTClient, rc = %d\n", rc);
	}

	return rc;
}

int publishDisconnectedMessage() {
	int rc = -1;
	MQTTClient_deliveryToken dt;

	// Assume topicName was set earlier

	rc = MQTTClient_publish(client, topicName, 12, "DISCONNECTED", 2, 1, &dt);
	if (rc == MQTTCLIENT_SUCCESS) {
		rc = MQTTClient_waitForCompletion(client, dt, 10000L);
	}

	return rc;
}

void disconnectFromMQTT() {
	int rc = -1;

	if (client) {
		if (MQTTClient_isConnected(client)) {
			publishDisconnectedMessage();
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

	// Arguments are a unique ID for this instance and the server URL, which must include the port
	if (argc != 3) {
		printf("usage: DeviceSimulator <id> <URL>");
	} else {
		rc = connectToMQTT(argv);
		if (rc == MQTTCLIENT_SUCCESS) {
			printf(
					"Have connected. Input d then <Enter> to disconnect, anything else then <Enter> to terminate\n");
			read = getline(&line, &size, stdin);
			if (read == -1) {
				exit(-1);
			} else {
				if (read == 2) {
					if (line[0] == 'd') {
						disconnectFromMQTT();
						exit(0);
					} else {
						exit(-2);
					}
				} else {
					exit(-3);
				}
			}
		} else {
			fprintf(stderr, "Failed to connect, rc = %d\n", rc);
			exit(-4);
		}
	}

	return (EXIT_SUCCESS);
}
