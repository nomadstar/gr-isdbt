# gr-isdbt (fork nomadstar — GPU/datacasting)

Este repositorio es el fork personal de [`nomadstar`](https://github.com/nomadstar)
de [`gr-isdbt`](https://github.com/git-artes/gr-isdbt), usado como base de
trabajo de tesis. Es un módulo out-of-tree de GNU Radio para construir y
evaluar cadenas ISDB-T/ISDB-Tb con foco en transmisión SDR, recepción,
sincronización OFDM y medición de desempeño. Sobre la base de `gr-isdbt` se
agregan bloques acelerados por GPU, ejemplos y material experimental orientado
a datacasting sobre TDT. El repositorio original queda referenciado como
remoto `upstream`; todo el desarrollo nuevo de este fork vive en `master`.

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

Los módulos Python de paquetización ISDB-Tb tomaron como referencia
[`opencaster_isdb-tb`](https://github.com/0xalen/opencaster_isdb-tb), en
particular el enfoque de construir secciones MPEG-2/MPE y convertirlas a
paquetes TS para insertarlas en el flujo de transporte.

## Aceleración GPU (CUDA / ROCm)

El bloque `ofdm_synchronization_gpu` reemplaza el interpolador FIR MMSE de
`gr::filter` por un kernel GPU (`lib/ofdm_synchronization_gpu_kernel*`) para
acelerar la interpolación de muestras en la sincronización OFDM. El backend se
detecta automáticamente al configurar con CMake (`CheckLanguage`), sin
intervención manual:

- **CUDA**: backend principal, verificado — compila con `nvcc` y expone los
  símbolos `isdbt_gpu_*` en `libgnuradio-isdbt.so`. Requiere el CUDA Toolkit
  (`nvcc` en el `PATH`).
- **ROCm/HIP**: backend alternativo para GPUs AMD
  (`lib/ofdm_synchronization_gpu_kernel_hip.cpp`), con la misma interfaz
  `extern "C"` que el kernel CUDA. El código está escrito y cableado en CMake,
  pero **aún no se ha probado en hardware/toolchain ROCm real** — queda
  pendiente para una sesión futura cuando haya una GPU AMD disponible.
- Si no se detecta ni CUDA ni HIP, el bloque GPU se excluye del build
  automáticamente y el resto del módulo compila sin cambios.

## Instalación básica

Requisitos generales:

- GNU Radio compatible con módulos OOT C++/Python.
- CMake y compilador C++.
- Dependencias de ISDB-T usadas por el entorno de GNU Radio.
- USRP/UHD si se ejecutan pruebas por RF.
- Opcional, para aceleración GPU: CUDA Toolkit o ROCm/HIP (ver sección
  anterior).

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
- módulos de paquetización Python: generan secciones compatibles con MPEG-TS a
  partir de la referencia práctica de `opencaster_isdb-tb`.
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

## Capa de datacasting en C++

El flujo de datacasting descrito arriba estaba originalmente implementado en
Python en el submódulo `examples/TransIsdtb` (banco experimental de tesis).
Ese diseño se portó a C++ dentro de `lib/`/`include/gnuradio/isdbt/` de este
repositorio, manteniendo compatibilidad de bit con el formato original:

- **`datacast_tx` / `datacast_rx`**: encapsulación MPE sobre TS,
  secuenciamiento, verificación CRC-32/XXH64, carrusel adaptativo de
  repeticiones, reordenamiento y buffer de rescate en el receptor. El
  esquema de construcción de secciones MPEG-2/MPE y su segmentación a
  paquetes TS toma como referencia el enfoque de
  [`opencaster_isdb-tb`](https://github.com/0xalen/opencaster_isdb-tb)
  (`ip2sec`/`sec2ts`), igual que hacía el prototipo Python original.
- **`rms_monitor` / `mer_snr_estimator` / `metrics_logger`**: pipeline de
  métricas (RMS, MER/SNR por de-rotación de fase ciega contra constelación
  ideal, PER), publicadas a una pizarra compartida (`metrics_board`) y
  consolidadas a CSV/consola.

**`examples/TransIsdtb` se usa únicamente como referencia de lectura y no se
modifica** — es el trabajo de tesis de otra persona y sirvió de inspiración
para este proyecto. Cualquier mejora a esa lógica se implementa de forma
independiente en este repositorio, nunca escribiendo sobre el submódulo.

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

Este fork (`nomadstar/gr-isdbt`) mantiene esa licencia y agrega, por encima de
la base original, los bloques GPU y el trabajo de datacasting descritos en
este documento.
