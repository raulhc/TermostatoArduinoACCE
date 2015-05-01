/**********************************************
          Termostato Arduino ACCE
          
          Por raulhc @ 2015
***********************************************/
#include <Process.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS       7 // Pin usado para conectar con los dispositivos 1-wire, en este caso el sensor de temperatura Ds18b20
#define HEATING_DEVICE_PIN 8 // Pin digital de salida que activara el rele que acciona el dispositivo para calentar
#define COOLING_DEVICE_PIN 9 // Pin digital de salida que activara el rele que acciona el dispositivo para enfriar

OneWire oneWire(ONE_WIRE_BUS);       // Creamos objeto para comunicarnos con dispositivos 1-wire.
DallasTemperature sensors(&oneWire); // Creamos objeto para comunicarnos con sensores de temperatura.

float  _temperatureSet  = 20.0; // Temperatura que se debe mantener, se inicializa con un valor de 20.
float  _temperatureRead = 0.0;  // Temperatura leida de la sonda.
int    _actionMode      = 0;    // Indicara el modo de accion: -1 Enfriar, 0 Sin accion, 1 Calentar 
String _temperatureTime = "";   // Fecha y hora en la que se produjo la ultima lectura de temperatura

/**
  * Configuracion de Arduino
  */
void setup() {
  
  // Inicializar el puente de comunicacion entre Arduino<->Linux
  Bridge.begin();
  
  // Establecemos los pines de accion calor / frio como de salida
  pinMode(HEATING_DEVICE_PIN, OUTPUT);
  pinMode(COOLING_DEVICE_PIN, OUTPUT);
  
  // Comunicacion serie para enviar datos de depuracion
  Serial.begin(9600); 
  while(!Serial); // Esperar a que la comunicacion serie este activa

}

/**
  * Bucle principal de ejecucion
  */
void loop() {
  
  // Leemos la temperatura
  readTemperature();
  
  // Leemos la fecha y hora del servidor
  readServerTime();
  
  // Obtenemos que accion ejecutar
  getActionMode();
  
  // Procesamos la accion a ejecutar
  processAction();
  
  // *** DEPURACION *** 

  // Enviamos al puerto serie la Lectura de fecha y hora
  Serial.print(_temperatureTime);
  Serial.print(" -> ");
  
  // Enviamos al puerto serie la Lectura de temperatura
  Serial.print(_temperatureRead);
  Serial.print("°C -> ");
  
  // Enviamos al puerto serie que accion se esta ejecutando
  switch (_actionMode) {
    case -1:
      Serial.println("Enfriando");
      break;
    case 0:
      Serial.println("Sin Accion");
      break;
    case 1:
      Serial.println("Calentando");
      break;
  }  
  
  // Hacemos una pausa de un segundo
  delay(1000);
}

/**
  * Procesamos la accion necesaria, calentar, enfriar o no hacer nada
  */
void processAction() {
  
  switch (_actionMode) {
    case -1:
      digitalWrite(HEATING_DEVICE_PIN, HIGH);
      digitalWrite(COOLING_DEVICE_PIN, LOW);
      break;
    case 0:
      digitalWrite(HEATING_DEVICE_PIN, LOW);
      digitalWrite(COOLING_DEVICE_PIN, LOW);
      break;
    case 1:
      digitalWrite(HEATING_DEVICE_PIN, LOW);
      digitalWrite(COOLING_DEVICE_PIN, HIGH);
      break;
  }
  
}

/**
  * Obtener el modo de accion necesaria
  */
void getActionMode() {
  
  // Por defecto indicamos que no es necesaria accion alguna
  _actionMode = 0;
  
  // Si la temperatura es menor de la temperatura objetivo debemos calentar
  if (_temperatureRead < _temperatureSet) {
    _actionMode = 1;
  }
  
  // Si la temperatura es mayor de la temperatura objetivo debemos enfriar
  if (_temperatureRead > _temperatureSet) {
    _actionMode = -1;
  }
}


/**
 * Leer temperatura de Sensor DS18B20. Se devolvera la lectura redondeada a 1 decimal
 */
void readTemperature() {
  
    // Pedimos lectura de temperatura a todos los sensores conectados
  sensors.requestTemperatures();
  
  // Leemos la temperatura del primer sensor conectado en celcius.
  // La resolucion por defecto es de 12 bits, incremento de 0.0625°C
  // Redondeamos el resultado a un decimal
  _temperatureRead = round(sensors.getTempCByIndex(0) * 10) / 10.0;
}

/**
 * Devuelve la fecha y hora del servidor Linux
 */
void readServerTime() {  
  Process time;
  _temperatureTime = "";
  time.runShellCommand("date +\"%d/%m/%Y %H:%M.%S\"");
  while (time.available()) {
    char c = time.read();
    if (c != '\n') _temperatureTime += c;
  }
}
