# gr-isdbt-gpu

`gr-isdbt-gpu` es un módulo out-of-tree de GNU Radio para construir y
evaluar cadenas ISDB-T/ISDB-Tb con foco en transmisión SDR, recepción,
sincronización OFDM y medición de desempeño. El repositorio parte del trabajo
`gr-isdbt` y agrega bloques, ejemplos y material experimental orientado a
datacasting sobre TDT.

## Contexto

ISDB-Tb permite usar la infraestructura de televisión digital terrestre como
un canal de difusión `one-to-many`. Aunque se usa principalmente para audio y
video, MPEG-TS también puede transportar datos mediante PID privados y
secciones compatibles con MPE. Esto permite enviar archivos, alertas o
actualizaciones a receptores sin internet y sin canal de retorno.

La memoria usada como referencia evalúa una capa de red unidireccional sobre
ISDB-Tb. El aporte principal es una capa de transporte sobre MPEG-TS con:

- encapsulación de payload IP/datos en unidades compatibles con TS de 188 B;
- secuenciamiento para detectar discontinuidades;
- verificación con CRC-32 y hash XXH64;
- carrusel de repeticiones para recuperación temporal sin retransmisión
  selectiva;
- métricas trazables de PER, goodput, MER, SNR y drops internos.

## Componentes del repositorio

- `lib/`: implementación C++ de bloques GNU Radio para la cadena ISDB-T.
- `include/gnuradio/isdbt/`: interfaces públicas de los bloques.
- `python/isdbt/`: pruebas QA y bindings Python.
- `grc/`: definiciones GRC de bloques.
- `examples/`: flowgraphs y scripts de demostración.
- `examples/TransIsdtb`: submódulo con ejemplos de datacasting, métricas y
  scripts de transmisión/recepción usados como banco experimental.
- `docs/`: documentación técnica y notas de uso.

## Cadena ISDB-Tb

La cadena física mantiene la protección estándar de ISDB-Tb:

1. Reed-Solomon `(204,188)` como FEC externo.
2. Byte interleaver.
3. Codificación convolucional como FEC interno.
4. Bit interleaver.
5. BST-OFDM modo 3 hacia RF.

La capa de datacasting se monta sobre esa cadena sin modificar la capa física.
El flujo experimental usa MPEG-TS como contenedor genérico y separa servicios
mediante PID.

## Instalación básica

Requisitos generales:

- GNU Radio compatible con módulos OOT C++/Python.
- CMake y compilador C++.
- Dependencias de ISDB-T usadas por el entorno de GNU Radio.
- USRP/UHD si se ejecutan pruebas por RF.

Compilación típica:

```bash
mkdir -p build
cmake -S . -B build
cmake --build build
sudo cmake --install build
sudo ldconfig
```

Después de instalar, los bloques se importan desde Python como:

```python
import gnuradio.isdbt as isdbt
```

## Clonado con ejemplos

Este repositorio usa `examples/TransIsdtb` como submódulo de ejemplos. Para
clonar todo el material:

```bash
git clone --recurse-submodules <repo-url>
```

Si el repositorio ya estaba clonado:

```bash
git submodule update --init --recursive
```

En esta copia de trabajo el submódulo apunta a `/home/ignatus/GitHub/TransIsdtb`,
tal como fue solicitado para el entorno local.

## Flujo experimental de datacasting

El flujo documentado en la memoria usa un transmisor y receptor SDR:

- `tx.py`/bloques embebidos: capturan tráfico, descartan no UDP, agregan
  secuencia e integridad, segmentan a TS y emiten datos nuevos junto con
  repeticiones del carrusel.
- `rx.py`/bloques embebidos: validan CRC-32, procesan control START/EOT,
  verifican hash, usan buffer de rescate y reordenan paquetes por secuencia.
- `loggermaster.py`: serializa métricas a CSV en tiempo real.
- `mastermetrics.py`, `metrics_cleaner.py`, `metrics_reporter.py` y
  `plot_per_mer.py`: consolidan, limpian, resumen y grafican resultados.

La guía completa está en `docs/datacasting-isdbtb.md`.

## Métricas principales

- `MER`: calidad de constelación, usada como eje principal de los resultados.
- `SNR`: condición física estimada del enlace.
- `PER_obs`: errores explícitos observados por el receptor.
- `PER_est`: PER corregido con pérdidas silenciosas inferidas por saltos de
  secuencia.
- `Goodput`: payload útil entregado.
- `Drops`: saturación de buffers o presión computacional local.

En el banco experimental citado, los umbrales operacionales fueron
aproximadamente `MER >= 6.5 dB` para QPSK 3/4 y `MER >= 11.5 dB` para 16-QAM
3/4. Estos valores son de laboratorio y no equivalen a certificación de
cobertura en campo abierto.

## Limitaciones

- Validación principalmente de laboratorio interior.
- Canal simplex sin canal de retorno ni retransmisión selectiva.
- Resultados sensibles al determinismo del procesador, colas y bloqueos.
- Los umbrales absolutos requieren campañas de campo más amplias.

## Licencia y origen

El manifiesto original describe `gr-isdbt` como un transceptor completo para
ISDB-T desarrollado por Federico La Rocca, Pablo Belzarena, Gabriel Gomez Sena,
Pablo Flores Guridi y Victor Gonzalez Barbone. Ver `MANIFEST.md`, `LICENSE` y
`COPYING` para detalles del origen y licenciamiento.
