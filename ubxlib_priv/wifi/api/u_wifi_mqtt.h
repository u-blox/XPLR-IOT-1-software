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

#ifndef _U_WIFI_MQTT_H_
#define _U_WIFI_MQTT_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the MQTT APIs for WiFi.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef U_WIFI_MQTT_BUFFER_SIZE
#define U_WIFI_MQTT_BUFFER_SIZE 4096
#endif

#ifndef U_WIFI_MQTT_WRITE_TIMEOUT_MS
#define U_WIFI_MQTT_WRITE_TIMEOUT_MS 500
#endif

/** The maximum number of connections that can be open at one time.
 */
#define U_WIFI_MQTT_MAX_NUM_CONNECTIONS 7


typedef enum {
    U_WIFI_MQTT_QOS_AT_MOST_ONCE = 0,
    U_WIFI_MQTT_QOS_AT_LEAST_ONCE = 1,
    U_WIFI_MQTT_QOS_EXACTLY_ONCE = 2,
    U_WIFI_MQTT_QOS_MAX_NUM
} uWifiMqttQos_t;
/* ----------------------------------------------------------------
 * FUNCTIONS:
 * -------------------------------------------------------------- */
/** Initialise the WiFi MQTT client.  If the client is already
 *  initialised then this function returns U_ERROR_COMMON_SUCCESS
 *  @param wifiHandle the handle of the wifi instance to be used.
 */
int32_t uWifiMqttInit(int32_t wifiHandle);

/** Allocate a new MQTT session
 * @param pContext - MQTT client context returned by pUMqttClientOpen()
 * @param pConnection - MQTT connection information for this session
 */
int32_t uWifiMqttConnect(const uMqttClientContext_t *pContext,
                         const uMqttClientConnection_t *pConnection);

/** Disconnect from MQTT broker
 * @param pContext - MQTT client context returned by pUMqttClientOpen().
 * @return         -  zero on success or negative error code.
 */
int32_t uWifiMqttDisconnect(const uMqttClientContext_t *pContext);

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
                               bool retain);

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
 *                          pCallbackParam.  Use NULL to deregister
 *                          a previous callback.
 *
 * @param pCallbackParam  - this value will be passed to pCallback
 *                          as the second parameter.
 *
 * @return                - zero on success else negative error
 *                          code.
 */
int32_t uWifiMqttSetMessageCallback(const uMqttClientContext_t *pContext,
                                    void (*pCallback) (int32_t, void *),
                                    void *pCallbackParam);

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
                                    void *pCallbackParam);

/** Subscribe topic on connected MQTT session.
 * @param pContext         - MQTT client context returned by pUMqttClientOpen().
 * @param pTopicFilterStr  - Pointer to topic string that need to be subscribed
 * @param maxQos           - QOS of the message

 * @return                 - zero on success or negative error code.
 */
int32_t uWifiMqttSubscribe(const uMqttClientContext_t *pContext,
                                 const char *pTopicFilterStr,
                                 uMqttQos_t maxQos);

/** Unsubscribe from topic on connected MQTT session.
 * @param pContext         - MQTT client context returned by pUMqttClientOpen().
 * @param pTopicFilterStr  - Pointer to topic string that need to be unsubscribed
 * @return                 - zero on success or negative error code.
 */

int32_t uWifiMqttUnsubscribe(const uMqttClientContext_t *pContext,
                                   const char *pTopicFilterStr);

/** Close connected MQTT session. This API will disconnect from the broker if connected
 * followed by releasing all the resources associated to that particular session
 * @param pContext         - MQTT client context returned by pUMqttClientOpen().
 * @return                 - zero on success or negative error code.
 */
void uWifiMqttClose(uMqttClientContext_t *pContext);

/** Get total number of unread messages in a given MQTT session.
 * @param pContext         - MQTT client context returned by pUMqttClientOpen().
 * @return                 - zero on success or negative error code.
 */
int32_t uWifiMqttGetUnread(const uMqttClientContext_t *pContext);

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
                               uMqttQos_t *pQos);

/** Check if we are connected to the given MQTT session.
 * @param pContext           - MQTT client context returned by pUMqttClientOpen().
 * @return                   - true/false
 */
bool uWifiMqttIsConnected(const uMqttClientContext_t *pContext);


#ifdef __cplusplus
}
#endif

#endif // _U_WIFI_MQTT_H_

// End of file
