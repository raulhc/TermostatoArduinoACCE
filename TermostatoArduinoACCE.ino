/**********************************************
          Termostato Arduino ACCE
          
          Por raulhc @ 2015
***********************************************/
#include <Process.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 7 // Pin usado para conectar con los dispositivos 1-wire, en este caso el sensor de temperatura Ds18b20

OneWire oneWire(ONE_WIRE_BUS);       // Creamos objeto para comunicarnos con dispositivos 1-wire.
DallasTemperature sensors(&oneWire); // Creamos objeto para comunicarnos con sensores de temperatura.

void setup() {
  
  Bridge.begin();
  
  Serial.begin(9600); 
  while(!Serial); // Esperar a que la comunicacion serie este activa

}

void loop() {
  
  // Leemos la temperatura
  float temperature = getTemperature();
  
  // Leemos la fecha y hora del servidor
  String serverTime = readServerTime();

  // Enviamos al puerto serie la Lectura de fecha y hora
  Serial.print(serverTime);
  Serial.print(" -> ");
  
  // Enviamos al puerto serie la Lectura de temperatura
  Serial.print(temperature);
  Serial.println("°C");  
  
  // Hacemos una pausa de un segundo
  delay(1000);
}

/**
 * Leer temperatura de Sensor DS18B20. Se devolvera la lectura redondeada a 1 decimal
 */
float getTemperature() {
  
    // Pedimos lectura de temperatura a todos los sensores conectados
  sensors.requestTemperatures();
  
  // Leemos la temperatura del primer sensor conectado en celcius.
  // La resolucion por defecto es de 12 bits, incremento de 0.0625°C
  float temperature = sensors.getTempCByIndex(0);
  
  // Devolver la temperatura con redondeo a un decimal
  return round(temperature * 10) / 10.0;
}

/**
 * Devuelve la fecha y hora del servidor Linux
 */
String readServerTime() {
  String strTime;
  
  Process time;
  time.runShellCommand("date +\"%d/%m/%Y %H:%M.%S\"");
  while (time.available()) {
    char c = time.read();
    if (c != '\n') strTime += c;
  }
  return strTime;
}
