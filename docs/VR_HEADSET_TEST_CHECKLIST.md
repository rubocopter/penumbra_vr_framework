# Black Plague — checklist de validación con visor

Actualizado: 2026-09-16.

## Gate del candidato Release

Antes de cualquier prueba de crouch, room-scale o interacción, realiza un gate
de presentación de 20-30 segundos. Los candidatos `3333be1` y `ca099ca` quedan
descartados: ambos reprodujeron `VRCompositorError_AlreadySubmitted (108)` al
pasar de menú a gameplay, y PID 6016 demostró que el segundo todavía podía
reutilizar un snapshot de compositor ya enviado. PID 22096 probó después la
corrección de consumo único de `7f84235`: 15.990 frames registrados, 15.887 de
gameplay y **cero fallos estéreo/compositor**. Ese resultado cierra el error 108
como bloqueo actual, pero cada candidato posterior debe repetir este gate breve
antes de validar confort. En el candidato actual:

1. entra al menú con visor y mandos activos;
2. carga una partida y permanece quieto 10 segundos;
3. mueve sólo la cabeza y verifica imagen estéreo continua durante otros 10-20
   segundos;
4. si el visor se queda negro, vuelve a escritorio o aparece cualquier pérdida
   de controles, cierra el juego ahí y no continúes con los demás gates;
5. el log debe mostrar secuencias de presentación nuevas y puede mostrar
   `presentation_pose_stale_rejects`, pero no debe contener
   `VRCompositorError_AlreadySubmitted (108)`.

Analiza una ejecución cerrada con
`tools\Analyze-BlackPlaguePresentation.ps1 -ProcessId <PID> -CollisionComfort`
antes de interpretar el resto de la tanda. Sin `-CollisionComfort` el script
sigue dando el resumen de presentación habitual.

El SHA-256 `48489A2573BC96F56C55F0E0C2E3559450D8E4BD249767C349777B92F1BE1A76`
corresponde al candidato histórico del gate de presentación. No identifica la
build actual con integración gameplay de palmas; valida siempre el candidato
recién compilado y el ejecutable exact-build antes de iniciar la tanda.

La siguiente build Release debe tratarse como **candidato de validación**, no
como release soportada. PID 25484 ya aporta evidencia con visor para el
crouch/Y actual, locomoción directa y el filtro X/Z; los edges de postura,
locomoción restringida, lifecycle y la integración gameplay de palmas conservan
su nivel documentado hasta obtener la evidencia específica indicada aquí. Black
Plague permanece default-off fuera de los gates transitorios de validación.

Para cerrar el candidato con visor, la tanda debe cubrir, en este orden:

1. **gate de presentación primero**: desde menú entra en gameplay, confirma
   imagen estereoscópica estable en visor, controles activos y al menos unos
   segundos de envío sostenido sin `VRCompositorError_AlreadySubmitted (108)`;
   si falla aquí, detén la tanda y conserva el log fresco del PID. El log nuevo
   expone `presentation_pose_acquisitions` y `presentation_pose_reuses` para
   comprobar que el owner exterior adquiere la muestra y los eye passes la
   reutilizan;
2. baseline de pie y dos ciclos completos de crouch físico `4/0` con shapes
   `0.95/1.65 m`;
3. crouch por botón mantenido y composición Hybrid, incluyendo stand bloqueado
   bajo techo y recuperación al quedar espacio;
4. movimiento físico X/Z corto de `5–10 cm`, quietud e inclinación de cabeza,
   sin pullback, deriva ni oscilación perceptible;
5. stick normal y sprint con varios yaw, conservando `1.5/2.25 m/s`, más una
   combinación breve de stick y desplazamiento físico opuestos;
6. un caso Push y un caso Move para validar en visor la rama restringida
   `0.5 m/s`; sprint no debe elevar esa velocidad;
7. seated/standing, recenter y pérdida/recuperación de tracking sin doble
   integración ni salto de época de yaw;
8. pared, slide y esquina para comprobar rechazo/reconciliación sin repetir la
   validación ya cerrada de `0xD7281` salvo regresión;
9. manos, herramientas, free-body grab/release y cambio de mapa mientras se
   sostiene un objeto, verificando que un hold de una generación anterior no se
   reutiliza;
10. mirror/focus y Alt+Tab como gate separado; si reaparece el crash histórico de
   SDL, conservar dump y lista de módulos sin atribuir causa por proximidad;
