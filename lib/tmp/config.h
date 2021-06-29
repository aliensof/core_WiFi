#ifndef _CONFIG_H
#define _CONFIG_H

#define TINY_GSM_MODEM_SIM7020

/* Define the serial console for debug prints, if needed */
// #define TINY_GSM_DEBUG SerialMon
/* uncomment to dump all AT commands */
// #define DEBUG_DUMP_AT_COMMAND

#define UPLOAD_INTERVAL         60000

#define SerialMon               Serial
#define MONITOR_BAUDRATE        115200


/* AM7020 module setup: Serial port, baudrate, and reset pin */
#if defined M5CORE
    /* ESP32 Boards */
    #define SerialAT            Serial2
    #define AM7020_BAUDRATE     115200
    #define AM7020_RESET        5

#else
    /* add your own boards and uncomment it */
    /*
    #define SerialAT            Serial1
    #define AM7020_BAUDRATE     9600
    #define AM7020_RESET        7
    */

#endif

/* set GSM PIN */
#define GSM_PIN             ""

// for taiwan mobile
#define APN                 "SPHONE"
// #define BAND                28

// for CHT
//#define APN               "internet.iot"
//#define BAND              8

// MQTT Setting
#define MQTT_BROKER         "mqtt.thingspeak.com"
#define MQTT_PORT           1883
#define MQTT_USERNAME       ""
#define MQTT_PASSWORD       ""
#define MQTT_TOPIC          ""

#endif /* _CONFIG_H */