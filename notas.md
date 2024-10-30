#IOT 
Una infraestructura donde no solo los humanos son usuarios de internet, sino también los objetos, los cuales tendrán un propósito específico en su conexión 

Se utilizan para crear notificaciones autónomas o mejor uso de recursos como agua, electricidad, etc.

La idea de IoT es que se tenga la menor interacción humana, que su configuración sea en su mayoría, automática. Y una vez en la red
se puedan sincronizar entre ellos para lograr su objetivo.

Las aplicaciones son por ejemplo:
- Smart cities
- Smart homes
- Health care
- Transportation
- Wearables
- Energy engagement
- Smart manufacturing
- Agriculture

## Arquitectura

- Aplicación 
- Red 
- Percepción 

### WiFi
- El ESP32 puede ser un AP (Access point) o una STA (Station)
- Basic Service Set (BSS)
- Dirección MAC 
- Service Set Identifier (SSID)

El esp32 siempre requiere la NVS (Non-Volatile Storage) para funcionar, por lo que se debe iniciar siempre al utilizar
el wifi.

El uso del wifi requiere un manejador de eventos

## Archivos 
```c
//El SPIFFS es una forma de utilizar archivos organizados en la memoria flash
//swapfile
// Se utiliza para particionar la FLASH 
void init spiffs(void){
    
}
```

- Inicilizar NVS
- Iniciar el SPIFFS 
  - Parition partition.csv 
- Iniciar modo AP 
- Iniciar los manejadores 




