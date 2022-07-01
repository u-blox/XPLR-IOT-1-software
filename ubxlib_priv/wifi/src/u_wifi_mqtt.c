
/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the u-blox MQTT client API for WiFi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc(), free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "ctype.h"     // isdigit()
#include "string.h"    // memset(), strcpy(), strtok_r(), strtol()
#include "stdio.h"     // snprintf()
#include "assert.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* strtok_r and integer stdio, must
                                              be included before the other port
                                              files if any print or scan function
                                              is used. */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_event_queue.h"
#include "u_port_os.h"

#include "u_at_client.h"

#include "u_mqtt_common.h"
#include "u_mqtt_client.h"
#include "u_wifi_private.h"
#include "u_wifi_mqtt.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_edm_stream.h"
#include "u_ringbuffer.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * ------------------------------------------------------------- */
// todo: MQTT TLS connections need to be handled (AT+USECMNG) need to be implemented in u_security_tls.c
// todo: inactivity time out, MQTT will message, keep alive support (present in uconnect) - refer to u_mqtt_client.h
#define U_WIFI_MQTT_DATA_EVENT_STACK_SIZE 1536
#define U_WIFI_MQTT_DATA_EVENT_PRIORITY (U_CFG_OS_PRIORITY_MAX - 5)

typedef struct uWifiMqttTopic_t {
    char *pTopicStr;
    int32_t edmChannel;
    int32_t peerHandle;
    bool isTopicUnsubscribed;
    bool isPublish;
    uMqttQos_t qos;
    bool retain;
    struct uWifiMqttTopic_t *next;
} uWifiMqttTopic_t;

typedef struct uWifiMqttTopicList_t {
    uWifiMqttTopic_t *head;
    uWifiMqttTopic_t *tail;
} uWifiMqttTopicList_t;

typedef struct uWifiMqttSession_t {
    char *pBrokerNameStr;
    char *pClientIdStr;
    char *pUserNameStr;
    char *pPasswordStr;
    uint8_t *pRxBuffer;
    bool isConnected;
    uRingBuffer_t rxRingBuffer;
    uWifiMqttTopicList_t topicList;
    int32_t sessionHandle;
    uAtClientHandle_t atHandle;
    int32_t localPort;
    int32_t unreadMsgsCount;
    uPortSemaphoreHandle_t semaphore;
    void *cbParam;
    void (*dataCb)(int unreadMsgsCount, void *cbParam);
    void (*disconnectCb)(int status, void *cbParam);
} uWifiMqttSession_t;

typedef struct {
    uWifiMqttSession_t *mqttSession;
    void *cbParam;
    void (*dataCb)(int unreadMsgsCount, void *cbParam);
    int32_t disconnStatus;
    void (*disconnectCb)(int disconnStatus, void *cbParam);
} uCallbackEvent_t;

static uWifiMqttSession_t gMqttSessions[U_WIFI_MQTT_MAX_NUM_CONNECTIONS];
static uPortMutexHandle_t gMqttSessionMutex = NULL;
static int32_t gCallbackQueue = U_ERROR_COMMON_NOT_INITIALISED;
static int32_t gEdmChannel = -1;

/**
 * Fetch the topic string in a given MQTT session associated to particular EDM channel
 */
static char *getTopicStrForEdmChannel(uWifiMqttSession_t *mqttSession, int32_t edmChannel)
{
    uWifiMqttTopic_t *topic;
    char *ptopicNameStr = NULL;

    for (topic = mqttSession->topicList.head; topic != NULL; topic = topic->next) {

        if (topic->edmChannel == edmChannel) {

            ptopicNameStr = topic->pTopicStr;

        }
    }

    return ptopicNameStr;
}

/**
 * Fetch the topic object in a given MQTT session
 */
static uWifiMqttTopic_t* findTopic (uWifiMqttSession_t *mqttSession, const char *topicStr, bool isPublish)
{
    uWifiMqttTopic_t *temp;
    uWifiMqttTopic_t *foundTopic = NULL;

    for (temp = mqttSession->topicList.head; temp != NULL; temp = temp->next) {

        if ((!strcmp(temp->pTopicStr, topicStr)) && (isPublish == temp->isPublish)) {

            foundTopic = temp;
        }
    }

    return foundTopic;
}

/**
 * Allocate topic object and associate it to a given MQTT session
 */
static uWifiMqttTopic_t* pAllocateMqttTopic (uWifiMqttSession_t *mqttSession, bool isPublish)
{
    uWifiMqttTopic_t *topic = NULL;

    if (mqttSession != NULL) {

        topic = malloc(sizeof(uWifiMqttTopic_t));

        if (topic != NULL) {

            topic->next = NULL;

            if (mqttSession->topicList.head == NULL) {

                mqttSession->topicList.head = topic;

            } else {

                mqttSession->topicList.tail->next = topic;
            }

            mqttSession->topicList.tail = topic;

            topic->peerHandle = -1;
            topic->edmChannel = -1;
            topic->isTopicUnsubscribed = false;
            topic->isPublish = isPublish;
        }
    }

    return topic;
}
/**
 * Free a specific topic object associated to given MQTT session
 */
