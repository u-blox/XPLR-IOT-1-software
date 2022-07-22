/*
 * Copyright 2022 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/** @file
 * @brief This file contains "modules" shell commands implementation and
 * defines the shell commands structure for the "modules" root command of the
 * XPLR-IOT-1 Sensor Aggregation Use Case. It is about control of u-blox modules
 * and MQTT(SN) clients control.
 */


#include <shell/shell.h>

#include "x_pos_maxm10s.h"
#include "x_wifi_ninaW156.h"
#include "x_cell_saraR5.h"
#include "x_wifi_mqtt.h"
#include "x_cell_mqttsn.h"



/* ----------------------------------------------------------------
 * COMMAND FUNCTION IMPLEMENTATION (STATIC)
 * -------------------------------------------------------------- */

static void xModulesCmdTypeStatus(const struct shell *shell, size_t argc, char **argv)
{
        ARG_UNUSED(argc);
        ARG_UNUSED(argv);
        
        char *thread_status_str[2]= {"Suspended", "Running"}; // holds 2 strings
        char *comm_str[2] = {"USB to UART comm","NORA comm"};
        char *powered_str[2] = {"Powered Off", "Powered On"};
        char *initialized_str[2] = {"Not Iniatialized", "Initialized"};
        char *ubxlibStatus_str[4] = {"Not Initialized", "Initialized", "Net Initialized", "Net Added"};
        char *yesNoBoolStr[2] = {"No","Yes"};


        // //MAXM10
        xPosMaxM10Status_t maxstatus = xPosMaxM10GetModuleStatus();
        // just print the current status of max
        shell_print(shell,"\r\n\
MAXM10S --------------------------------\r\n\
        - Power status: %s\r\n\
        - Active Serial Comm: %s\r\n\
        - Init status: %s \r\n\
        - Thread_status %s \r\n\
        - Update Period Setting: %d ms \r\n\
        - Timeout Setting: %d ms\r\n",
        powered_str[ (int)maxstatus.isPowered ],\
        comm_str[ (int)maxstatus.com ],\
        initialized_str[ (int)maxstatus.isUbxInit ],\
        thread_status_str[ (int)maxstatus.isEnabled ],\
        maxstatus.updatePeriod, maxstatus.timeoutPeriod);

        xWifiNinaStatus_t ninastatus = xWifiNinaGetModuleStatus();

        shell_print(shell,"\r\n\
NINAW156 --------------------------------\r\n\
        - Power status: %s\r\n\
        - Active Serial Comm: %s\r\n\
        - ubxlib status: %s \r\n\
        - Connected: %s \r\n",
        powered_str[ (int)ninastatus.isPowered ],\
        comm_str[ (int)ninastatus.com ],\
        ubxlibStatus_str[ (int)ninastatus.uStatus ],\
        yesNoBoolStr[ (int)ninastatus.isConnected ]);

        xCellSaraStatus_t sarastatus = xCellSaraGetModuleStatus();
        

        shell_print(shell,"\r\n\
SARAR5 --------------------------------\r\n\
        - Power status: %s\r\n\
        - ubxlib status: %s \r\n\
        - Registered: %s \r\n\
        - Connected: %s \r\n",
        powered_str[ (int)sarastatus.poweredUp ],\
        ubxlibStatus_str[ (int)sarastatus.uStatus ],\
        yesNoBoolStr[ (int)sarastatus.isRegistered ],\
        yesNoBoolStr[ (int)sarastatus.isConnected ]);

        xWifiMqttClientStatusCmd(shell, argc, argv);
        xCellMqttSnClientStatusCmd(shell, argc, argv);

        return;
}



/* ----------------------------------------------------------------
 * DEFINE MODULES SHELL COMMAND MENU
 * -------------------------------------------------------------- */

SHELL_STATIC_SUBCMD_SET_CREATE(MAXM10S,
        SHELL_CMD(power_on, NULL, "Only Powers On MAXM10S module", xPosMaxM10PowerOn),
        SHELL_CMD(power_off, NULL, "Powers Off MAXM10S module", xPosMaxM10PowerOff),
        SHELL_CMD(enable, NULL, "Enable MAXM10S measurements (also powers on)", xPosMaxM10Enable),
        SHELL_CMD(disable,NULL, "Disable MAXM10S measurements", xPosMaxM10Disable),
        SHELL_CMD(set_period,NULL, "Set MAXM10S period in ms", xPosMaxM10UpdatePeriodCmd),
        SHELL_CMD(set_timeout,NULL, "Set MAXM10S timeout period in ms", xPosMaxM10TimeoutPeriodCmd),
        SHELL_CMD(comm=nora,NULL, "Set MAXM10S serial comm: nora", xPosMaxM10EnableNoraCom),
        SHELL_CMD(comm=usb,NULL, "Set MAXM10S serial comm: usb", xPosMaxM10DisableNoraCom),
        SHELL_CMD(publish,NULL, "Publish MaxM10S measurements: parameters on/off. Eg: publish on ", xPosMaxM10EnablePublishCmd),
        SHELL_SUBCMD_SET_END
);


