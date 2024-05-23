/**************************************************************************
 
 * Contractor Name: 
 *
 * File Name: MAIN.C
 *
 *
 * File Overview:
 *  tool to set IP address
 *
 *
 *
 *
 *
 * Revision History:
 *
 * Author Date Description
 * B.Norrod         01/01/2023
 *
 *
 *
 *
 * ***********************************************************************/
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include "../include/header.h"

int socket_indicator;
int loop_num;
long iHeartBeat = 0;
int iHeartBeatLastState = 0;
long lHeartBeatInterval = 2000000; // heartbeat flash rate count

int main()
{
    //
    // Main entry point.
    //
    //
    //
    int DEBUG_TCP_RCV = 0;
    int iLooper = 0;
    int iLooper2 = 0;
    int PacketType;
    int iMsgLen;
    int iValue;
    int Verbose = 0;
    int pruInitFail = 0;
    int status;
    int tcp_Data;
    int new_SSID;
    int last_SSID = -1;

    char tcp_stack_buffer[1024];
    char IPaddr[20];
    char hostname[80];
    char Payload_In[1024];
    char Payload_Out[1024];
    char *token;
    char *param;
    char *paramvalue;
    char *paramPtr;
    char *stack_token;
    char *stackPtr;
    char LogItem[512];
    char strStateLookup[64];
    char SourceDIR[512];
    char DestDIR[512];
    char FileName[512];
    char ProgramIdent[80];

    double elapsed_time;
    double *tempSensorList;

    clock_t start_time = clock();
    struct timeval tv1, tv2, tv3;

    Mission_ID = 1;
    Mission_RUN = 0;
    TemperatureReal = 25;
    Mission_TRead_ID = set_TemperatureLUTpointer((double)TemperatureReal);
    UART_ENABLE = 0;

    /********************************************************************************************/
    /*   L o a d   C o n f i g u r a t i o n   F i l e                                          */
    /********************************************************************************************/
    sprintf(LogItem, "Loaded Configfile: %d", load_config_data("/home/ctuser/vfs/data/config.txt"));
    write_log(LogItem, DEBUG);

    sprintf(ProgramIdent, VERSION);
    sprintf(LogItem, "Starting VFS %s", ProgramIdent);
    write_log(LogItem, DEBUG);

    gethostname(hostname, 80);
    GetIPaddr(IPaddr);

    init_gpio();
    // flash_activity_leds(LED_ALLRND, 0);
    sprintf(LogItem, "GPIO Initialized");
    write_log(LogItem, DEBUG);

    uart485_init();
    enableRS485(FALSE);

    // Extract_lut_data();
    // exit(0);

    /********************************************************************************************/
    /*   P R U   I n i t a l i z a t i o n                                                      */
    /********************************************************************************************/
    do
    {
        pruInitFail = init_pru();
        if (pruInitFail >= 0)
            break;

        // we are fighting a race condition where
        // we're waiting for the PRU services to come online.

        sprintf(LogItem, "Failed to open %s", DEVICE_NAME1);
        write_log(LogItem, DEBUG);
        printf("pru\n");

        // build in some delay
        for (iLooper2 = 0; iLooper2 < 6; iLooper2++)
            flash_activity_leds(LED_MARQUEE, 0);

        iLooper++;
    } while (iLooper < 50 & pruInitFail < 0);

    if (pruInitFail < 0)
    {
        flash_activity_leds(LED_CRASH_OUT, 0);
        sprintf(LogItem, "PRU INIT failure");
        write_log(LogItem, DEBUG);
        exit(88);
    }
    sprintf(LogItem, "PRU INIT Complete");
    write_log(LogItem, DEBUG);

    /********************************************************************************************/
    /*   D A C    I n i t a l i z a t i o n                                                     */
    /********************************************************************************************/
    init_DACs();
    sprintf(LogItem, "DAC INIT Complete");
    write_log(LogItem, DEBUG);

    /********************************************************************************************/
    /*  Fetch Module ID that is set by the dip switches.                                        */
    /********************************************************************************************/
    Module_HWID = readMODID();
    sprintf(LogItem, "MODID read complete %i", Module_HWID);
    write_log(LogItem, DEBUG);

    flash_activity_leds(LED_ALLRND, 0);

    GetIPaddr(IPaddr);
    sprintf(LogItem, "********************* %s\n", IPaddr);
    write_log(LogItem, DEBUG);

    if (Module_HWID > 254 | Module_HWID < 1)
    {
        strcpy(Payload_In, IPaddr);
        stack_token = strtok(Payload_In, ".");
        stack_token = strtok(NULL, ".");
        stack_token = strtok(NULL, ".");
        stack_token = strtok(NULL, ".");
        Module_HWID = atoi(stack_token);
    }
    else if (AUTO_IP_SET)
    {
        printf("Setting IP address\n");
        autoIPconfig(Module_HWID);
    }

    sprintf(LogItem, "VFS Module: Address=%3i", Module_HWID);
    write_log(LogItem, DEBUG);
    if (DEBUG)
        printf("%s\n", LogItem);

    sprintf(LogItem, "Running Client Name: %s @ %s", hostname, IPaddr);
    write_log(LogItem, DEBUG);

    /********************************************************************************************/
    /*      M a i n   R e c o v e r y   L o o p                                                 */
    /********************************************************************************************/
    do
    {

        /********************************************************************************************/
        /*   S e a r c h i n g   f o r   H o s t                                                    */
        /********************************************************************************************/
        enableRS485(FALSE);
        new_SSID = read_UART485();
        do
        {
            sprintf(LogItem, "Searching for System Host@%s:%s", SYS_HOST_IP, SYS_HOST_PORT);
            ////   write_log(LogItem, 0);
            if (DEBUG)
                printf("%s\n", LogItem);
            flash_activity_leds(LED_ALLRND, 0);

            status = makeConnection(SYS_HOST_IP, SYS_HOST_PORT, &socket_1_handle);
            if (status != OK | socket_1_handle < 0)
            {
                sprintf(LogItem, "Failed to Connect.   Auto-Retry:ON");
                ////     write_log(LogItem, 0);
                flash_activity_leds(LED_ALLRND, 0);
            }

        } while (status != OK | socket_1_handle < 0);

        srand(time(NULL));
        sprintf(LogItem, "Connection from VFS(%s):%03i : %s @ %s", ProgramIdent, Module_HWID, hostname, IPaddr);
        write_log(LogItem, DEBUG);
        send_status(1000, LogItem);

        /********************************************************************************************/
        /*    POP off a few temperature reads.  First read sometimes yields a bogus return.         */
        /********************************************************************************************/
        tempSensorList = Read_ALL_TempSensor();
        tempSensorList = Read_ALL_TempSensor();
        tempSensorList = Read_ALL_TempSensor();

        /********************************************************************************************/
        /*    If Enabled, Load LUT tables                                                           */
        /********************************************************************************************/
        if (0)  /// moved to load after credetials are sent.
        {
            if (load_LUT_Data() != OK)
            {
                sprintf(LogItem, "MOD_ID%03i:LUT Data RAM load: Failed!", Module_HWID);
                // send_status(1010, LogItem);
            }
            else
            {
                sprintf(LogItem, "MOD_ID%03i:LUT Data RAM load: Success!", Module_HWID);
                // send_status(1011, LogItem);
            }

            write_log(LogItem, DEBUG);
        }

        Mission_ID = 0;
        Mission_RUN = 0;
        TemperatureReal = 25;
        Mission_TRead_ID = set_TemperatureLUTpointer((double)TemperatureReal);
        UART_ENABLE = 0;

        // Set to default state
        LUT_lookup(strStateLookup, 21);
        setVFSstate(strStateLookup);

        new_SSID = read_UART485();
        UART_ENABLE = 0;
        enableRS485(UART_ENABLE);

        /********************************************************************************************/
        /*      M a i n   P r o c e s s  L o o p                                                    */
        /********************************************************************************************/
        while (1)
        {

            /********************************************************************************************/
            /*           check for 485 data on UART1                                                    */
            /********************************************************************************************/
            if (UART_ENABLE)
            {
                new_SSID = read_UART485();
                if (new_SSID > -1)
                {
                    if (last_SSID != new_SSID)
                    {
                        LUT_lookup(strStateLookup, new_SSID);
                        setVFSstate(strStateLookup);
                        if (0)
                            printf("UART D=%03i     %s\n", new_SSID, strStateLookup);

                        if (ECHO_UART)
                        {
                            flash_activity_leds(LED_TRAFFIC_IN_STATIC, 0);
                            sprintf(Payload_Out, "UART ECHO:%03i, SSID:%03i , Payload:%s", Module_HWID, new_SSID, strStateLookup);
                            SendMessage((int)TYPE_ECHO, Module_HWID, ClientAssigned_ID, Payload_Out);
                        }
                        last_SSID = new_SSID;
                    }
                }
            }

            /********************************************************************************************/
            /*         C h e c k   f o r   I n c o m i n g   T C P    D a t a                            */
            /********************************************************************************************/
            tcp_Data = getData(socket_1_handle, tcp_stack_buffer);
            if (tcp_Data > 0)
            {
                flash_activity_leds(LED_TRAFFIC_IN_STATIC, 1);

                if (DEBUG_TCP_RCV)
                    printf("payload:  %s %i\n", tcp_stack_buffer, tcp_Data);

                // gettimeofday(&tv1, NULL);
                //   The tcp buffer may contain more then one command packet.
                //   So we need to parse through it and execute eack sub-packet
                stack_token = strtok_r(tcp_stack_buffer, ";", &stackPtr);

                while (stack_token != NULL)
                {
                    /********************************************************************************************/
                    /*        We have TCP Data !                                                                */
                    /********************************************************************************************/
                    // int Module_HWID=1;
                    // int ClientAssigned_ID=0;
                    // Strip off msg type (2-byte hex value)

                    PacketType = ParseOutHEX2INTvalue(stack_token, 0, 2);
                    ClientAssigned_ID = ParseOutHEX2INTvalue(stack_token, 6, 4);
                    iMsgLen = ParseOutHEX2INTvalue(stack_token, 10, 4);
                    substring(stack_token, Payload_In, 14, iMsgLen);

                    if (DEBUG_TCP_RCV)
                        printf("PAYLOAD:   %s, PKT:%i\n", stack_token, PacketType);

                    // set default payload_out
                    sprintf(Payload_Out, "ACK:%s", Payload_In);

                    /********************************************************************************************/
                    /*        Do Nothing
                    /********************************************************************************************/
                    if (PacketType == TYPE_NOOP)
                    {
                    }

                    /********************************************************************************************/
                    /*        TYPE_MISSION_RUN_STATE
                    /********************************************************************************************/
                    else if (PacketType == TYPE_MISSION_RUN_STATE)
                    {
                        token = strtok_r(Payload_In, ";", &paramPtr);
                        paramvalue = strtok(token, ",");
                        iValue = atoi(paramvalue);
                        Mission_RUN = iValue;
                        UART_ENABLE = Mission_RUN;

                        if (Mission_RUN)
                        {
                            //
                            //  Start of Mission RUN
                            //  We need to read the temperature sensors.
                            tempSensorList = Read_ALL_TempSensor();
                            TemperatureReal = *(tempSensorList + 1) - 7;
                            Mission_TRead_ID = set_TemperatureLUTpointer((double)TemperatureReal);
                            sprintf(Payload_Out, "%i,MOD_ID:%03i: T1=%0.1f,T2=%0.1f,T3=%0.1f,  Tindex=%i", 1008, Module_HWID, *(tempSensorList), *(tempSensorList + 1), *(tempSensorList + 2), Mission_TRead_ID);
                            send_status(1008, Payload_Out);

                            enableRS485(TRUE);
                        }
                        else
                        {
                            enableRS485(FALSE);
                        }

                        sprintf(Payload_Out, "%s_%03i:MRUN:%i", WHOAMI, Module_HWID, Mission_RUN);
                        send_status(1006, Payload_Out);
                    }

                    if (Mission_RUN > 0)
                        break;

                    /********************************************************************************************/
                    /*        TYPE_HEARTBEAT
                    /********************************************************************************************/
                    else if (PacketType == TYPE_HEARTBEAT)
                    {
                        //  flash_activity_leds(LED_MARQUEE_SHORT, 0);
                        flash_activity_leds(LED_CRASH_OUT, 1);
                    }

                    /********************************************************************************************/
                    /*        TYPE_MISSION_ID_STATE                                                                 */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_MISSION_ID_STATE)
                    {
                        token = strtok_r(Payload_In, ";", &paramPtr);
                        paramvalue = strtok(token, ",");
                        iValue = atoi(paramvalue);
                        Mission_ID = iValue;

                        sprintf(Payload_Out, "MID\n");
                        SendMessage((int)TYPE_ACK, Module_HWID, ClientAssigned_ID, Payload_Out);
                        sprintf(Payload_Out, "%s_%03i:MID:%i", WHOAMI, Module_HWID, Mission_ID);
                        send_status(1007, Payload_Out);
                    }

                    /********************************************************************************************/
                    /*        TYPE_ECHO                                                                 */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_ECHO)
                    {
                        token = strtok_r(Payload_In, ";", &paramPtr);
                        paramvalue = strtok(token, ",");
                        iValue = atoi(paramvalue);
                        ECHO_UART = iValue;
                        sprintf(Payload_Out, "ECHO\n");
                        SendMessage((int)TYPE_ACK, Module_HWID, ClientAssigned_ID, Payload_Out);
                    }

                    /********************************************************************************************/
                    /*                E N A B L E   U A R T   4 8 5                                             */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_485_ENABLE)
                    {
                        token = strtok_r(Payload_In, ";", &paramPtr);
                        paramvalue = strtok(token, ",");
                        iValue = atoi(paramvalue);
                        UART_ENABLE = iValue;
                        if (DEBUG)
                            printf("UART_ENABLE:%i\n", iValue);

                        if (UART_ENABLE)
                        {
                            enableRS485(TRUE);
                            sprintf(LogItem, "UART Processing ENABLED");
                        }
                        else
                        {
                            enableRS485(FALSE);
                            sprintf(LogItem, "UART Processing DISABLED");
                        }

                        if (DEBUG)
                            printf("%s\n", LogItem);
                    }

                    /********************************************************************************************/
                    /*                L U T   L O A D                                                           */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_TRIGGER_LUT_LOAD)
                    {
                        download_lut_data();
                    }

                    /********************************************************************************************/
                    /*          S o u r c e   f i l e   U p d a t e                                             */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_UPDATE_SRC_FILE)
                    {
                        strcpy(SourceDIR, strtok(Payload_In, ","));
                        strcpy(DestDIR, strtok(NULL, ","));
                        strcpy(FileName, strtok(NULL, ","));

                        if (DEBUG)
                        {
                            printf("     >:%s\n", SourceDIR);
                            printf("     >:%s\n", DestDIR);
                            printf("     >:%s\n", FileName);
                        }
                        status = downloadFile(SourceDIR, DestDIR, FileName);
                        sprintf(Payload_Out, "%s_%03i : %s/%s", WHOAMI, Module_HWID, DestDIR, FileName);
                        send_status(1004, Payload_Out);
                    }

                    /********************************************************************************************/
                    /*          P r o c e s s   S H E L L   C o m m a n d                                       */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_SHELL_CMD)
                    {
                        if (DEBUG)
                            printf(" shell  >:%s\n", Payload_In);
                        status = system(Payload_In);
                        if (DEBUG)
                            printf("shell r = %i\n\n\n", status);

                        sprintf(Payload_Out, "%s_%03i : %s", WHOAMI, Module_HWID, Payload_In);
                        send_status(1005, Payload_Out);
                    }

                    /********************************************************************************************/
                    /*         S E T   D A T E / T I M E                                                        */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_SETDATETIME)
                    {
                        setDateTime(Payload_In);
                        if (LOG_MOD_CMD)
                        {
                            sprintf(LogItem, "setdate:%s\n", Payload_In);
                            write_log(LogItem, 1);
                        }
                        gettimeofday(&tv1, NULL);
                        sprintf(Payload_Out, "ACK:%s", Payload_In);
                        flash_activity_leds(LED_TRAFFIC_IN_STATIC, 0);
                        //  SendMessage((int)TYPE_ACK, Module_HWID, ClientAssigned_ID, Payload_Out);
                    }

                    /********************************************************************************************/
                    /*                P r o c e s s   M o d u l e   C o m m a n d                               */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_CMD_MODULE)
                    {
                        if (DEBUG_DAC)
                            printf("%s  %i\n", Payload_In, strlen(Payload_In));

                        if (LOG_MOD_CMD)
                        {
                            sprintf(LogItem, "%s, L:%i\n", Payload_In, strlen(Payload_In));
                            ///   write_mod_cmd_log(LogItem);
                        }

                        setVFSstate(Payload_In);
                        sprintf(Payload_Out, "ACK:%s", Payload_In);
                        // flash_activity_leds(LED_TRAFFIC_IN_STATIC, 0);
                        SendMessage((int)TYPE_ACK, Module_HWID, ClientAssigned_ID, Payload_Out);
                    }

                    /********************************************************************************************/
                    /*                P r o c e s s   T e m p e r a t u r e   Q u e r y                         */
                    /********************************************************************************************/

                    else if (PacketType == TYPE_ReportTemperatures)
                    {

                        /*  Read the three temperature sensors.
                            On first start up, sometimes there are bogus readings on the first two.
                            It seems a follw up read clears it out.  We will allow up to 10 reading before continuing.
                            */

                        tempSensorList = Read_ALL_TempSensor();
                        sprintf(Payload_Out, "%0.1f,%0.1f,%0.1f\n", *(tempSensorList), *(tempSensorList + 1), *(tempSensorList + 2));

                        if (DEBUG)
                            printf("\n\n%0.1f,%0.1f,%0.1f\n\n", *(tempSensorList), *(tempSensorList + 1), *(tempSensorList + 2));

                        flash_activity_leds(LED_TRAFFIC_IN_STATIC, 0);
                        SendMessage((int)TYPE_ReportTemperatures, Module_HWID, ClientAssigned_ID, Payload_Out);
                    }

                    /********************************************************************************************/
                    /*             Read the three temperature sensors and log to local file.                    */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_LogTemperatures)
                    {
                        tempSensorList = Read_ALL_TempSensor();
                        sprintf(LogItem, "%0.1f,%0.1f,%0.1f\n", *(tempSensorList), *(tempSensorList + 1), *(tempSensorList + 2));

                        write_temperaturelog(LogItem, 1);

                        sprintf(Payload_Out, "ACK:%s", Payload_In);
                        flash_activity_leds(LED_TRAFFIC_IN_STATIC, 0);
                        SendMessage((int)TYPE_ACK, Module_HWID, ClientAssigned_ID, Payload_Out);
                    }

                    /********************************************************************************************/
                    /*        TYPE_Ping                                                                         */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_Ping)
                    {
                        if (Verbose)
                            printf("Server:RFP\n");
                        sprintf(Payload_Out, "HB\n");
                        SendMessage((int)TYPE_Ping, Module_HWID, ClientAssigned_ID, Payload_Out);
                    }

                    /********************************************************************************************/
                    /*        TYPE_PingResponse                                                                 */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_PingResponse)
                    {
                        if (Verbose)
                            printf("Pinging server for HB.\n");
                        sprintf(Payload_Out, "beat\n");
                    }

                    /********************************************************************************************/
                    /*      Termination order received from HOST                                                */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_Close)
                    {
                        sprintf(LogItem, "PKILL order received from HOST");

                        if (DEBUG)
                            printf("%s\n", LogItem);

                        send_status(1009, LogItem);
                        usleep(2000000);
                        close(socket_1_handle);
                        flash_activity_leds(LED_ALLRND, 0);
                        return (0);
                    }

                    /********************************************************************************************/
                    /*      R e s p o n d   t o   C r e d e n t i a l s   R e q u e s t                         */
                    /********************************************************************************************/
                    else if (PacketType == TYPE_RequestCredentials)
                    {
                        //
                        // send credentials
                        //
                        flash_activity_leds(LED_ALLRND, 0);
                        sprintf(LogItem, "Credential Request from Server");
                        write_log(LogItem, DEBUG);

                        setDateTime(Payload_In);
                        if (LOG_MOD_CMD)
                        {
                            sprintf(LogItem, "setdate:%s\n", Payload_In);
                            write_log(LogItem, 1);
                        }
                        gettimeofday(&tv1, NULL);
                        sprintf(Payload_Out, "ACK:%s", Payload_In);
                        flash_activity_leds(LED_TRAFFIC_IN_STATIC, 0);

                        sprintf(LogItem, "Sending Credentials");
                        write_log(LogItem, DEBUG);

                        sprintf(Payload_Out, "%s_%d%s:%s:%d", WHOAMI, Module_HWID, VERSION, IPaddr, Module_HWID);
                        flash_activity_leds(LED_ALLRND, 0);
                        SendMessage((int)TYPE_MyCredentials, Module_HWID, ClientAssigned_ID, Payload_Out);
                        // send_status(1001, Payload_Out);
                        flash_activity_leds(LED_ALLRND, 0);
                        sprintf(LogItem, "Connection succesful!");
                        write_log(LogItem, DEBUG);

                        if (1)
                        {
                                 usleep(20000000);
                            if (load_LUT_Data() != OK)
                            {
                                sprintf(LogItem, "MOD_ID%03i:LUT Data RAM load: Failed!", Module_HWID);
                                send_status(1010, LogItem);
                            }
                            else
                            {
                                sprintf(LogItem, "MOD_ID%03i:LUT Data RAM load: Success!", Module_HWID);
                                send_status(1011, LogItem);
                            }

                            write_log(LogItem, DEBUG);
                        }
                    }

                    else if (PacketType == TYPE_IDN) // Id request)
                    {
                        //
                        // send credentials
                        //
                        flash_activity_leds(LED_ALLRND, 0);
                        sprintf(Payload_Out, "%s_%d%s:%s:HWID_%d", WHOAMI, Module_HWID, VERSION, IPaddr, Module_HWID);
                        send_status(1015, Payload_Out);
                        flash_activity_leds(LED_ALLRND, 0);
                    }

                    stack_token = strtok_r(NULL, ";", &stackPtr);
                } //  end of stack processing
            }

            /********************************************************************************************/
            /*      Lost Host Connection                                                                */
            /********************************************************************************************/
            else if (tcp_Data == -2)
            {
                sprintf(LogItem, "Lost Host Connection");
                write_log(LogItem, DEBUG);
                break;
            }

            /*
            else
            {

                flash_activity_leds(LED_TRAFFIC_IN_STATIC, 0);
                gettimeofday(&tv2, NULL);
                elapsed_time = (double)((tv2.tv_usec - tv1.tv_usec) / 1000000 + (double)(tv2.tv_sec - tv1.tv_sec));
                // printf("%f\n", elapsed_time);
                if (elapsed_time > 10)
                {
                    sprintf(Payload_Out, "HB\n");
                    SendMessage( (int)TYPE_Ping, Module_HWID, ClientAssigned_ID, Payload_Out);
                    // printf("Ping out\n");
                    gettimeofday(&tv1, NULL);
                }

                // if (elapsed_time > 1)
                // {
                //
                // }
            }
            */
        }
        sprintf(LogItem, "Application terminating");
        write_log(LogItem, DEBUG);
        close(socket_1_handle);
    } while (1);

    return (0);
}

void SendMessage(int PacketType, int Module_HWID, int ClientAssigned_ID, char PayLoad[])
{
    char buffer_out[1024];
    char hMsgType[3];
    char hClientAssigned_ID[5];
    char hModule_HWID[5];
    char hPayloadSize[8];

    sprintf(hMsgType, "%02x", PacketType);
    sprintf(hModule_HWID, "%04x", (int)Module_HWID);
    sprintf(hClientAssigned_ID, "%04x", (int)ClientAssigned_ID);
    sprintf(hPayloadSize, "%04x", strlen(PayLoad));

    sprintf(buffer_out, "%s%s%s%s%s", hMsgType, hModule_HWID, hClientAssigned_ID, hPayloadSize, PayLoad);

    flash_activity_leds(LED_TRAFFIC_IN_STATIC, 0);
    flash_activity_leds(LED_TRAFFIC_OUT_STATIC, 1);
    sendData(socket_1_handle, buffer_out);
    flash_activity_leds(LED_TRAFFIC_OUT_STATIC, 0);
}