static void freeMqttTopic(uWifiMqttSession_t *mqttSession, uWifiMqttTopic_t *topic)
{
    uWifiMqttTopic_t *prev;
    uWifiMqttTopic_t *curr;

    for (prev = NULL, curr = mqttSession->topicList.head; curr != NULL; prev = curr, curr = curr->next) {

        if (!strcmp(curr->pTopicStr, topic->pTopicStr)) {

            if (prev == NULL) {

                mqttSession->topicList.head = curr->next;

            } else {

                prev->next = curr->next;

            }
            free(curr->pTopicStr);
            free(curr);
            break;
        }
    }
}

/**
 * Free all topic objects associated to given MQTT session
 */
static void freeAllMqttTopics(uWifiMqttSession_t *mqttSession)
{
    uWifiMqttTopic_t *temp;
    uWifiMqttTopic_t *next;

    for (temp = mqttSession->topicList.head; temp != NULL; temp = next) {

        next = temp->next;
        free(temp->pTopicStr);
        free(temp);
    }

    mqttSession->topicList.head = NULL;
    mqttSession->topicList.tail = NULL;
}

static int32_t copyConnectionParams(char **mqttSessionParams, const char *connectionParams)
{
    int32_t len;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;

    if (connectionParams != NULL) {

        len = strlen(connectionParams) + 1;

        if (mqttSessionParams != NULL) {

            *mqttSessionParams = malloc(len);
            memset(*mqttSessionParams, 0, len);
            strcpy(*mqttSessionParams, connectionParams);
            err = U_ERROR_COMMON_SUCCESS;

        }
    }

    return err;
}

/**
 * Establish connection to a given broker. Report the disconnection
 * to the user via a callback in case of connection failure
 */
static int32_t establishMqttConnectionToBroker(uWifiMqttSession_t *mqttSession,
                                               uWifiMqttTopic_t *topic,
                                               bool isPublish)
{
    char url[200];
    uAtClientHandle_t atHandle;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;
    int len;

    memset(url, 0, sizeof(url));

    if (isPublish) {

        len = snprintf(url, sizeof(url), "mqtt://%s:%d/?pt=%s&retain=%d&qos=%d",
                                                         mqttSession->pBrokerNameStr,
                                                         mqttSession->localPort,
                                                         topic->pTopicStr,
                                                         topic->retain,
                                                         topic->qos);

    } else {

        len = snprintf(url, sizeof(url), "mqtt://%s:%d/?st=%s&qos=%d",
                                                        mqttSession->pBrokerNameStr,
                                                        mqttSession->localPort,
                                                        topic->pTopicStr,
                                                        topic->qos);
    }

    if ((mqttSession->pClientIdStr) && (len < sizeof(url))) {
        len += snprintf(&url[len], sizeof(url), "&client=%s", mqttSession->pClientIdStr);
    }

    if ((mqttSession->pUserNameStr) && (len < sizeof(url))) {
        len += snprintf(&url[len], sizeof(url), "&user=%s", mqttSession->pUserNameStr);
    }

    if ((mqttSession->pPasswordStr) && (len < sizeof(url))) {
        len += snprintf(&url[len], sizeof(url), "&passwd=%s", mqttSession->pPasswordStr);
    }



    atHandle = mqttSession->atHandle;
    uPortLog("U_WIFI_MQTT: Sending AT+UDCP\n");
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UDCP=");
    uAtClientWriteString(atHandle, (char *)&url[0], false);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UDCP:");
    topic->peerHandle = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    err = uAtClientUnlock(atHandle);

    if (err == U_ERROR_COMMON_SUCCESS) {
        if (uPortSemaphoreTryTake(mqttSession->semaphore, 5000) != U_ERROR_COMMON_SUCCESS) {
            err = U_ERROR_COMMON_TIMEOUT;
        }
    }
    // Report to user that we are disconnected
    if (err == U_ERROR_COMMON_TIMEOUT) {

        mqttSession->isConnected = false;

        // Remove the topic from the mqtt session
        freeMqttTopic(mqttSession, topic);

        if (mqttSession->disconnectCb) {
            uCallbackEvent_t event = {
                .dataCb = NULL,
                .disconnectCb = mqttSession->disconnectCb,
                .cbParam = mqttSession->cbParam,
                .mqttSession = mqttSession,
                .disconnStatus = err
            };
            uPortEventQueueSend(gCallbackQueue, &event, sizeof(event));
        }
    }
    uPortLog("U_WIFI_MQTT: MQTT connection err = %d\n", err);
    return err;
}

/**
 * Disconnect from a given broker.
 */
static int32_t disconnectMqttConnectionToBroker(uWifiMqttSession_t *mqttSession)
{
    uAtClientHandle_t atHandle;
    uWifiMqttTopic_t *topic;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;

    atHandle = mqttSession->atHandle;

    for (topic = mqttSession->topicList.head; topic != NULL; topic = topic->next) {

        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UDCPC=");
        uAtClientWriteInt(atHandle, topic->peerHandle);
        uAtClientCommandStopReadResponse(atHandle);
        err = uAtClientUnlock(atHandle);
        if (err == U_ERROR_COMMON_SUCCESS) {
            if (uPortSemaphoreTryTake(mqttSession->semaphore, 5000) != U_ERROR_COMMON_SUCCESS) {
                err = U_ERROR_COMMON_TIMEOUT;
            }
        }
        uPortLog("U_WIFI_MQTT: MQTT disconnection err = %d\n", err);
    }
    return err;
}

