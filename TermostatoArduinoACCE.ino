/**********************************************
          Termostato Arduino ACCE
          
          Por raulhc @ 2015
***********************************************/
#include <Process.h>
#include <OneWire.h>
#include <FileIO.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS       7 // Pin usado para conectar con los dispositivos 1-wire, en este caso el sensor de temperatura Ds18b20
#define HEATING_DEVICE_PIN 8 // Pin digital de salida que activara el rele que acciona el dispositivo para calentar
#define COOLING_DEVICE_PIN 9 // Pin digital de salida que activara el rele que acciona el dispositivo para enfriar

#define CONFIG_FILE "/mnt/sd/termostatoACCE.config" // Fichero de configuracion

OneWire oneWire(ONE_WIRE_BUS);       // Creamos objeto para comunicarnos con dispositivos 1-wire.
DallasTemperature sensors(&oneWire); // Creamos objeto para comunicarnos con sensores de temperatura.

float  _temperatureSet    = 20.0; // Temperatura que se debe mantener, se inicializa con un valor de 20.
float  _temperatureRead   = 0.0;  // Temperatura leida de la sonda.
int    _actionMode        = 0;    // Indicara el modo de accion: -1 Enfriar, 0 Sin accion, 1 Calentar 
String _temperatureTime   = "";   // Fecha y hora en la que se produjo la ultima lectura de temperatura
float  _temperatureMargin = 0.5;  // Margen admisible en el mantenimiento de la temperatura


/**
  * Configuracion de Arduino
  */
void setup() {
  
  // Inicializar el puente de comunicacion entre Arduino<->Linux
  Bridge.begin();
  
  // Inicializar libreria para manejo de ficheros en la parte Linux
  FileSystem.begin();
  
  // Establecemos los pines de accion calor / frio como de salida
  pinMode(HEATING_DEVICE_PIN, OUTPUT);
  pinMode(COOLING_DEVICE_PIN, OUTPUT);
  
  // Comunicacion serie para enviar datos de depuracion
  Serial.begin(9600); 
  while(!Serial); // Esperar a que la comunicacion serie este activa

  // Leemos los datos de configuracion del fichero "termostatoACCE.config" que tenemos en la SD
  readConfigFile();
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
  
  // Si la temperatura es menor de la temperatura objetivo menos el margen de tolerancia, entonces activar el modo calor
  if (_temperatureRead < (_temperatureSet - _temperatureMargin)) {
    _actionMode = 1;
  }
  
  // Si la temperatura es mayor de la temperatura objetivo mas el margen de tolerancia, entonces activar el modo frio
  if (_temperatureRead > (_temperatureSet + _temperatureMargin)) {
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

/**
  * Lee la configuracion del fichero guardado en la SD. 
  * Si el fichero no existe se crea con los valores de configuracion por defecto.
  */
void readConfigFile() {
  
  // Si el fichero no existe entonces se crea con los valores por defecto de configuracion
  
  if (!FileSystem.exists(CONFIG_FILE)) {
      writeConfigFile();
  } else {
    // Abrimos el fichero para lectura
    File configFile = FileSystem.open(CONFIG_FILE, FILE_READ);
    
    // Si tenemos acceso al fichero leemos la configuracion
    if (configFile) {
      String property1, property2;
      float value1, value2;
      
      // El orden podria variar, asi que leemos los dos posible datos
      readConfigData(configFile, property1, value1);
      readConfigData(configFile, property2, value2);
      if (property1 == "temperatureSet") {
        _temperatureSet    = value1;
        _temperatureMargin = value2;
      } else {
        _temperatureSet    = value2;
        _temperatureMargin = value1;
      } 
      
    configFile.close();
    } 
  } 
  Serial.print("--Configuracion--\n\ntemperatureSet = ");
  Serial.println(_temperatureSet);
  Serial.print("temperatureMargin = ");
  Serial.println(_temperatureMargin);
  Serial.println();
}

/**
  * Escribe los datos de configuracion a un fichero. Se almacenan:
  * - temperatureSet
  * - temperatureMargin
  */
void writeConfigFile() {
  
  // Creamos el fichero para escritura, si existe se sobreescribe
  File configFile = FileSystem.open(CONFIG_FILE, FILE_WRITE);
  
  // Si se ha inicializado correctamente el acceso al fichero
  if (configFile) {
    writeConfigData(configFile, "temperatureSet", _temperatureSet);
    writeConfigData(configFile, "temperatureMargin", _temperatureMargin);
    configFile.close();
  }
}

/**
  * Lee el siguiente dato de configuracion
  */
void readConfigData(File& file, String& property, float& value) {
  String strValue;
  
  file.readStringUntil('[');
  property = file.readStringUntil('=');
  strValue = file.readStringUntil(']');  
  value = strValue.toFloat();
}

/**
  * Escribe un dato de configuracion
  */
void writeConfigData(File& file, String property, float value) {
  file.print("[");
  file.print(property);
  file.print("=");
  file.print(value);
  file.print("]\n");
}