11. yaw/footstep-body-bob como gate de confort separado antes de cualquier
    promoción a `supported`.

La palma collision-aware ya está conectada al gameplay en estado host-tested.
PID 28412 cerró el query nativo no-write y PID 8644 cerró
`--validate-palm-resolver` en proceso real, con lifecycle equilibrado y sin
cambios de memoria gameplay. El path actual publica el held body por mano,
mantiene aim en tracking raw y usa la palma resuelta para manos, objetos ya
poseídos y herramientas; la adquisición puede seguir el controlador raw hasta
`0,18 m` desde la palma detenida, como Rework. `Grab=6` y el `Move=2` free-body
tienen ownership distinto. PID
23000 llegó a este path con visor, pero la pérdida severa de FPS y la caída del
mando derecho invalidan la tanda como evidencia de promoción. La siguiente
validación debe usar `tools\Start-BlackPlaguePalmCollisionValidation.ps1` tras
reinicio: el helper corregido activa también room-scale/desplazamiento físico y
comprueba la composición. Antes y después del contacto de palma verifica
desplazamiento físico X/Z corto y crouch/stand, además de ambos mandos, objetos
representativos, pared/slide y un mecanismo nativo. PID 4720 no sirve para
comparar room-scale porque el helper antiguo lo lanzó con esa ruta desactivada.

### Build preparada para la siguiente sesión

PID 25484 (2026-09-16) ya ha ejercitado el candidato actual con visor. El
análisis cerrado registró 12.949 frames, 12.859 de gameplay, 12.858 de
presentación y cero fallos estéreo; además contó 2.477 muestras de cuerpo
(`2407 free / 43 blocked / 27 partial`) y 5.488 frames activos de room-scale.
La telemetría de crouch obtuvo baseline válido, `3/3` entradas/salidas físicas,
estado final `0` con body `1.65 m` y ownership VR liberado, toggle por botón
estable en state `4`, `0.961 m` de recorrido Y renderizado y 650 muestras de
locomoción directa aceptada. El usuario indicó que la sesión se sentía bien.

No hace falta repetir toda la tanda para volver a demostrar esos puntos. El log
sí capturó el estado Hybrid combinado (`physical=1`, latch=1, state `4`), pero no
capturó después un sample periódico con la fuente física ya liberada y el latch
todavía activo. Por tanto, el subgate Hybrid release-hold sigue pendiente de una
repetición corta y dirigida. El stand bloqueado/techo bajo y los gates separados
de yaw/bob siguen pendientes igualmente.

Los candidatos `3333be1` y `ca099ca` quedan descartados por error 108. `7f84235`
es ahora el baseline de presentación con evidencia positiva de visor en PID
22096: menú -> gameplay sostenido sin fallos estéreo. El candidato actual añade
encima el filtro de confort X/Z: la reconciliación física del último tick viaja
al renderer y la predicción entre ticks elimina únicamente la componente que
continúa hacia la dirección rechazada, preservando slide tangencial y movimiento
de salida. PID 25484 aporta evidencia positiva de visor y confort subjetivo para
este cambio; pared/slide deliberados y la reproducción específica del antiguo
pullback siguen siendo evidencias más estrechas si se necesitan para cerrar esos
casos por separado.

El candidato de prueba con visor se genera directamente en
`build\bin\Release`. No hace falta crear una release pública ni copiar DLLs al
directorio del juego: el helper usa
`build\bin\Release\PenumbraVR.ProbeLauncher.exe`, valida primero el ejecutable
exact-build soportado y lanza Black Plague por la ruta de inyección existente.

La preparación offline de 2026-09-14 queda cerrada con:

- Release: 34/34 CTest;
- Debug: 34/34 CTest;
- Release sin SDK OpenVR: 34/34 CTest;
- metadata: 6 entradas de catálogo, 2 manifests exact-build, 42 actions,
  6 action sets y 8 bindings;
- verifier Black Plague exact-build: pasa incluyendo locomoción, ownership,
  cuerpo/colisión y ABI/contacto de palma;
- Overture `-Full`: build Release, Large Address Aware, 16 shaders, 8.752
  comprobaciones visuales CPU, selección/decodificación de 231 texturas y
  289/289 `VRTrackingTest`.

Esto deja la build **lista para validación con visor**, no `headset-validated`
ni `supported`.

Para iniciar la tanda principal desde la raíz del repositorio:

```powershell
tools\Start-BlackPlagueRoomScaleValidation.ps1
```