/**
 *  Callback to handle both data available and disconnection events
 */
static void onCallbackEvent(void *pParam, size_t eventSize)
{
    uCallbackEvent_t *cbEvent = (uCallbackEvent_t *)pParam;
    uWifiMqttSession_t *mqttSession = (uWifiMqttSession_t *)cbEvent->mqttSession;
    int unreadMsgsCount = 0;

    if (cbEvent->dataCb) {
        U_PORT_MUTEX_LOCK(gMqttSessionMutex);

        if (mqttSession != NULL) {
            unreadMsgsCount = mqttSession->unreadMsgsCount;
        }
        U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);

        cbEvent->dataCb(unreadMsgsCount, cbEvent->cbParam);
    } else if (cbEvent->disconnectCb) {

        cbEvent->disconnectCb(cbEvent->disconnStatus, cbEvent->cbParam);
    }
}
/**
 * EDM data callback to store the data in ringbuffer.
 * Data will be stored in the following format
 * (length of message(2 bytes) + edm channel id (1 byte) + original message)
 */

static void edmMqttDataCallback(int32_t edmHandle, int32_t edmChannel, int32_t length,
                              char *pData, void *pCallbackParameter)
{
    uWifiMqttSession_t *mqttSession = NULL;
    uWifiMqttTopic_t *topic;
    int32_t i;
    uint16_t len;
    uint8_t edmChan;

    U_PORT_MUTEX_LOCK(gMqttSessionMutex);

    for (i = 0; i < U_WIFI_MQTT_MAX_NUM_CONNECTIONS; i++) {

        mqttSession = &gMqttSessions[i];

        for (topic = mqttSession->topicList.head; topic != NULL; topic = topic->next) {

            if ((topic->edmChannel == edmChannel) && (topic->isTopicUnsubscribed == false)) {

                uPortLog("U_WIFI_MQTT: EDM data event for channel %d\n", edmChannel);

                len = (uint16_t)length;
                edmChan = (uint8_t)edmChannel;

                // We store the message in ringbuffer in a format (length of message(2 bytes) + edm channel id (1 byte) + original message)
                // Check if we have enough room size to fit all of these.
                if (uRingBufferAvailableSize(&mqttSession->rxRingBuffer) > (3 + length)) {

                    uRingBufferAdd(&mqttSession->rxRingBuffer, (const char *)&len, 2);
                    uRingBufferAdd(&mqttSession->rxRingBuffer, (const char *)&edmChan, 1);
                    uRingBufferAdd(&mqttSession->rxRingBuffer, pData, length);

                    mqttSession->unreadMsgsCount++;
                } else {
                    uPortLog("U_WIFI_MQTT: RX FIFO full, dropping %d bytes!\n", length);
                }

                // Schedule user data dataCb
                if (mqttSession->dataCb) {
                    uCallbackEvent_t event = {
                        .dataCb = mqttSession->dataCb,
                        .disconnectCb = NULL,
                        .cbParam = mqttSession->cbParam,
                        .mqttSession = mqttSession
                    };
                    uPortEventQueueSend(gCallbackQueue, &event, sizeof(event));
                }
            }
        }
    }

    U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
}


static void edmIpConnectionCallback(int32_t edmHandle,
                                    int32_t edmChannel,
                                    uShortRangeConnectionEventType_t eventType,
                                    const uShortRangeConnectDataIp_t *pConnectData,
                                    void *pCallbackParameter)
{

    switch (eventType) {
        case U_SHORT_RANGE_EVENT_CONNECTED:
        uPortLog("U_WIFI_MQTT: EDM connect event for channel %d\n", edmChannel);
        gEdmChannel = edmChannel;
        break;
        case U_SHORT_RANGE_EVENT_DISCONNECTED:
        uPortLog("U_WIFI_MQTT: EDM disconnect event for channel %d\n", edmChannel);
        gEdmChannel = -1;
        break;
    }

}

