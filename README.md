# Control inteligente de herramientas con RFID

Sistema de préstamo y devolución de herramientas para un taller mecánico. Un ESP32 con lector RC522 identifica a cada trabajador (tarjeta) y a cada herramienta (etiqueta), envía el evento a un servidor local (PHP + MySQL) y el servidor decide si es préstamo o devolución. Un script de Python calcula si la duración de cada préstamo es anómala, y un panel protegido muestra el estado de las herramientas al encargado. El nodo informa cada paso con una señal de LED y buzzer.

Cliente: Servicio Mecánico Salinas, Toluca, Estado de México.

## Avance por checkpoint

| Checkpoint | Contenido                                                   | Archivos principales                         |
| ---------- | ----------------------------------------------------------- | -------------------------------------------- |
| CP1        | Lectura de UID con ESP32 y RC522                            | sketch del nodo (versión inicial)            |
| CP2        | Registro de eventos en MySQL y regla z-score                | evento.php, zscore_prestamos.py              |
| CP4        | Procesamiento local en Miniconda                            | procesar_prestamos.py                        |
| CP5        | Clave compartida, validación y registro de tags             | config.example.php, identificar.php          |
| CP6        | Panel del encargado y web service con autenticación         | encargado/                                   |
| CP7        | Demostración del sistema completo con señales de LED/buzzer | sketch_nov16b.ino (versión final)            |

## Estructura

```
.
├── sketch_nov16b.ino              # Firmware del ESP32 (sin clave real)
├── evento.php                     # Registra préstamo o devolución
├── identificar.php                # Dice si un UID es trabajador, herramienta o desconocido
├── config.example.php             # Plantilla de configuración (copiar a config.php)
├── prueba.html                    # Formulario para probar el servidor sin el ESP32
├── encargado/
│   ├── estado.php                 # Panel del encargado (tabla, CP6)
│   ├── estado.json.php            # Mismo estado en JSON (web service, CP6)
│   └── .htaccess                  # Protección HTTP Basic del panel
├── procesar_prestamos.py          # Procesa préstamos reales y guarda en anomalias (CP4)
├── zscore_prestamos.py            # Regla z-score con datos simulados (CP2)
├── taller_rfid.sql                # Creación de la base de datos
├── .gitignore
└── README.md
```

## Requisitos

- ESP32 DevKit V1, módulo RC522 (13.56 MHz), LEDs verde y rojo con resistencias de 220 Ω, buzzer activo de 5 V.
- Arduino IDE 2.3.11, con el paquete **esp32** (Espressif) y la librería **MFRC522** (miguelbalboa).
- XAMPP 3.3.0 (Apache y MySQL).
- Miniconda con Python 3.11 y `mysql-connector-python` (solo para el análisis).

## Conexiones

### RC522 → ESP32

| RC522    | ESP32            |
| -------- | ---------------- |
| SDA (SS) | GPIO 5           |
| SCK      | GPIO 18          |
| MOSI     | GPIO 23          |
| MISO     | GPIO 19          |
| IRQ      | sin conectar     |
| GND      | GND              |
| RST      | GPIO 22          |
| 3.3V     | 3.3V (nunca 5 V) |

### Señales → ESP32

| Componente            | ESP32                                     |
| --------------------- | ----------------------------------------- |
| LED verde (con 220 Ω) | GPIO 25 (ánodo), cátodo a GND             |
| LED rojo (con 220 Ω)  | GPIO 26 (ánodo), cátodo a GND             |
| Buzzer activo         | GPIO 27 (+), pin − a GND (sin resistencia) |

## Instalación

### 1. Base de datos

1. Enciende Apache y MySQL en XAMPP.
2. En phpMyAdmin, pestaña **Importar**, selecciona `taller_rfid.sql`.
3. Registra trabajadores y herramientas con sus UID (ver *Registro de tags*).

### 2. Servidor

1. Copia los archivos `.php` y `prueba.html` a `C:\xampp\htdocs\rfid\`, y la carpeta `encargado/` completa.
2. Copia `config.example.php` a `config.php` y cambia `CLAVE_NODO` por una clave larga (mínimo 32 caracteres).
3. Verifica que `http://localhost/rfid/prueba.html` responda `"ok":true` con la clave correcta.

### 3. Nodo (ESP32)

