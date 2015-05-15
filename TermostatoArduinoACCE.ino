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

#define DATA_TEMPERATURE_SET    "temperatureSet"
#define DATA_TEMPERATURE_MARGIN "temperatureMargin"
#define DATA_TEMPERATURE_READ   "temperatureRead"
#define DATA_TEMPERATURE_TIME   "temperatureTime"
#define DATA_PROBE_OFFSET       "probeOffset"
#define DATA_ACTION_MODE        "actionMode"
#define DATA_CONFIG_CHANGE      "configChanged"
#define DATA_WORKING_MODE       "workingMode"

OneWire oneWire(ONE_WIRE_BUS);       // Creamos objeto para comunicarnos con dispositivos 1-wire.
DallasTemperature sensors(&oneWire); // Creamos objeto para comunicarnos con sensores de temperatura.

float  _temperatureSet    = 20.0; // Temperatura que se debe mantener, se inicializa con un valor de 20.
float  _temperatureRead   = 0.0;  // Temperatura leida de la sonda.
int    _actionMode        = 0;    // Indicara el modo de accion: -1 Enfriar, 0 Sin accion, 1 Calentar 
String _temperatureTime   = "";   // Fecha y hora en la que se produjo la ultima lectura de temperatura
float  _temperatureMargin = 0.5;  // Margen admisible en el mantenimiento de la temperatura
float  _probeOffset       = 0.0;  // Desplazamiento de sonda. 
int    _workingMode       = 0;    // Indica el modo de trabajo: -1 Solo enfriar, 0 Frio / Calor, 1 Solo Calentar

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
  digitalWrite(HEATING_DEVICE_PIN, LOW);
  digitalWrite(COOLING_DEVICE_PIN, LOW);
  
  // Comunicacion serie para enviar datos de depuracion
  Serial.begin(9600); 
  //while(!Serial); // Esperar a que la comunicacion serie este activa

  // Leemos los datos de configuracion del fichero "termostatoACCE.config" que tenemos en la SD
  readConfigFile();
  
  // Escribimos la configuracion actual en Linux
  writeConfigToLinux();
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
  
  // Escribir los datos a la parte Linux
  writeDataToLinux();
  
  // Leemos la configuracion de Linux
  if (readConfigFromLinux()) {
    // Si ha cambiado escribimos la configuracion en la microSD
    writeConfigFile();
  }
  
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
  
  // Si trabajamos con Frio y Calor
  if (_workingMode == 0) {
    switch (_actionMode) {
      case -1:
        digitalWrite(HEATING_DEVICE_PIN, LOW);
        digitalWrite(COOLING_DEVICE_PIN, HIGH);
        break;
      case 0:
        digitalWrite(HEATING_DEVICE_PIN, LOW);
        digitalWrite(COOLING_DEVICE_PIN, LOW);
        break;
      case 1:
        digitalWrite(HEATING_DEVICE_PIN, HIGH);
        digitalWrite(COOLING_DEVICE_PIN, LOW);
        break;
    }
  } else {
    // Si trabjamos con frio o calor la salida del rele de enfriar siempre a inactiva por defecto
    digitalWrite(COOLING_DEVICE_PIN, LOW);

    // Como se usa la salida del rele de calor para ambos modos de funcionamiento esta se activara
    // siempre y cuando el modo de accion sea distinto de 0
    if (_actionMode == 0) {
      digitalWrite(HEATING_DEVICE_PIN, LOW);
    } else {
      digitalWrite(HEATING_DEVICE_PIN, HIGH);
    } 
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
  
  // El modo de accion puede variar dependiendo del modo de trabajo
  // Si estamos en un modo de trabajo que sea solo frio o solo calor solo se activara el modo de accion si 
  // el valor de este coincide con el del modo de trabajo, en otro caso no se realizara accion alguna
  if (_workingMode != 0 && _workingMode != _actionMode) _actionMode = 0;
  
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
  // Asignamos la lectura de temperatura ajustado con el desplazamiento de sonda
  _temperatureRead += _probeOffset;
}

/**
 * Devuelve la fecha y hora del servidor Linux
 */