static void atMqttConnectionCallback(int32_t shortRangeHandle,
                                     int32_t connHandle,
                                     uShortRangeConnectionEventType_t eventType,
                                     uShortRangeConnectDataIp_t *pConnectData,
                                     void *pCallbackParameter)
{
    uWifiMqttSession_t *mqttSession;
    uWifiMqttTopic_t *topic;
    int32_t i;
    bool topicFound = false;

    U_PORT_MUTEX_LOCK(gMqttSessionMutex);

    for (i = 0; i < U_WIFI_MQTT_MAX_NUM_CONNECTIONS; i++) {

        mqttSession = &gMqttSessions[i];

        for (topic = mqttSession->topicList.head; topic != NULL; topic = topic->next) {

            if (topic->peerHandle == connHandle) {

                switch (eventType) {
                    case U_SHORT_RANGE_EVENT_CONNECTED:
                    uPortLog("U_WIFI_MQTT: AT+UUDCPC connect event for connHandle %d\n", connHandle);
                    topic->edmChannel = gEdmChannel;
                    topic->peerHandle = connHandle;
                    topicFound = true;
                    break;
                    case U_SHORT_RANGE_EVENT_DISCONNECTED:
                    uPortLog("U_WIFI_MQTT: AT+UUDCPC disconnect event for connHandle %d\n", connHandle);
                    topic->peerHandle = -1;
                    topic->edmChannel = -1;
                    topicFound = true;
                    mqttSession->isConnected = false;
                    // Report to user that we are disconnected
                    if (mqttSession->disconnectCb) {
                        uCallbackEvent_t event = {
                            .dataCb = NULL,
                            .disconnectCb = mqttSession->disconnectCb,
                            .cbParam = mqttSession->cbParam,
                            .mqttSession = mqttSession,
                            .disconnStatus = U_ERROR_COMMON_SUCCESS
                        };
                        uPortEventQueueSend(gCallbackQueue, &event, sizeof(event));
                    }
                    break;
                }
            }

            if (topicFound) {
                break;
            }
        }

        if (topicFound) {
            break;
        }
    }

    U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
    uPortSemaphoreGive(mqttSession->semaphore);
}

static int32_t getInstance(int32_t wifiHandle, uShortRangePrivateInstance_t **pInstance)
{
    int32_t shoHandle;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;

    shoHandle = uWifiToShoHandle(wifiHandle);
    if (pInstance != NULL) {
        *pInstance = pUShortRangePrivateGetInstance(shoHandle);
        if (*pInstance != NULL) {
            if ((*pInstance)->mode == U_SHORT_RANGE_MODE_EDM) {
                err = U_ERROR_COMMON_SUCCESS;
            }
        } else {
            uPortLog("U_WIFI_MQTT: sho instance failed err = %d\n", err);
        }
    }
    return err;
}