SHELL_STATIC_SUBCMD_SET_CREATE(NINAW156,
        SHELL_CMD(power_on, NULL, "Power On NINA-W156 module", xWifiNinaPowerOn),
        SHELL_CMD(power_off, NULL, "Power Off NINAW156 module (also disconnect,deconfig if necessary)", xWifiNinaPowerOff),
        SHELL_CMD(init,NULL, "Initializes NINAW156 and saved network. Prepare for connection", xWifiNinaInit),
        SHELL_CMD(deinit,NULL, "Deinitializes network", xWifiNinaDeinit),
        SHELL_CMD(connect, NULL, "Connect to saved WiFi network", xWifiNinaConnect),
        SHELL_CMD(disconnect,NULL, "Disconnect from WiFi network", xWifiNinaDisconnect),
        SHELL_CMD(provision,NULL, "Provide WiFi network credentials: provision <SSID> <Password> : if open network do not provide <Password>", xWifiNinaProvisionCmd),
        SHELL_CMD(type_cred,NULL, "Type WiFi Network credentials (active/saved/provided for next setup)", xWifiNinaTypeNetworkParamsCmd),
        SHELL_CMD(comm=nora,NULL, "Set NINAW156 serial comm: nora", xWifiNinaEnableNoraCom),
        SHELL_CMD(comm=usb,NULL, "Set NINAW156 serial comm: usb", xWifiNinaDisableNoraCom),
        SHELL_SUBCMD_SET_END
);


SHELL_STATIC_SUBCMD_SET_CREATE(MQTT,
        SHELL_CMD(open, NULL, "Open MQTT client (prepare for connection)", xWifiMqttClientOpen),
        SHELL_CMD(connect, NULL, "(Open and) Connect MQTT client", xWifiMqttClientConnect),
        SHELL_CMD(close, NULL, "Disconnect and close MQTT client", xWifiMqttClientClose),
        // using the disconnect command without closing and then reconnecting, creates problems if then the user,
        // wants to send a message
        //SHELL_CMD(disconnect, NULL, "disconnect MQTT client", xWifiMqttClientDisconnect),  
        SHELL_CMD(save, NULL, "Save MQTT credentials: save <Device ID> <Username> <Password>", xWifiMqttSaveConfigCmd),
        SHELL_CMD(type, NULL, "Type Saved/Active MQTT credentials", xWifiMqttTypeConfigCmd),
        SHELL_CMD(status, NULL, "Get MQTT client status", xWifiMqttClientStatusCmd),
        SHELL_CMD(send, NULL, "Send MQTT Message: send <topic> <message> <QOS>   QOS values:0,1,2", xWifiMqttSendCmd),
        SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(MQTTSN,
        SHELL_CMD(open, NULL, "Open MQTT-SN client. Prepare for connection", xCellMqttSnClientOpen),
        SHELL_CMD(close, NULL, "Close MQTT-SN client", xCellMqttSnClientClose),
        SHELL_CMD(connect, NULL, "Connect MQTT-SN client", xCellMqttSnClientConnect),
        SHELL_CMD(disconnect, NULL, "Disconnect MQTT-SN client", xCellMqttSnClientDisconnect),
        SHELL_CMD(save, NULL, "Save MQTT-SN Setting: <plan> <Identity> <Connection duration (seconds):if anywhere enabled>    plan:<anywhere,flex>", xCellMqttSnSaveConfigCmd),
        SHELL_CMD(type, NULL, "Type Saved MQTT-SN Settings", xCellMqttSnTypeConfigCmd),
        SHELL_CMD(status, NULL, "Get MQTT-SN client status", xCellMqttSnClientStatusCmd),
        SHELL_CMD(send, NULL, "Send MQTT-SN Message: send <type> <topic> <message> <QOS>   type: <normal/short/pre>  QOS:0/1/2/3", xCellMqttSnSendCmd),
        SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(plans,
        SHELL_CMD(flex, NULL, "MQTT Flex plan", xCellSaraActivateMqttFlexPlan),
        SHELL_CMD(anywhere, NULL, "MQTT Anywhere plan", xCellSaraActivateMqttAnywherePlan),
        SHELL_CMD(get_active, NULL, "Get active cellular plan", xCellSaraGetActiveMqttPlanCmd),
        SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(SARAR5,
        SHELL_CMD(plans, &plans, "Choose plan", NULL),
        SHELL_CMD(power_on, NULL, "power up module", xCellSaraPowerOn),
        SHELL_CMD(power_off, NULL, "power off module", xCellSaraPowerOff),
        SHELL_CMD(init, NULL, "config", xCellSaraInit),
        SHELL_CMD(deinit, NULL, "disconnect, deinit and power down", xCellSaraDeinit),
        SHELL_CMD(connect, NULL, "connect", xCellSaraConnect),
        //this command cannot be used because it also powers off the module, use deinit instead
        //SHELL_CMD(disconnect, NULL, "disconnect", saraR5_disconnect),   
        SHELL_SUBCMD_SET_END
);


/* Creating subcommands (level 1 command) array for command "sensors". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_modules,
       SHELL_CMD(MAXM10S, &MAXM10S, "MAXM10S control", NULL),
       SHELL_CMD(NINAW156, &NINAW156, "NINAW156 control", NULL),
       SHELL_CMD(SARAR5, &SARAR5, "SARAR5 control", NULL),
       SHELL_CMD(MQTT, &MQTT, "MQTT control", NULL),
       SHELL_CMD(MQTTSN, &MQTTSN, "MQTTSN control", NULL),
       SHELL_CMD(status,   NULL, "Type u-blox modules status", xModulesCmdTypeStatus),
       SHELL_SUBCMD_SET_END
);

/* Creating root (level 0) command "Sensors" without a handler */
SHELL_CMD_REGISTER(modules, &sub_modules, "u-blox module control commands", NULL);