# Black Plague — checklist de validación con visor

Actualizado: 2026-09-13.

## Estado antes de la siguiente tanda

La ruta física X/Z collision-aware de `0xD7281` está **live-tested** en PID
26144: queue, injection, resolución nativa y reconciliación emparejada, con
`12 stationary/jitter`, `37 free`, `2 blocked` y `15 slide/partial`. El jitter
horizontal de hasta `2 mm` cuenta como stationary; no necesitas mantener el
visor matemáticamente inmóvil.

PID 19192 también aclaró el comportamiento de mirror apagado. En gameplay el
pase de mundo del monitor se suprime y la ventana queda negra; en los menús el
framebuffer 2D del juego continúa visible porque no existe un `RenderWorld` que
suprimir. Ese resultado es coherente con la implementación y no indica por sí
solo un fallo.

PID 21548 repitió el camino activo después de corregir la partición del tick
combinado. El helper terminó correctamente: `queued=201 consumed=201
injected=201 matched=201`, 92 stationary, 477 free, 1 blocked, 45 slide/partial,
mirror encendido, secuencia `1.65 -> 0.95 -> 1.65 m` y recuperación posterior.
La inclinación de cabeza en el sitio ya no movió al personaje, por lo que esa
regresión concreta queda confirmada como corregida en visor.

La sesión todavía **no** valida room-scale. El mundo se veía continuamente como
en un pequeño terremoto y el rechazo al acercar la cabeza a una pared seguía
siendo agresivo. En 539 frames muestreados sin stick, el offset X/Z tuvo una
mediana de 9,65 mm y cambió 15,77 mm entre muestras; 430 ventanas invirtieron al
menos una dirección. Solo 4 de 64 resúmenes periódicos contenían rechazo físico,
así que el temblor no dependía únicamente de estar junto a una pared.

La comparación con Rework `23c890f` encontró la diferencia de presentación:
Rework coloca la vista desde su ancla VR a 90 Hz y no hereda el suavizado/bob de
posición de la cámara nativa. Black Plague retenía una muestra corporal de ~60
Hz y sumaba el offset sobre esa cámara. La corrección coloca X/Z directamente en
el ancla reconciliada y, entre ticks físicos, la continúa con el delta HMD más
reciente. Cámara, visibilidad y manos usan la misma colocación.

PID 13672 ya probó esa corrección con visor. El resultado subjetivo mejoró mucho:
el “terremoto” continuo anterior desapareció. El helper observó `stationary`,
`free` y `slide/partial`, pero al final declaró ausente `blocked` aunque la prueba
contra pared sí se había realizado. El clasificador usaba la magnitud aceptada
total; ahora usa, como Rework, únicamente el desplazamiento aceptado en la
dirección solicitada. Una corrección lateral del solver ya no puede convertir un
bloqueo directo en `slide/partial`.

PID 11804 probó después el rumbo basado en tracking. La estabilidad general se
mantuvo y el stick pasó a mover al personaje en la dirección horizontal indicada
por el HMD sin exigir recenter. La prueba descubrió una diferencia nueva: la
velocidad seguía dependiendo de si ese vector coincidía con el frente, atrás o
lateral del cuerpo nativo oculto. Mirar hacia una dirección nueva y pulsar
delante podía sentirse como caminar hacia atrás, más lento, mientras el antiguo
frente corporal conservaba la velocidad normal. El helper terminó correctamente
marcando evidencia incompleta porque esa sesión solo capturó `stationary` y
`free`; no capturó `blocked` ni `slide/partial`.

La causa está aislada. El remap corregía el rumbo, pero todavía alimentaba los
ejes `MoveForward/MoveSideways` de Black Plague, cuyas aceleraciones y límites
son distintos por eje y signo. La siguiente build usa la dirección HMD probada y
la política métrica de Rework: `1.5 m/s` al caminar y `2.25 m/s` al esprintar. El
backend introduce ese desplazamiento en el owner collision-aware `0xD7281`, una
sola vez por tick nativo, junto con la petición física. Mantiene la petición
combinada dentro de `0.05 m`, da prioridad de reconciliación a la traslación
física y conserva teclado, salto y rechazo de movimiento por estados nativos.
Esta adaptación ya tiene evidencia de visor tras la corrección final descrita
abajo.

