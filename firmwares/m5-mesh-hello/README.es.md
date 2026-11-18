[English](README.md) | [Português](README.pt-BR.md) | **Español** | [Русский](README.ru.md)

> Traducido del inglés; en caso de duda, prevalece la [versión en inglés](README.md).

# M5-Mesh-Hello

Un nodo pequeño y autónomo, compatible con [Meshtastic](https://meshtastic.org), para el **M5Stack Cardputer-Adv** con el **Cap LoRa-1262**. Se une a la red Meshtastic como un nodo más (nombre por defecto **4stro-031** / **4S31**), muestra los nodos cercanos, chatea con ellos mediante mensajes directos cifrados, lee y escribe en el canal público LongFast y puede saludar a cada nodo nuevo **exactamente una vez**. No hace falta un teléfono.

No es el firmware oficial de Meshtastic. Es una implementación mínima escrita desde cero (RadioLib + protocolo + criptografía), con su propia interfaz de teclado, lo bastante pequeña como para instalarse junto a otras aplicaciones con M5Launcher.

La interfaz en pantalla está en portugués. La documentación cita los textos tal como aparecen y explica su significado.

## Características

- Meshtastic **LongFast** en el canal público por defecto (AES-128-CTR, clave por defecto).
- **Mensajes directos cifrados** (PKI: X25519 + SHA-256 + AES-256-CCM) con acuse de recibo (ACK).
- **Lista de nodos** con nombres, modelo de hardware, RSSI, tiempo desde la última recepción y marcadores de no leído y de ya saludado.
- Chat en el **canal público**.
- **Saludo automático**: desactivado en cada arranque. Cuando está activo, envía tu saludo una sola vez por nodo, para siempre (lista persistente), como máximo uno cada 30 s.
- **18 regiones** (11 dentro de la banda nominal del Cap y 7 experimentales), con la potencia de TX limitada al máximo de cada región y un presupuesto horario de tiempo en el aire en las regiones con ciclo de trabajo (duty cycle).
- Nombre largo/corto, texto del saludo y potencia de TX editables. El par de claves se genera una sola vez a partir de la entropía del hardware y se guarda en la NVS (solo cambia si se borra la NVS).
- Nunca retransmite el tráfico de otros nodos (`CLIENT_MUTE`). El Wi-Fi y el Bluetooth permanecen apagados.
- Núcleo de protocolo portable con pruebas unitarias nativas, una prueba de fuzzing, ASan/UBSan y CodeQL en la CI.

## Pantallas

```
+----------------------------------------+   +----------------------------------------+
|ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |   |ANZ 919.875 | AUTO:ON | rx 57 | -101dBm |
| Nós*  | Conversa |  Canal  |  Config   |   |  Nós  | Conversa |  Canal  |  Config   |
|BASE Base Stati TBEAM          -97  2m* |   |!9c2ea3f1 Base Station TBEAM ESP32      |
|JOAO Joao Movel HELTEC_V3     -112 15s +|   |v>Saudaçoes de 4stro-031! ? (Cardputer-A|
|a3f1 !9c2ea3f1  -                -   -  |   |  dv)                                   |
|                                        |   |< Oi! Bem-vindo a rede                  |
|                                        |   |                                        |
|Enter:conversa A:auto-saudaçao Tab:tela |   |> _                                     |
+----------------------------------------+   +----------------------------------------+
        Nós (lista de nodos)                          Conversa (mensajes directos)

+----------------------------------------+   +----------------------------------------+
|ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |   |ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |
|  Nós  | Conversa |  Canal  |  Config   |   |  Nós  | Conversa |  Canal  |  Config   |
|< JOAO: bom dia pessoal                 |   |Regiao     ANZ 919.875                  |
|.>alguem em Sao Paulo?                  |   |Potência   22 dBm                       |
|                                        |   |Nome longo 4stro-031                    |
|                                        |   |Nome curto 4S31                         |
|                                        |   |Saudaçao   Saudaçoes de 4stro-031! ? (Ca|
|                                        |   |Enviar NodeInfo                         |
|                                        |   |ID !a1b2c3d4  chave 1a2b3c4d            |
|> _                                     |   |Enter:editar/acionar  ;/. move          |
+----------------------------------------+   +----------------------------------------+
        Canal (canal público)                         Config (configuración)
```

`Tab` cambia de pantalla, `;`/`.` mueven la selección, `Enter` abre/envía/edita, `` ` `` vuelve atrás y `A` en la pantalla Nós activa o desactiva el saludo automático. La guía completa está en [docs/es/user-guide.md](docs/es/user-guide.md).

## Hardware

- **M5Stack Cardputer-Adv** (el Cardputer original no es compatible).
- **M5Stack Cap LoRa-1262** (SX1262, banda nominal 868–923 MHz, hasta 22 dBm).
- **Una antena** en el conector RP-SMA del Cap. La antena del kit está sintonizada a 868 MHz y funciona en las regiones de 902–928 MHz con una pérdida de aproximadamente 0.5–2 dB. Para esas regiones es mejor una antena de 915 MHz.

> **Enrosca siempre la antena antes de encender el dispositivo.** El firmware transmite su NodeInfo justo después del arranque. Transmitir sin antena puede dañar la radio.

Detalles: [docs/es/hardware.md](docs/es/hardware.md).

## Inicio rápido

1. **Verifica** las imágenes de la versión en [`bin/`](bin/):
   ```sh
   cd bin && sha256sum -c SHA256SUMS
   ```
2. Copia `bin/M5-Mesh-Hello.bin` a la tarjeta microSD e instálalo con **M5Launcher**. Si hay una versión anterior instalada, elimínala primero (**PMan → Remove**).
   (`bin/M5-Mesh-Hello-merged.bin` es una imagen completa de la flash para esptool en la dirección `0x0`. Grabarla **borra M5Launcher y todas las aplicaciones instaladas con él**. Úsala solo si no usas M5Launcher.)
3. Conecta la antena e inicia la aplicación. En el primer arranque crea su par de claves y te pide la **región** (ANZ preseleccionada; `;` / `.` para cambiarla, `Enter` para confirmar). **No se transmite nada hasta que confirmes.** Después se anuncia y empieza a escuchar.
4. Puedes cambiar la región más tarde en la pantalla **Config** (`Região`): todos los nodos deben usar la misma, y debes usar la que es legal en tu país.
5. Espera a que aparezcan nodos en la pantalla **Nós**, selecciona uno, pulsa `Enter` y envía el saludo ya escrito. O pulsa `A` para activar el saludo automático.

## Regiones

La región por defecto es **ANZ** (919.875 MHz), que usa la comunidad Meshtastic de Brasil. La región oficial de Meshtastic para Brasil es BR_902 (903.875 MHz). El firmware también admite US, EU_868, RU, KR, UA_868, MY_919, SG_923, PH_868 y PH_915, además de las experimentales IN, NZ_865, NP_865, KZ_863, JP, TW y TH (un poco fuera de la banda nominal del Cap). **Usa siempre la región del país en el que te encuentras.** La tabla completa, la fórmula de la frecuencia, los límites de potencia y el ciclo de trabajo están en [docs/es/frequencies.md](docs/es/frequencies.md).

## Documentación

| | |
|---|---|
| [Guía de usuario](docs/es/user-guide.md) | Pantallas, teclas, ajustes, saludo automático, solución de problemas, preguntas frecuentes |
| [Hardware](docs/es/hardware.md) | Mapa de pines, antena, alcance, M5Launcher |
| [Regiones y frecuencias](docs/es/frequencies.md) | Regiones, fórmula, potencia, ciclo de trabajo, Brasil |
| [Protocolo](docs/es/protocol.md) | Formato en el aire y criptografía, para implementadores |
| [Arquitectura](docs/es/architecture.md) | Módulos, flujo de datos, almacenamiento, límites |
| [Desarrollo](docs/es/development.md) | Compilación, pruebas, CI, versiones |
| [Seguridad](docs/es/security.md) | Modelo de amenazas y límites |
| [Hoja de ruta](docs/es/roadmap.md) | Ideas y limitaciones conocidas |

Todos los idiomas: [docs/README.md](docs/README.md). Los siguientes archivos solo existen en inglés: cómo contribuir en [CONTRIBUTING.md](CONTRIBUTING.md), informes de seguridad en [SECURITY.md](SECURITY.md), cambios en [CHANGELOG.md](CHANGELOG.md), la licencia en [LICENSE](LICENSE) y los componentes de terceros en [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Estado

**v0.1.0.** Probado en campo con un Cardputer-Adv + Cap LoRa-1262: la inicialización de la radio, la transmisión, la persistencia de la identidad entre reinicios y toda la interfaz (pantallas, navegación, activación del saludo automático) funcionan. **El intercambio de extremo a extremo con otros nodos Meshtastic (recibir sus NodeInfo, DM y ACK en campo) aún no se ha verificado**, porque no había ningún nodo al alcance durante las pruebas. El protocolo se implementó siguiendo el firmware oficial y se probó con vectores de referencia, pero considera la interoperabilidad como no confirmada hasta que alguien informe de que funciona. Los informes son bienvenidos.

## Licencia

[GPL-3.0-or-later](LICENSE) (texto en inglés). Los componentes de terceros y sus licencias se enumeran en [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