static int32_t getMqttInstance(const uMqttClientContext_t *pContext,
                               uShortRangePrivateInstance_t **pInstance,
                               uWifiMqttSession_t **mqttSession)
{
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;

    err = getInstance(pContext->networkHandle, pInstance);

    if (err == U_ERROR_COMMON_SUCCESS) {

        err = U_ERROR_COMMON_INVALID_PARAMETER;

        if (mqttSession != NULL) {

            err = U_ERROR_COMMON_NOT_INITIALISED;
            *mqttSession = (uWifiMqttSession_t *)pContext->priv;
            if (*mqttSession != NULL) {
                err = U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return err;
}

static void freeMqttSession(uWifiMqttSession_t *mqttSession)
{
    if (mqttSession != NULL) {

        if (mqttSession->pClientIdStr) {
            free(mqttSession->pClientIdStr);
        }
        if (mqttSession->pPasswordStr) {
            free(mqttSession->pPasswordStr);
        }

        if (mqttSession->pUserNameStr) {
            free(mqttSession->pUserNameStr);
        }

        if (mqttSession->pBrokerNameStr) {
            free(mqttSession->pBrokerNameStr);
        }
        if (mqttSession->semaphore) {
            uPortSemaphoreDelete(mqttSession->semaphore);

        }
        if (mqttSession->pRxBuffer) {
            free(mqttSession->pRxBuffer);
            uRingBufferDelete(&mqttSession->rxRingBuffer);
        }

        if (mqttSession->topicList.head) {
            freeAllMqttTopics(mqttSession);
        }

        memset(mqttSession, 0, sizeof(uWifiMqttSession_t));
        mqttSession->sessionHandle = -1;
    }
}

static int32_t initMqttSessions(void)
{
    int32_t err = U_ERROR_COMMON_SUCCESS;

    err = uPortMutexCreate(&gMqttSessionMutex);

    if (err == U_ERROR_COMMON_SUCCESS) {
        for (int32_t i = 0; i < U_WIFI_MQTT_MAX_NUM_CONNECTIONS; i++) {
            freeMqttSession(&gMqttSessions[i]);
        }
    }
    uPortLog("U_WIFI_MQTT: init MQTT session err = %d\n", err);

    return err;
}

static void freeMqtt(uMqttClientContext_t *pContext)
{
    int32_t count = 0;
    uShortRangePrivateInstance_t *pInstance;
    int32_t shoHandle;

    for (int32_t i = 0; i < U_WIFI_MQTT_MAX_NUM_CONNECTIONS; i++) {
        if (gMqttSessions[i].sessionHandle == -1) {
            count++;
        }
    }

    if (count == U_WIFI_MQTT_MAX_NUM_CONNECTIONS) {
        uPortMutexDelete(gMqttSessionMutex);
        gMqttSessionMutex = NULL;
        if (getInstance(pContext->networkHandle, &pInstance) == U_ERROR_COMMON_SUCCESS) {

            shoHandle = uWifiToShoHandle(pContext->networkHandle);

            uShortRangeSetMqttConnectionStatusCallback(shoHandle, NULL, NULL);

            uShortRangeEdmStreamMqttEventCallbackSet(pInstance->streamHandle, NULL, NULL);

            uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle,
                                                     U_SHORT_RANGE_CONNECTION_TYPE_MQTT,
                                                     NULL,
                                                     NULL);
            uPortEventQueueClose(gCallbackQueue);
            gCallbackQueue = U_ERROR_COMMON_NOT_INITIALISED;

        }
    }
}

/**
 * Allocate MQTT session based on the given connection params
 */
static uWifiMqttSession_t *pAllocateMqttSession(const uMqttClientConnection_t *pConnection)
{
    uWifiMqttSession_t *mqttSession = NULL;
    bool outOfMemory = false;
    int32_t err;

    for (int32_t i = 0; i < U_WIFI_MQTT_MAX_NUM_CONNECTIONS; i++) {

        if (gMqttSessions[i].sessionHandle == -1) {

            mqttSession = &gMqttSessions[i];
            mqttSession->sessionHandle = i;


            err = copyConnectionParams(&mqttSession->pBrokerNameStr, pConnection->pBrokerNameStr);

            if (err != U_ERROR_COMMON_SUCCESS) {
                outOfMemory = true;
                break;
            }

            copyConnectionParams(&mqttSession->pClientIdStr, pConnection->pClientIdStr);
            copyConnectionParams(&mqttSession->pUserNameStr, pConnection->pUserNameStr);
            copyConnectionParams(&mqttSession->pPasswordStr, pConnection->pPasswordStr);
            mqttSession->localPort = pConnection->localPort;

            if (uPortSemaphoreCreate(&(mqttSession->semaphore), 0, 1) != U_ERROR_COMMON_SUCCESS) {
                outOfMemory = true;
                break;
            }

            mqttSession->pRxBuffer = malloc(U_WIFI_MQTT_BUFFER_SIZE);

            if (mqttSession->pRxBuffer == NULL) {
                outOfMemory = true;
                break;
            }
            uRingBufferCreate(&mqttSession->rxRingBuffer, mqttSession->pRxBuffer, U_WIFI_MQTT_BUFFER_SIZE);

            break;
        }
    }

    if (outOfMemory) {
        uPortLog("U_WIFI_MQTT: %s Out of memory\n", __func__);
        if (mqttSession) {
            freeMqttSession(mqttSession);
            mqttSession = NULL;
        }
    }

    return mqttSession;
}

/** Initialise the WiFi MQTT client.  If the client is already
 *  initialised then this function returns U_ERROR_COMMON_SUCCESS
 *  @param wifiHandle the handle of the wifi instance to be used.
 */
int32_t uWifiMqttInit(int32_t wifiHandle)
{
    int32_t shoHandle;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = U_ERROR_COMMON_NOT_INITIALISED;

    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {

        if (gMqttSessionMutex == NULL) {

            if (initMqttSessions() == U_ERROR_COMMON_SUCCESS) {

                if (getInstance(wifiHandle, &pInstance) == U_ERROR_COMMON_SUCCESS) {

                    shoHandle = uWifiToShoHandle(wifiHandle);

                    err = uShortRangeSetMqttConnectionStatusCallback(shoHandle,
                                                                     atMqttConnectionCallback,
                                                                     pInstance);
                    if (err == U_ERROR_COMMON_SUCCESS) {

                        err = uShortRangeEdmStreamMqttEventCallbackSet(pInstance->streamHandle,
                                                                       edmIpConnectionCallback,
                                                                       pInstance);
                        if (err != U_ERROR_COMMON_SUCCESS) {
                            uPortLog("U_WIFI_MQTT: EDM IP event cb register failed err = %d\n", err);
                        }

                    } else {

                        uPortLog("U_WIFI_MQTT: MQTT conn status cb register failed err = %d\n", err);
                    }

                    if (err == U_ERROR_COMMON_SUCCESS) {

                        err = uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle,
                                                                      U_SHORT_RANGE_CONNECTION_TYPE_MQTT,
                                                                      edmMqttDataCallback,
                                                                      pInstance);
                        if (err != U_ERROR_COMMON_SUCCESS) {
                            uPortLog("U_WIFI_MQTT: EDM stream event cb register failed err = %d\n", err);
                        }
                    }
                }
            }
        } else {
            err = U_ERROR_COMMON_SUCCESS;
        }
        uShortRangeUnlock();
    } else {
        uPortLog("U_WIFI_MQTT: sho lock failed err = %d\n", err);
    }
    return err;
}

/** Allocate a new MQTT session
 * @param pContext - MQTT client context returned by pUMqttClientOpen()
 * @param pConnection - MQTT connection information for this session
 */
int32_t uWifiMqttConnect(const uMqttClientContext_t *pContext,
                         const uMqttClientConnection_t *pConnection)
{
    uWifiMqttSession_t *mqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;
    uMqttClientContext_t *clientContext = pContext;

    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {
        if (getInstance(pContext->networkHandle, &pInstance) == U_ERROR_COMMON_SUCCESS) {
            U_PORT_MUTEX_LOCK(gMqttSessionMutex);
            if (clientContext->priv == NULL) {
                mqttSession = pAllocateMqttSession(pConnection);
                if (mqttSession != NULL) {
                    mqttSession->atHandle = pInstance->atHandle;
                    clientContext->priv = (void *)mqttSession;
                    // we set isConnected to true eventhough we have not connected to the
                    // real broker. The connection to MQTT broker will occur when the user
                    // invokes publish/Subscribe API
                    mqttSession->isConnected = true;
                    err = U_ERROR_COMMON_SUCCESS;
                } else {
                    uPortLog("U_WIFI_MQTT: Mqtt session NULL\n");
                }
            } else {
                mqttSession = clientContext->priv;
                mqttSession->isConnected = true;
                err = U_ERROR_COMMON_SUCCESS;
            }
            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    } else {
        uPortLog("U_WIFI_MQTT: sho lock failed err = %d\n", err);
    }
    return err;
}

/** Set a callback to be called when new messages are
 * available to be read.  The callback may then call
 * uMqttClientGetUnread() to read the messages.
 *
 * @param pContext        - MQTT client context returned by pUMqttClientOpen().
 *
 * @param pCallback       - the callback. The first parameter to
 *                          the callback will be filled in with
 *                          the number of messages available to
 *                          be read. The second parameter will be
 *                          pCallbackParam.
 *
 * @param pCallbackParam  - this value will be passed to pCallback
 *                          as the second parameter.
 *
 * @return                - zero on success else negative error
 *                          code.
 */
int32_t uWifiMqttSetMessageCallback(const uMqttClientContext_t *pContext,
                                    void (*pCallback) (int32_t, void *),
                                    void *pCallbackParam)
{
    uWifiMqttSession_t *mqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;

    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {
        err = getMqttInstance(pContext, &pInstance, &mqttSession);
        if (err == U_ERROR_COMMON_SUCCESS) {
            U_PORT_MUTEX_LOCK(gMqttSessionMutex);
            mqttSession->dataCb = pCallback;
            mqttSession->cbParam = pCallbackParam;

            if (gCallbackQueue == (int32_t)U_ERROR_COMMON_NOT_INITIALISED) {
                gCallbackQueue = uPortEventQueueOpen(onCallbackEvent,
                                                     "uWifiMqttCallbackQueue",
                                                     sizeof(uCallbackEvent_t),
                                                     U_WIFI_MQTT_DATA_EVENT_STACK_SIZE,
                                                     U_WIFI_MQTT_DATA_EVENT_PRIORITY,
                                                     2 * U_WIFI_MQTT_MAX_NUM_CONNECTIONS);
            }
            err = (gCallbackQueue >= 0) ? U_ERROR_COMMON_SUCCESS : U_ERROR_COMMON_NOT_INITIALISED;
            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }
    return err;
}

/** Set a callback to be called if the MQTT client disconnects
 * from the boker. WiFi MQTT client triggers disconnect callback
 * when the connection to the broker fails, Error code will be set to U_ERROR_COMMON_TIMEOUT in this case
 * when the user invokes uWifiMqttDisconnect() or uWifiMqttClose(), error code will be set to U_ERROR_COMMON_SUCCESS
 *
 * @param pContext       - MQTT client context returned by pUMqttClientOpen().
 *
 * @param pCallback      - the callback. The first parameter is the
 *                         error code,second parameter is pCallbackParam.
 *
 * @param pCallbackParam - this value will be passed to pCallback.
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uWifiMqttSetDisconnectCallback(const uMqttClientContext_t *pContext,
                                       void (*pCallback) (int32_t, void *),
                                       void *pCallbackParam)
{
    uWifiMqttSession_t *mqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;

    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {
        err = getMqttInstance(pContext, &pInstance, &mqttSession);
        if (err == U_ERROR_COMMON_SUCCESS) {
            U_PORT_MUTEX_LOCK(gMqttSessionMutex);
            mqttSession->disconnectCb = pCallback;
            mqttSession->cbParam = pCallbackParam;

            if (gCallbackQueue == (int32_t)U_ERROR_COMMON_NOT_INITIALISED) {
                gCallbackQueue = uPortEventQueueOpen(onCallbackEvent,
                                                     "uWifiMqttCallbackQueue",
                                                     sizeof(uCallbackEvent_t),
                                                     U_WIFI_MQTT_DATA_EVENT_STACK_SIZE,
                                                     U_WIFI_MQTT_DATA_EVENT_PRIORITY,
                                                     2 * U_WIFI_MQTT_MAX_NUM_CONNECTIONS);
            }
            err = (gCallbackQueue >= 0) ? U_ERROR_COMMON_SUCCESS : U_ERROR_COMMON_NOT_INITIALISED;
            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }
    return err;

}

/** Publish topic on connected MQTT session.
 * @param pContext         - MQTT client context returned by pUMqttClientOpen().
 * @param pTopicNameStr    - Pointer to topic string
 * @param pMessage         - Pointer to message buffer that need to be published
 * @param messageSizeBytes - Size of the message buffer
 * @param qos              - QOS of the message
 * @param retain           - Set to true if the message need to be retained by the broker
 *                           between connect and disconnect
 * @return                 - zero on success or negative error code.
 */
int32_t uWifiMqttPublish(const uMqttClientContext_t *pContext,
                               const char *pTopicNameStr,
                               const char *pMessage,
                               size_t messageSizeBytes,
                               uMqttQos_t qos,
                               bool retain)
{
    uWifiMqttSession_t *mqttSession;
    uWifiMqttTopic_t *topic;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;

    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {

        if (getMqttInstance(pContext, &pInstance, &mqttSession) == U_ERROR_COMMON_SUCCESS) {

            topic = findTopic(mqttSession, pTopicNameStr, true);

            if (topic == NULL) {

                topic = pAllocateMqttTopic(mqttSession, true);

                if (topic != NULL) {

                    topic->retain = retain;
                    topic->qos = qos;

                    err = copyConnectionParams(&topic->pTopicStr, pTopicNameStr);

                    if (err == U_ERROR_COMMON_SUCCESS) {
                        err = establishMqttConnectionToBroker(mqttSession, topic, true);
                    }

                }

            } else {

                err = U_ERROR_COMMON_SUCCESS;

            }

            if (err == U_ERROR_COMMON_SUCCESS) {
                err = uShortRangeEdmStreamWrite(pInstance->streamHandle,
                                                topic->edmChannel,
                                                pMessage,
                                                messageSizeBytes,
                                                U_WIFI_MQTT_WRITE_TIMEOUT_MS);
                uPortLog("EDM write for channel %d message bytes %d written bytes %d\n", topic->edmChannel,
                                                                                         messageSizeBytes,
                                                                                         err);
            }
            if (err == messageSizeBytes) {
                err = U_ERROR_COMMON_SUCCESS;
            }
        }
        uShortRangeUnlock();
    }

    return err;
}

/** Subscribe topic on connected MQTT session.
 * @param pContext         - MQTT client context returned by pUMqttClientOpen().
 * @param pTopicFilterStr  - Pointer to topic string that need to be subscribed
 * @param maxQos           - QOS of the message

 * @return                 - zero on success or negative error code.
 */
int32_t uWifiMqttSubscribe(const uMqttClientContext_t *pContext,
                                 const char *pTopicFilterStr,
                                 uMqttQos_t maxQos)
{
    uWifiMqttSession_t *mqttSession;
    uShortRangePrivateInstance_t *pInstance;
    uWifiMqttTopic_t *topic;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;

    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {

        if (getMqttInstance(pContext, &pInstance, &mqttSession) == U_ERROR_COMMON_SUCCESS) {

            topic = findTopic(mqttSession, pTopicFilterStr, false);

            if (topic == NULL) {

                topic = pAllocateMqttTopic(mqttSession, false);

                if (topic != NULL) {

                    topic->qos = maxQos;

                    err = copyConnectionParams(&topic->pTopicStr, pTopicFilterStr);

                    if (err == U_ERROR_COMMON_SUCCESS) {
                        err = establishMqttConnectionToBroker(mqttSession, topic, false);
                    }
                }

            } else {

                topic->isTopicUnsubscribed = false;
                err = U_ERROR_COMMON_SUCCESS;
            }
        }
        uShortRangeUnlock();
    }

    return err;
}

/** Unsubscribe from topic on connected MQTT session.
 * @param pContext         - MQTT client context returned by pUMqttClientOpen().
 * @param pTopicFilterStr  - Pointer to topic string that need to be unsubscribed
 * @return                 - zero on success or negative error code.
 */
int32_t uWifiMqttUnsubscribe(const uMqttClientContext_t *pContext,
                                   const char *pTopicFilterStr)
{
    uWifiMqttSession_t *mqttSession;
    uWifiMqttTopic_t *topic;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;


    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {

        if (getMqttInstance(pContext, &pInstance, &mqttSession) == U_ERROR_COMMON_SUCCESS) {

            U_PORT_MUTEX_LOCK(gMqttSessionMutex);

            topic = findTopic(mqttSession, pTopicFilterStr, false);

            if (topic != NULL) {
                topic->isTopicUnsubscribed = true;
                err = U_ERROR_COMMON_SUCCESS;
            } else {
                uPortLog("U_WIFI_MQTT: Topic not found in session %p\n", mqttSession);
            }

            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }

    return err;
}

/** Disconnect from MQTT broker
 * @param pContext - MQTT client context returned by pUMqttClientOpen().
 * @return         -  zero on success or negative error code.
 */
int32_t uWifiMqttDisconnect(const uMqttClientContext_t *pContext)
{
    uWifiMqttSession_t *mqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;
    bool isMqttConnected = false;

    isMqttConnected = uWifiMqttIsConnected(pContext);


    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {

        if (getMqttInstance(pContext, &pInstance, &mqttSession) == U_ERROR_COMMON_SUCCESS) {

            if (isMqttConnected) {
                err = disconnectMqttConnectionToBroker(mqttSession);
            } else {
                err = U_ERROR_COMMON_SUCCESS;
            }
        }
        uShortRangeUnlock();
    }
    return err;
}

/** Close connected MQTT session. This API will disconnect from the broker if connected
 * followed by releasing all the resources associated to that particular session
 * @param pContext         - MQTT client context returned by pUMqttClientOpen().
 * @return                 - zero on success or negative error code.
 */
void uWifiMqttClose(uMqttClientContext_t *pContext)
{
    uWifiMqttSession_t *mqttSession;
    uShortRangePrivateInstance_t *pInstance;
    bool isMqttConnected = false;

    isMqttConnected = uWifiMqttIsConnected(pContext);


    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {

        if (getMqttInstance(pContext, &pInstance, &mqttSession) == U_ERROR_COMMON_SUCCESS) {

            if (isMqttConnected) {
                disconnectMqttConnectionToBroker(mqttSession);
                pContext->priv = NULL;
            }
            freeMqttSession(mqttSession);
            freeMqtt(pContext);
        }
        uShortRangeUnlock();
    }
}

/** Get total number of unread messages in a given MQTT session.
 * @param pContext         - MQTT client context returned by pUMqttClientOpen().
 * @return                 - zero on success or negative error code.
 */
int32_t uWifiMqttGetUnread(const uMqttClientContext_t *pContext)
{
    uWifiMqttSession_t *mqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t unReadMsgsCount = 0;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;

    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {

        if (getMqttInstance(pContext, &pInstance, &mqttSession) == U_ERROR_COMMON_SUCCESS) {

            U_PORT_MUTEX_LOCK(gMqttSessionMutex);
            unReadMsgsCount = mqttSession->unreadMsgsCount;
            err = U_ERROR_COMMON_SUCCESS;
            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }

    return unReadMsgsCount;
}

/** Read messages and their corresponding topics for a given MQTT session.
 * @param pContext           - MQTT client context returned by pUMqttClientOpen().
 * @param pTopicNameStr      - User should provide empty buffer of topicNameSizeBytes
 * @param topicNameSizeBytes - topicNameSizeBytes should be >= minimum length of topic string
 * @param pMessage           - User should provide empty buffer of size pMessageSizeBytes.
 * @param pMessageSizeBytes  - pMessageSizeBytes should be >= minimum length of topic buffer
 * @param pQos               - Retrieve the QOS of the message.
 * @return                   - zero on success or negative error code.
 *
 */
int32_t uWifiMqttMessageRead(const uMqttClientContext_t *pContext,
                               char *pTopicNameStr,
                               size_t topicNameSizeBytes,
                               char *pMessage,
                               size_t *pMessageSizeBytes,
                               uMqttQos_t *pQos)
{
    uWifiMqttSession_t *mqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = U_ERROR_COMMON_INVALID_PARAMETER;
    uint16_t msgLen = 0;
    uint8_t edmChannel;
    char *foundTopicStr;
    size_t foundTopicLen;
    char ignoreMsg;

    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {

        if (getMqttInstance(pContext, &pInstance, &mqttSession) == U_ERROR_COMMON_SUCCESS) {

            U_PORT_MUTEX_LOCK(gMqttSessionMutex);

            if ((pTopicNameStr != NULL) &&
                (pMessage != NULL) &&
                (pMessageSizeBytes != NULL)) {

                memset(pMessage, 0, *pMessageSizeBytes);
                memset(pTopicNameStr, 0, topicNameSizeBytes);

                // First 2 bytes represent length of the message content
                uRingBufferRead(&mqttSession->rxRingBuffer, (char *)&msgLen, 2);
                // Next 1 byte represents edm channel used for the topic
                uRingBufferRead(&mqttSession->rxRingBuffer, &edmChannel, 1);
                foundTopicStr = getTopicStrForEdmChannel(mqttSession, (int32_t)edmChannel);
                foundTopicLen = strlen(foundTopicStr);

                // check if the message can be accomodated in the given buffer size
                if ((foundTopicStr != NULL) &&
                    (msgLen <= *pMessageSizeBytes) &&
                    ((foundTopicLen + 1) <= topicNameSizeBytes)) {

                    strcpy(pTopicNameStr, foundTopicStr);
                    uRingBufferRead(&mqttSession->rxRingBuffer, pMessage, msgLen);
                    *pMessageSizeBytes = msgLen;
                    err = U_ERROR_COMMON_SUCCESS;

                } else {

                    // Drop the message if application provides buffer with lesser size

                    for (uint16_t i = 0; i < msgLen; i++) {
                        uRingBufferRead(&mqttSession->rxRingBuffer, &ignoreMsg, 1);
                    }

                    err = U_ERROR_COMMON_NO_MEMORY;
                }
                mqttSession->unreadMsgsCount--;
            }
            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }

    return err;
}

/** Check if we are connected to the given MQTT session.
 * @param pContext           - MQTT client context returned by pUMqttClientOpen().
 * @return                   - true/false
 */
bool uWifiMqttIsConnected(const uMqttClientContext_t *pContext)
{
    uWifiMqttSession_t *mqttSession;
    uShortRangePrivateInstance_t *pInstance;
    bool isConnected = false;

    if (uShortRangeLock() == U_ERROR_COMMON_SUCCESS) {

        if (getMqttInstance(pContext, &pInstance, &mqttSession) == U_ERROR_COMMON_SUCCESS) {

            U_PORT_MUTEX_LOCK(gMqttSessionMutex);

            isConnected = mqttSession->isConnected;

            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }

    return isConnected;
}