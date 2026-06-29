# Datacasting ISDB-Tb

Esta guía resume la arquitectura experimental de datacasting usada como
referencia para este repositorio. El objetivo es transmitir datos de propósito
general sobre ISDB-Tb en un canal unidireccional, sin modificar la capa física
del estándar.

## Motivación

ISDB-Tb entrega cobertura de televisión digital terrestre ya desplegada. Esa
infraestructura puede actuar como canal de difusión masiva para archivos,
alertas, configuración o actualizaciones, incluso hacia receptores sin internet.
El costo de emisión no crece con la cantidad de receptores porque el enlace es
`un transmisor -> N receptores`.

## Objetivo técnico

El sistema busca desarrollar y evaluar una capa de transporte sobre MPEG-TS con
estas propiedades:

- unidad de encapsulación compatible con paquetes TS de 188 bytes;
- redundancia temporal mediante carrusel;
- verificación de integridad por paquete;
- recuperación de pérdidas sin canal de retorno;
- medición de PER, goodput, MER, SNR, drops y estabilidad.

El alcance es una validación técnica en laboratorio sobre GNU Radio y USRP.

## Arquitectura

La propuesta añade una capa confiable sobre la cadena ISDB-Tb existente:

```text
Datos/IP
  -> encapsulación + secuencia + hash + CRC
  -> secciones privadas/MPE
  -> MPEG-TS por PID
  -> gr-isdbt / ISDB-Tb
  -> RF
```

La cadena física conserva los mecanismos estándar:

```text
Reed-Solomon (204,188)
  -> byte interleaver
  -> codificación convolucional
  -> bit interleaver
  -> BST-OFDM modo 3
```