Si Black Plague no está en la ruta Steam por defecto:

```powershell
tools\Start-BlackPlagueRoomScaleValidation.ps1 -GamePath "X:\ruta\Penumbra Black Plague\redist\penumbra.exe"
```

El helper debe permanecer abierto hasta cerrar el juego. Imprime los pasos de
la tanda, mantiene los gates transitorios activos y al final analiza únicamente
el log fresco de ese PID. No lances `penumbra.exe` manualmente para esta tanda.

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
botón probado de Rework. La primera adaptación de backend intentó sincronizar el
body mediante las entradas `0x9CFA0/0x9CFD0`; PID 23260 y PID 24948 demostraron
que esos callbacks configurables no sirven como owner persistente del estado.
La implementación vigente sincroniza desde el owner existente del hilo de juego
mediante `cPlayer::ChangeMoveState` (`0x9C750`), con crouch=`4` y walk=`0`. Si el
techo impide ponerse de pie, reintenta la salida en ticks posteriores. No añade
otro hook.

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
hold/toggle y, en toggle, release deja la postura latched.

PID 24948 probó la corrección release/segundo-press y reveló el siguiente límite.
Ya no quedó atrapado permanentemente a `0.95 m`, pero el usuario observó que el
botón solo producía una bajada/subida breve y nunca mantenía el crouch/sigilo real
del juego. El log coincide: terminó con `native_entries=14` y
`native_exits=14`, señal de que la forma nativa alternaba repetidamente. La
revisión exact-build localizó `cPlayer::ChangeMoveState` en `0x9C750`; los
handlers originales prueban que el estado `4` es crouch, el `0` es walk y el
`3` sigue siendo jump. La build actual aplica directamente `4/0` desde el owner
del hilo de juego, como hace conceptualmente Rework con su desired state. La
caída visual de botón sigue siendo deliberadamente la profundidad VR configurada
(`0.25 m` por defecto); lo nuevo que debe quedar estable es el move-state `4`,
la forma `0.95 m` y el comportamiento de crouch/sigilo. Compila en Release,
pasa el verificador exact-build y 30/30 tests de host; sigue pendiente de visor.

## Siguiente tanda — crouch físico por altura HMD

Desde la raíz del repositorio ejecuta:

```powershell
tools\Start-BlackPlagueRoomScaleValidation.ps1
```

PID 8092 ya cerró el gate técnico anterior de room-scale y stick. Esta tanda
comprueba la corrección posterior a PID 24948 y el nuevo eje Y. El helper mantiene las
rutas activas, pero no obliga a caminar por la habitación, esprintar ni repetir
`blocked`/`slide`. Además:

- valida la imagen exact-build antes de iniciar;
- exige un proceso nuevo de `penumbra.exe`;
- activa los mutex transitorios de physical displacement y room-scale;
- guarda `MonitorMirror=true` antes del arranque;
- rechaza la sesión si el probe no confirma los modos de validación;
- registra `physical_crouch` con altura HMD, latch, estado deseado, forma nativa,
  **move-state nativo**, ownership, reintentos y contadores sincronizados;
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
   click de crouch del stick derecho y permanece quieto al menos 5 segundos. El
   juego debe entrar y **mantener** su crouch/sigilo real: `native_state=4` y
   body `0.95 m`; no vale una bajada/subida momentánea. La vista puede bajar solo
   ~`0.25 m`, porque esa es la postura VR de confort heredada de Rework. Pulsa
   otra vez, espera otros 5 segundos y debe quedar `native_state=0`, body
   `1.65 m`. Repite una vez para que la telemetría periódica capture ambos lados.
7. **Composición Hybrid.** Agáchate físicamente, pulsa una vez crouch y levántate
   físicamente sin volver a pulsar. El latch debe mantener **move-state 4**, el
   crouch/sigilo y body `0.95 m` aunque la fuente física ya haya salido. Pulsa
   otra vez solo estando de pie: debe pasar a state `0` y `1.65 m`.
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
- dos transiciones físicas alineadas con **move-state 4 + `0.95 m`** y retorno a
  **move-state 0 + `1.65 m`**;
- un intervalo button-only estable con `physical=0`, latch activo,
  `native_state=4` y body `0.95 m`, seguido de segundo click a state `0`;
- composición Hybrid donde al liberar solo el crouch físico el latch conserva
  state `4` hasta el segundo click;
- estado final realmente de pie, move-state `0` y ownership VR liberado;
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