PID 17612 sirvió para aislar un fallo de esa primera adaptación. El stick
izquierdo sí llegaba a OpenVR con deflexión completa, pero no producía ningún
movimiento y toda la telemetría `locomotion_*` permanecía a cero. La causa era
el gate usado antes de publicar: al quitar el componente VR de los ejes nativos,
`MoveForward/MoveSideways` recibían `amount=0` y retornaban antes de escribir
`cPlayer+0x264`, de modo que ese byte nunca podía confirmar permiso. El bridge
corregido replica únicamente el predicate pre-Move exact-build (dos gates de
estado y `+0x268/+0x26C`), mantiene prioridad para un eje nativo real y publica
la locomoción métrica por `0xD7281`. La corrección compila en x86 Release y el
verificador exact-build pasa, pero aún necesita una nueva prueba con visor.

PID 28996 demostró que ese primer predicate corregido aún resolvía mal los dos
estados indexados: se había perdido el `0x200` de los campos y se habían
intercambiado vector e índice. La implementación actual usa exactamente
`vector +0x2C4 / index +0x2BC` y `vector +0x2D8 / index +0x2D0`.

PID 8092 validó esa corrección con visor. El usuario confirmó que el stick vuelve
a mover correctamente al personaje y que la sensación general es mucho mejor.
El helper cerró además `direct_locomotion=True`, los cuatro outcomes físicos,
queue/consume/inject/match `201/201/201/201`, mirror activo y recuperación tras
el cambio nativo de forma crouch/stand. Esto cierra la ruta técnica de stick y
colisión como evidencia de investigación. No cierra el confort posicional, que
PID 20520 volvió a dejar abierto; el siguiente gate combina esa comprobación
corta con el crouch físico por altura HMD.

El visor de 90 Hz y el cuerpo nativo de 60 Hz son frecuencias esperadas y sí
importan para el confort: tracking, cámara y presentación deben continuar a 90
Hz, mientras cuerpo y colisiones se reconcilian a 60 Hz. PID 11804 mostró esa
división sin recuperar el terremoto anterior. No explica la asimetría de
velocidad; esa procedía de los ejes nativos descritos arriba.

PID 20520 ejercitó la primera integración de crouch físico. El detector de altura
funcionó, pero el resultado no queda validado: al ponerse de pie, el body nativo
seguía a `0.95 m` hasta que otro gesto de agacharse generaba el siguiente flanco.
El helper dio un falso positivo porque comparaba contadores y una secuencia de
formas agregada sin exigir que cada salida física coincidiera con el retorno a
`1.65 m`. El mismo log registra seis entradas/salidas de política y varias formas
nativas invertidas, pese a que la prueba buscaba dos ciclos controlados.

La causa era una diferencia concreta con Rework `23c890f`: el Framework enviaba
un estado held/released a las consultas configurables de crouch de Black Plague,
mientras Rework mantiene un latch único y aplica el move-state deseado. La nueva
implementación conserva la política común de baseline, rango plausible
`(0.90, 2.20) m`, profundidad `0.25 m` e histéresis `0.08 m`, añade el latch de
botón probado de Rework y sincroniza el body desde el owner existente del hilo de
juego mediante las entradas exactas `0x9CFA0/0x9CFD0`. Si el techo impide ponerse
de pie, reintenta la salida en ticks posteriores. No añade otro hook.

También se ha conectado el eje Y demostrado por Rework usando
`runtime::VrTrackingSpace`: la altura de la vista sigue de forma continua la
altura física del HMD, el ancla reconciliada sigue representando los pies y el
descenso nativo completo de cámara no se suma al crouch físico. El crouch de
botón usa solo la profundidad configurada. `HeightOffset` queda cableado para
Black Plague. Todo esto compila y pasa pruebas de host, pero todavía requiere una
prueba con visor. El usuario también describió un resto de corrección/molestia al
moverse físicamente en X/Z; la evidencia agregada de PID 20520 no permite
atribuirlo a una colisión concreta, así que sigue siendo un gate de confort.