Los módulos Python de paquetización ISDB-Tb fueron desarrollados usando como
referencia [`opencaster_isdb-tb`](https://github.com/0xalen/opencaster_isdb-tb).
Ese repositorio sirvió como base práctica para entender la generación de
secciones MPEG-2/MPE y su conversión a paquetes MPEG-TS aptos para ser
multiplexados dentro del flujo ISDB-Tb.

## Encapsulación

Cada unidad de datos incorpora una cabecera mínima de integridad antes de
segmentarse en MPEG-TS:

| Campo | Tamaño | Propósito |
| --- | ---: | --- |
| `Seq` | 4 B | Secuencia para ordenar y detectar saltos |
| `XXH64` | 8 B | Hash rápido del payload |
| `Payload` | variable | Datos IP o datos de aplicación |
| `CRC-32` | 4 B | Integridad de sección/paquete |

La reducción de overhead y el cambio desde SHA-256 hacia XXH64/fastcrc reducen
la presión de CPU en la ruta crítica.

## Transmisor

El transmisor experimental realiza:

1. captura de tráfico;
2. descarte de tráfico no UDP cuando aplica;
3. cálculo de huella de integridad;
4. empaquetado `Seq + Hash + Payload`;
5. segmentación a TS de 188 B;
6. inserción de repeticiones del carrusel;
7. salida hacia el transmisor SDR.

La etapa de paquetización en Python encapsula los datos en una estructura
compatible con MPEG-TS siguiendo la lógica de referencia de `opencaster_isdb-tb`,
pero agregando secuenciamiento, verificación y soporte de carrusel para el caso
simplex evaluado en este repositorio.

El primer paquete de un flujo dispara una ráfaga `START` repetida. Los datos
nuevos y las repeticiones convergen en un continuity counter maestro para evitar
colisiones entre tráfico original y carrusel.

## Receptor

El receptor experimental realiza:

1. recepción de TS desde GNU Radio;
2. validación de CRC-32;
3. procesamiento de control `START`, `EOT` y `Heartbeat`;
4. verificación de hash;
5. rescate de copias desde carrusel si falla o falta un paquete;
6. reordenamiento por secuencia;
7. entrega del payload reconstruido;
8. registro de métricas.

El carrusel no requiere canal de retorno: el receptor espera copias futuras de
paquetes perdidos o corruptos dentro de una ventana de rescate.

## Métricas

| Métrica | Interpretación |
| --- | --- |
| `MER` | Calidad de constelación. Eje X principal para curvas de enlace. |
| `SNR` | Condición física estimada. |
| `PER_obs` | Errores explícitos observados por CRC/hash/decodificación. |
| `PER_est` | PER corregido con pérdidas silenciosas inferidas. |
| `Goodput` | Payload útil entregado por unidad de tiempo. |
| `Drops` | Saturación local de buffers o colas de procesamiento. |

El `PER_obs` puede subestimar la pérdida real porque algunos paquetes
desaparecen sin generar evento explícito. Para corregirlo se estima una pérdida
implícita cuando aparece una secuencia posterior con salto:

```text
Eimp = max(0, Smax - Sesp - M)
PERobs = Eexp / (Pok + Eexp)
PERest = (Eexp + Eimp) / (Pok + Eexp + Eimp)
```

`Smax` es la mayor secuencia vista, `Sesp` la próxima esperada y `M` un margen
para evitar contar paquetes aún recuperables por carrusel.

## Pipeline de análisis

El flujo de análisis recomendado es:

```text
GNU Radio
  -> loggermaster.py
  -> mastermetrics.py
  -> metrics_cleaner.py
  -> metrics_reporter.py
  -> plot_per_mer.py
```

- GNU Radio produce métricas base en tiempo real.
- `loggermaster.py` serializa estado a CSV.
- `mastermetrics.py` consolida y etiqueta registros.
- `metrics_cleaner.py` filtra ruido y valores faltantes.
- `metrics_reporter.py` resume cobertura por configuración.
- `plot_per_mer.py` grafica PER contra MER.

## Parámetros experimentales de referencia

| Parámetro | Capa A datos | Capa B monitoreo |
| --- | --- | --- |
| Modulación | QPSK o 16-QAM | 64-QAM |
| Segmentos | 1, one-seg | 12 |
| Code rate | 2/3, 3/4, 5/6, 7/8 | igual capa A |
| Modo ISDB-T | 3, 8K | 3, 8K |
| Frecuencia central | 569.143 MHz | N/A |
| Distancia TX-RX | 2.1 m fija / 2-5 m variable | N/A |
| AGC receptor | desactivado | N/A |
| Entrada RX | RX2 | N/A |
| Carrusel | 2 repeticiones | N/A |

## Resultados operacionales citados

En el banco de laboratorio descrito, el enlace mostró un efecto acantilado al
bajar MER. Los umbrales aproximados fueron:

| Modulación | Code rate | Umbral MER | Transición |
| --- | ---: | ---: | --- |
| QPSK | 2/3 | ~5.5 dB | 4-7 dB |
| QPSK | 3/4 | ~6.5 dB | 5-8 dB |
| 16-QAM | 2/3 | ~10 dB | 9-12 dB |
| 16-QAM | 3/4 | ~11.5 dB | 10-13 dB |

Estos valores son umbrales operacionales del banco de pruebas. El MER medido en
GNU Radio no debe tratarse como C/N normativo ni como certificación de cobertura
en campo abierto.

## Ejemplos TransIsdtb

El submódulo `examples/TransIsdtb` contiene scripts, flowgraphs y resultados
para experimentar con transporte de datos y análisis de métricas:

- `full_transceiver.grc` y variantes: flowgraphs GNU Radio.
- `cypher.py` / `descypher.py`: codificación y reconstrucción experimental de
  datos en frames/video.
- `mastermetrics.py`, `metrics_cleaner.py`, `metrics_reporter.py`: análisis de
  métricas.
- `plot_per_mer.py`, `plot_drops_mer.py`, `plot_snr_per.py`: generación de
  gráficas.
- `tests/`: pruebas auxiliares del flujo experimental.

Para inicializar el submódulo en un clon existente:

```bash
git submodule update --init --recursive
```

## Riesgos y limitaciones

- Sin canal de retorno no hay retransmisión selectiva.
- El carrusel mejora recuperación, pero aumenta overhead y latencia.
- Los drops internos indican presión computacional, no necesariamente pérdida
  en el aire.
- El receptor puede producir resultados engañosos si sus colas se saturan.
- La validación citada fue de laboratorio interior y no cubre movilidad ni NLOS
  severo.

## Trabajo futuro

- FEC adicional sobre la capa de transporte.
- Carrusel adaptativo según PER en tiempo real.
- Aceleración GPU/FPGA para FFT, Viterbi o Reed-Solomon.
- Instrumentación p50/p95/p99 por etapa.
- Campañas de campo con mayor distancia, movilidad y antenas variadas.
- Comparación con DVB-T y ATSC datacasting.
