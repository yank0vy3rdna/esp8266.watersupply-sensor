#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "credentials.h"

#define BOTTOM_SENSOR 13
#define TOP_SENSOR 12

WiFiUDP Udp;
WiFiServer web(80);

IPAddress remote(192, 168, 0, 190);

void setup() {
    Serial.begin(115200);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.config(IPAddress(192, 168, 0, 191), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));
    WiFi.begin(credentials.WIFI_SSID, credentials.WIFI_PASS);
    Udp.begin(8888);
    web.begin();
}

unsigned long last_change_top = millis();
unsigned long last_change_bottom = millis();

bool current_top = false;
bool current_bottom = false;
bool before_top = false;
bool before_bottom = false;

// 0 - бочка пуста
// 1 - бочка наливается
// 2 - бочка полна
// 3 - бочка опустошается

int system_state = 0;

bool get_relay_status() {
    return system_state < 2;
}

void update_current_data() {
    bool top = digitalRead(TOP_SENSOR);
    bool bottom = digitalRead(BOTTOM_SENSOR);
    if (top != before_top) {
        last_change_top = millis();
        before_top = top;
    }
    if (bottom != before_bottom) {
        last_change_bottom = millis();
        before_bottom = bottom;
    }
    if (millis() - last_change_bottom >= 5000)
        current_bottom = bottom;
    if (millis() - last_change_top >= 5000)
        current_top = top;
}

int relay_before = -1;

void send_udp_packet() {
    if (relay_before != (int) get_relay_status()) {
        Udp.beginPacket(remote, 8888);
        Udp.write(get_relay_status());
        Udp.endPacket();
        relay_before = get_relay_status();
    }
}

void calc_system_state() {
    switch (system_state) {
        case 0:
            if (current_bottom)
                system_state++;
            break;
        case 1:
            if (current_top)
                system_state++;
            break;
        case 2:
            if (!current_top)
                system_state++;
            break;
        case 3:
            if (!current_bottom)
                system_state = 0;
            break;
        default:
            system_state = 0;
            break;
    }
}

void receive_data() {
    if (Udp.parsePacket()) {
        unsigned char incomingPacket;
        Udp.read(&incomingPacket, 1);
        relay_before = incomingPacket;
    }
}

void answer_web(){
    WiFiClient client = web.available();
    if (client){
        int counter = 0;
        String currentLine = "";
        while (client.connected()) {            // loop while the client's connected
            if (client.available()) {             // if there's bytes to read from the client,
                int c = client.read();             // read a byte, then

                if (c == '\n') {                    // if the byte is a newline character
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0) {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type: text/html");
                        client.println("Connection: close");
                        client.println();

                        // Display the HTML web page
                        client.println("<!DOCTYPE html><html>");
                        client.println(R"(<head><link href="//fonts.googleapis.com/css?family=Raleway:400,300,600" rel="stylesheet" type="text/css"><link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/skeleton/2.0.4/skeleton.css" integrity="sha512-5fsy+3xG8N/1PV5MIJz9ZsWpkltijBI48gBzQ/Z2eVATePGHOkMIn+xTDHIfTZFVb9GMpflF2wOWItqxAP2oLQ==" crossorigin="anonymous" referrerpolicy="no-referrer" />)");
                        client.println(R"(<meta http-equiv="refresh" content="3"><meta name="viewport" content="width=device-width, initial-scale=1"></head>)");
                        client.println();

                        // Web Page Heading
                        client.println("<body><div class=\"container\"><h1>Water supply system</h1>");
                        client.print("Top sensor: ");
                        if (current_top){
                            client.print("+");
                        } else
                            client.print("-");
                        client.println("<br>");
                        client.print("Bottom sensor: ");
                        if (current_bottom){
                            client.print("+");
                        } else
                            client.print("-");
                        client.println("<br>");

                        client.print("Current relay: ");
                        if (get_relay_status()){
                            client.print("+");
                        } else
                            client.print("-");
                        client.println("</div></body></html>");

                        // The HTTP response ends with another blank line
                        client.println();
                        // Break out of the while loop
                        break;
                    } else { // if you got a newline, then clear currentLine
                        currentLine = "";
                    }
                } else if (c != '\r') {  // if you got anything else but a carriage return character,
                    currentLine += c;      // add it to the end of the currentLine
                }
            }
            counter++;
            if (counter>500){
                break;
            }
        }
        // Close the connection
        client.stop();
    }
}

void loop() {
    update_current_data();
    calc_system_state();
    send_udp_packet();
    receive_data();
    answer_web();
}