PID 23260 separó definitivamente la política compartida del fallo nativo. El
log alcanzó `physical entries/exits=10/10`, y `button_latched` cambió en ambos
sentidos, así que tanto el crouch físico como el click derecho llegaron al
runtime. Sin embargo el body terminó todavía a `0.95 m`, con `native_exits=0`,
`vr_owned=1` y `stand_retries=17832`. La build de esa prueba trataba la entrada
nativa de release como un stand incondicional; Black Plague conserva su modo
hold/toggle y, en toggle, release deja la postura latched. La corrección actual
envía release y, solo si el body sigue agachado, el segundo press nativo que el
propio juego usa para solicitar volver a de pie. Compila en Release y pasa 30/30
tests de host; sigue pendiente la validación con visor.

## Siguiente tanda — crouch físico por altura HMD

Desde la raíz del repositorio ejecuta:

```powershell
tools\Start-BlackPlagueRoomScaleValidation.ps1
```

PID 8092 ya cerró el gate técnico anterior de room-scale y stick. Esta tanda
comprueba la corrección posterior a PID 23260 y el nuevo eje Y. El helper mantiene las
rutas activas, pero no obliga a caminar por la habitación, esprintar ni repetir
`blocked`/`slide`. Además:

- valida la imagen exact-build antes de iniciar;
- exige un proceso nuevo de `penumbra.exe`;
- activa los mutex transitorios de physical displacement y room-scale;
- guarda `MonitorMirror=true` antes del arranque;
- rechaza la sesión si el probe no confirma los modos de validación;
- registra `physical_crouch` con altura HMD, latch, estado deseado, forma nativa,
  ownership, reintentos y contadores sincronizados;
- rechaza la antigua falsa aprobación: exige dos ciclos físicos correlacionados,
  terminar realmente de pie y al menos `15 cm` de recorrido visual vertical;
- al cerrar el juego analiza únicamente el log fresco de ese PID.

Mantén abierta la consola durante toda la sesión. El log queda en:

`%LOCALAPPDATA%\PenumbraVR\logs\black-plague-probe-<PID>.log`

### Pruebas obligatorias

1. **Calibración de pie.** Entra en una partida ya cargada y empieza totalmente
   de pie. No pulses crouch. Mantén una postura natural al menos 10 segundos. El
   log debe fijar `standing_known=1`; no empieces agachado porque Rework usa la
   primera altura plausible como baseline y solo permite corregirla hacia arriba.
2. **X/Z en poco espacio.** Sin agacharte ni usar sticks, desplaza cabeza y torso
   solo `5–10 cm` hacia delante, atrás y ambos lados, regresando al mismo punto.
   La vista debe acompañarte de forma continua. Anota cualquier tirón, oscilación
   o sensación de que el mundo te arrastra de vuelta; no hace falta dar pasos.
3. **Entrada física y altura continua.** Sin pulsar crouch, baja lentamente
   cabeza y torso al menos `25 cm` y mantén 5–6 segundos. La altura visual debe
   seguir todo el gesto; al cruzar el umbral, el body pasa a `0.95 m` sin añadir
   una caída profunda o instantánea de cámara.
4. **Histéresis y salida.** Sube solo un poco y mantente cerca del umbral.
   No debe alternar rápidamente entre crouch/stand; Rework exige `8 cm` extra de
   recuperación. Después ponte totalmente de pie durante 5–6 segundos. Vista y
   body deben volver inmediatamente a de pie; no debes tener que agacharte otra
   vez para provocar la salida.
5. **Segundo ciclo.** Repite entrada y salida física completas. La consola exige
   dos entradas y dos salidas tanto en política como en el body nativo.