1. Abre `sketch_nov16b.ino` en Arduino IDE.
2. Cambia `WIFI_SSID`, `WIFI_PASS`, las URLs (`URL_IDENTIFICAR`, `URL_EVENTO`) con la IP de tu computadora en esa red (`ipconfig`, IPv4) y `CLAVE_NODO` con **la misma clave** que pusiste en `config.php`.
3. Selecciona la placa **ESP32 Dev Module** y el puerto COM, y sube el código.
4. Abre el Monitor Serie a 115200 baudios.

### 4. Panel del encargado (CP6)

1. Crea el archivo de contraseñas fuera de `htdocs`, por ejemplo:

mkdir C:\xampp\passwords
htpasswd.exe -c C:\xampp\passwords\encargado.htpasswd encargado

2. En `encargado/.htaccess`, la ruta de `AuthUserFile` debe apuntar a ese archivo, con barras `/`.
3. En `C:\xampp\apache\conf\httpd.conf`, el bloque `<Directory "C:/xampp/htdocs">` debe tener `AllowOverride All`. Reinicia Apache.
4. Abre `http://localhost/rfid/encargado/estado.php`: debe pedir usuario y contraseña.

### 5. Análisis (opcional)

conda create -n taller python=3.11 -y
conda activate taller
conda install -c conda-forge mysql-connector-python -y
python procesar_prestamos.py


Si la conexión nativa falla en tu equipo, el script ya usa `use_pure=True`.

## Registro de tags

1. Acerca un tag sin registrar al lector: el Monitor muestra `UID no registrado: <UID>`.
2. Anota el UID (8 caracteres para tarjetas MIFARE, 14 para etiquetas NTAG213).
3. Regístralo en phpMyAdmin:

   ```sql
   INSERT INTO trabajadores (uid, nombre) VALUES ('UID', 'Nombre');
   INSERT INTO herramientas (uid, nombre, categoria) VALUES ('UID', 'Nombre', 'uso_rapido');
   ```

   Categorías: `potencia`, `diagnostico`, `uso_rapido`.

## Uso

1. Acerca la tarjeta de un trabajador. El Monitor muestra `Trabajador: <UID>` y suena un pitido corto con LED verde.
2. Acerca la herramienta dentro de los 30 segundos siguientes. El servidor responde `prestamo` o `devolucion`; con `HTTP 200` suena un pitido corto con LED verde.
3. Si acercas la misma herramienta otra vez en menos de 15 s, el servidor responde `HTTP 409` y el nodo muestra LED rojo con pitido largo.
4. Para ver el estado de las herramientas, abre el panel del encargado (`encargado/estado.php`) o su versión JSON (`encargado/estado.json.php`).

## Seguridad y validación

- Cada petición del nodo lleva la clave `clave`. Si no coincide, el servidor responde `HTTP 401`.
- El panel del encargado requiere usuario y contraseña (HTTP Basic). La contraseña del encargado es distinta de la clave del nodo.
- El servidor valida que el trabajador y la herramienta estén registrados (`HTTP 404` si no).
- Las consultas usan PDO con placeholders, y el panel escapa los datos antes de mostrarlos.
- Limitaciones: la clave viaja en texto plano por HTTP y está en el firmware; las credenciales del panel también viajan sin cifrar. Para protegerlas frente a interceptación hace falta HTTPS. El buzzer se alimenta a 3.3 V, así que suena más bajo que su valor nominal.

## Pruebas

| Prueba                             | Resultado esperado                                |
| ---------------------------------- | ------------------------------------------------- |
| Trabajador identificado            | `Trabajador: <UID>`, LED verde y pitido corto     |
| Préstamo con clave correcta        | `HTTP 200`, `tipo: prestamo`, LED verde           |
| Devolución de la misma herramienta | `HTTP 200`, `tipo: devolucion`, LED verde         |
| Misma herramienta en menos de 15 s | `HTTP 409`, LED rojo y pitido largo               |
| Herramienta sin trabajador previo  | `Primero acerca la tarjeta del trabajador`, LED rojo |
| Tag no registrado                  | `UID no registrado`, LED rojo y pitido largo      |
| Clave incorrecta                   | `HTTP 401`                                        |
| Panel sin credenciales             | Pide usuario y contraseña                         |
| Panel con contraseña incorrecta    | Rechaza el acceso                                 |
| `estado.json.php` con credenciales | JSON con el total y el estado de cada herramienta |

## Licencia

Proyecto académico, UAEMex, 2026B. Uso libre con atribución.