void readServerTime() {  
  Process time;
  _temperatureTime = "";
  time.runShellCommand("date +\"%d/%m/%Y -- %H:%M.%S\"");
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
      String property;
      float value;
      
      // Leemos los tres datos posibles y los asignamos a la propiedad que corresponga
      readConfigData(configFile, property, value);
      setConfigData(property, value);
      readConfigData(configFile, property, value);
      setConfigData(property, value);
      readConfigData(configFile, property, value);
      setConfigData(property, value);
      readConfigData(configFile, property, value);
      setConfigData(property, value);
      
    configFile.close();
    } 
  } 
  printConfig();
}

void printConfig() {
  Serial.print("\n--Configuracion--\n\ntemperatureSet = ");
  Serial.println(_temperatureSet);
  Serial.print("temperatureMargin = ");
  Serial.println(_temperatureMargin);
  Serial.print("probeOffset = ");
  Serial.println(_probeOffset);
  Serial.print("workingMode = ");
  Serial.println(_workingMode);
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
    writeConfigData(configFile, DATA_TEMPERATURE_SET, String(_temperatureSet));
    writeConfigData(configFile, DATA_TEMPERATURE_MARGIN, String(_temperatureMargin));
    writeConfigData(configFile, DATA_PROBE_OFFSET, String(_probeOffset));
    writeConfigData(configFile, DATA_WORKING_MODE, String(_workingMode));
    configFile.close();
  }
}

/**
  * Asigna el valor leido de configuracion a la propiedad que corresponde. 
  */
void setConfigData(String &property, float& value) {
    if (property == DATA_TEMPERATURE_SET) _temperatureSet = value;
    if (property == DATA_TEMPERATURE_MARGIN) _temperatureMargin = value;
    if (property == DATA_PROBE_OFFSET) _probeOffset = value;
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
void writeConfigData(File& file, String property, String value) {
  file.print("[");
  file.print(property);
  file.print("=");
  file.print(value);
  file.print("]\n");
}

/**
  * Lee los datos de configuracion de la parte linux si es necesario.
  * La funcion devuelve true si los datos se han leido debido a que
  * se ha activado la bandera que indica que han cambiado.
  */
boolean readConfigFromLinux() {
  // Leemos la bandera que indicaria que la configuracion ha cambiado
  String configChanged = readStringDataFromLinux(DATA_CONFIG_CHANGE);
  
  // Si vale "1" entonces leemos la configuracion
  if (configChanged == "1") {
    _temperatureSet    = readFloatDataFromLinux(DATA_TEMPERATURE_SET);
    _temperatureMargin = readFloatDataFromLinux(DATA_TEMPERATURE_MARGIN);
    _probeOffset       = readFloatDataFromLinux(DATA_PROBE_OFFSET);
    _workingMode       = (int)readFloatDataFromLinux(DATA_WORKING_MODE);
    // Cambiamos la bandera a 0 para que indicar que ya esta leida la configuracion
    Bridge.put(DATA_CONFIG_CHANGE, "0");    
  }
  
  // Devolvera "true" si la configuracion se ha leido de Linux y "false" en caso contrario.
  return configChanged == "1";
  
}

/**
  * Escribe la configuracion actual a la parte Linux.
  */
void writeConfigToLinux() {
  Bridge.put(DATA_TEMPERATURE_SET,    String(_temperatureSet));
  Bridge.put(DATA_TEMPERATURE_MARGIN, String(_temperatureMargin));
  Bridge.put(DATA_PROBE_OFFSET,       String(_probeOffset));
  Bridge.put(DATA_WORKING_MODE,       String(_workingMode));
  Bridge.put(DATA_CONFIG_CHANGE, "0");
}

/**
  * Escribe los datos leidos de temperautra, fecha / hora y modo de accion a la parte linux.
  */
void writeDataToLinux() {
  Bridge.put(DATA_TEMPERATURE_READ,   String(_temperatureRead));
  Bridge.put(DATA_TEMPERATURE_TIME,   _temperatureTime);
  Bridge.put(DATA_ACTION_MODE,        String(_actionMode));
}

/**
  * Lee un dato de la parte Linux
  */
String readStringDataFromLinux(char* dataName) {
  char value[5];
  Bridge.get(dataName, value, 5);
  return String(value);
}

/**
  * Lee un dato de la parte Linux y lo devuelve como float
  */
float readFloatDataFromLinux(char* dataName) {
  String value = readStringDataFromLinux(dataName);
  return value.toFloat();
}