6. **Botón como toggle Rework.** Empieza totalmente de pie. Pulsa una vez el
   click de crouch del stick derecho: debe quedarse agachado al soltar. Espera
   2–3 segundos y pulsa otra vez: debe volver a `1.65 m` inmediatamente. Repite
   una vez más para confirmar que no depende de haber usado antes crouch físico.
7. **Composición Hybrid.** Agáchate físicamente, pulsa una vez crouch y levántate
   físicamente. El latch del botón debe mantener el crouch. Pulsa otra vez y debe
   liberar la postura y volver a `1.65 m` sin otro gesto físico. Repite el orden
   inverso: botón para agacharte, baja físicamente, quita el latch con otro click
   y después levántate. El body no debe quedar atrapado a `0.95 m`.
8. **Stick y combinación breve.** Usa un desplazamiento corto de stick de pie y
   agachado. Combínalo una vez con un movimiento físico de `5–10 cm`, suelta el
   stick y vuelve al punto inicial. No debe quedar deriva, pullback ni temblor.
9. **Presentación.** Comprueba manos y mirror durante subida, bajada y traslación.
   No necesitas sprint, pared, Alt+Tab ni caminar por la habitación.

No uses esta tanda para ajustar salto, mecanismos o Enhanced Visuals. El objetivo
es decidir si crouch, altura física y movimiento corto ya se comportan como una
unidad coherente y cómoda.

### Resultado automático exigido

Al cerrar el juego, el helper solo termina con éxito si el log fresco demuestra:

- petición de locomoción directa no nula consumida, inyectada y aceptada;
- tick nativo aproximado `dt=1/60`;
- sample room-scale fresco aplicado a la cámara;
- mirror activo en un frame de gameplay;
- baseline HMD plausible;
- dos entradas y dos salidas físicas;
- dos transiciones nativas a `0.95 m` y dos retornos a `1.65 m`, alineadas con la
  política;
- estado final realmente de pie y ownership VR liberado;
- recorrido vertical renderizado mínimo de `15 cm`;
- sample room-scale fresco después del último retorno a de pie.

La aprobación automática demuestra correlación técnica, no comodidad. Para
promoverlo a **headset-validated** también necesito el resultado visual de los
puntos anteriores.

## Qué devolver después de la prueba

Conserva y comunica:

- PID de la sesión;
- salida final completa del helper;
- si el mundo siguió los movimientos X/Z cortos o intentó arrastrarte al origen;
- si bajar/subir fue continuo y si el umbral produjo algún salto vertical;
- si cada salida física puso al personaje de pie sin un segundo gesto;
- resultado del toggle de botón y de la composición Hybrid;
- si stick, manos y mirror siguieron correctos antes, durante y después;
- cualquier mareo, temblor, doble movimiento o deriva, indicando si fue X/Z o Y.

Si el helper falla, no repitas a ciegas. El mensaje enumera la evidencia ausente;
conserva ese texto y el log del PID para separar un escenario no capturado de un
fallo real de cámara, reconciliación, ownership o presentación.

Los movimientos grandes causados al quitarse parcialmente el visor no sirven
para juzgar sensibilidad ni comfort, pero tampoco invalidan las muestras
collision-aware: el límite sigue acotando cada petición a `0.05 m`. Para las
pruebas de los puntos 2 y 8 usa movimientos pequeños y continuos; los pasos
completos ya aparecen impresos en la propia terminal para evitar consultar otra
pantalla.

## Gate anterior — repetir solo ante regresión

La prueba sin traslación activa sigue disponible:

```powershell
tools\Start-BlackPlaguePhysicalDisplacementValidation.ps1
```

Esa ruta mantiene `positional_translation_enabled=0` y sirve para aislar una
regresión en `0xD7281`. No es la siguiente tanda normal porque PID 26144 ya la
promocionó a **live-tested**.

Mantén separados los estados `implemented`, `host-tested`, `live-tested`,
`headset-validated` y `supported`. PID 8092 aporta evidencia de visor para la
locomoción directa; la corrección conjunta de crouch/Y y el confort X/Z de la
build actual siguen en `host-tested` hasta completar la sesión descrita arriba.
