/**********************************************
          Termostato Arduino ACCE
***********************************************/
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 7 // Pin usado para conectar con los dispositivos 1-wire, en este caso el sensor de temperatura Ds18b20

OneWire oneWire(ONE_WIRE_BUS);       // Creamos objeto para comunicarnos con dispositivos 1-wire.
DallasTemperature sensors(&oneWire); // Creamos objeto para comunicarnos con sensores de temperatura.

void setup() {
  
  Serial.begin(9600); 
  while(!Serial); // Esperar a que la comunicacion serie este activa

}

void loop() {
  
  // Pedimos temperatura a todos los sensores conectados, en nuestro caso solo uno
  sensors.requestTemperatures();
  // Leemos la temperatura del primer sensor conectado en celcius.
  // La resolucion por defecto es de 12 bits, incremento de 0.0625°C
  float temperature = sensors.getTempCByIndex(0);
  
  // Enviamos al puerto serie la Lectura de temperatura
  Serial.println(temperature);
  
  // Hacemos una pausa de un segundo
  delay(1000);
}
