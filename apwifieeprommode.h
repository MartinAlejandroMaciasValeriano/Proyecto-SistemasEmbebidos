#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <Wire.h>
#include "WiFi.h"

WebServer server(80);

String leerStringDeEEPROM(int direccion)
{
    String cadena = "";
    char caracter = EEPROM.read(direccion);
    int i = 0;
    while (caracter != '\0' && i < 100)
    {
        cadena += caracter;
        i++;
        caracter = EEPROM.read(direccion + i);
    }
    return cadena;
}

void escribirStringEnEEPROM(int direccion, String cadena)
{
    int longitudCadena = cadena.length();
    for (int i = 0; i < longitudCadena; i++)
    {
        EEPROM.write(direccion + i, cadena[i]);
    }
    EEPROM.write(direccion + longitudCadena, '\0'); // Null-terminated string
    EEPROM.commit();                                // Guardamos los cambios en la memoria EEPROM
}

void handleRoot()
{
    String html = "<html><body>";
    html += "<form method='POST' action='/wifi'>";
    html += "Red Wi-Fi: <input type='text' name='ssid'><br>";
    html += "Contraseña: <input type='password' name='password'><br>";
    html += "<input type='submit' value='Conectar'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
}

int posW = 50;
void handleWifi()
{
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    Serial.print("Conectando a la red Wi-Fi ");
    Serial.println(ssid);
    Serial.print("Clave Wi-Fi ");
    Serial.println(password);
    Serial.print("...");
    WiFi.disconnect(); // Desconectar la red Wi-Fi anterior, si se estaba conectado
    WiFi.begin(ssid.c_str(), password.c_str(), 6);

    int cnt = 0;
    while (WiFi.status() != WL_CONNECTED and cnt < 8)
    {
        delay(1000);
        Serial.print(".");
        cnt++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        // guardar en memoria eeprom la ultima red conectada

        Serial.print("Guardando en memoria eeprom...");
/*        if (posW == 0)
            posW = 50;
        else
            posW = 0;*/
        String varsave = leerStringDeEEPROM(300);
        if (varsave == "a") {
            posW = 0;
            escribirStringEnEEPROM(300, "b");
        }
        else{
            posW=50;
            escribirStringEnEEPROM(300, "a");
        }
        escribirStringEnEEPROM(0 + posW, ssid);
        escribirStringEnEEPROM(100 + posW, password);
        // guardar en memoria eeprom la ultima red conectada

        Serial.println("Conexión establecida");
        server.send(200, "text/plain", "Conexión establecida");
    }
    else
    {
        Serial.println("Conexión no establecida");
        server.send(200, "text/plain", "Conexión no establecida");
    }
}

bool lastRed()
{ // verifica si una de las 2 redes guardadas en la memoria eeprom se encuentra disponible
    // para conectarse en ese momento
    for (int psW = 0; psW <= 50; psW += 50)
    {
        String usu = leerStringDeEEPROM(0 + psW);
        String cla = leerStringDeEEPROM(100 + psW);
        Serial.println(usu);
        Serial.println(cla);
        WiFi.disconnect();
        WiFi.begin(usu.c_str(), cla.c_str(), 6);
        int cnt = 0;
        while (WiFi.status() != WL_CONNECTED and cnt < 5)
        {
            delay(1000);
            Serial.print(".");
            cnt++;
        }
        if (WiFi.status() == WL_CONNECTED){
            Serial.println("Conectado a Red Wifi");
            Serial.println(WiFi.localIP());
            break;
        }
    }
    if (WiFi.status() == WL_CONNECTED)
        return true;
    else
        return false;
}

void initAP(const char *apSsid, const char *apPassword)
{ // Nombre de la red Wi-Fi y  Contraseña creada por el ESP32
    Serial.begin(115200);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, apPassword);

    server.on("/", handleRoot);
    server.on("/wifi", handleWifi);

    server.begin();
    Serial.println("Servidor web iniciado");
}

void loopAP()
{

    server.handleClient();
}

void intentoconexion(const char *apname, const char *appassword)
{

    Serial.begin(115200);
    EEPROM.begin(512);
    Serial.println("ingreso a intentoconexion");
    if (!lastRed())
    {                               // redirige a la funcion
        
        Serial.println("Conectarse desde su celular a la red creada");
        Serial.println("en el navegador colocar la ip:");
        Serial.println("192.168.4.1");
        initAP(apname, appassword); // nombre de wifi a generarse y contrasena
    }
    while (WiFi.status() != WL_CONNECTED) // mientras no se encuentre conectado a un red
    {
        loopAP(); // genera una red wifi para que se configure desde la app movil
    }
}