# Black Plague: integración espacial y punto de continuación

2026-09-06. Solo imagen inicializada FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF.
Base virtual 0x00400000. Los números siguientes son RVAs, nunca offsets del PE
protegido en disco. Captura local: `artifacts/black-plague-22000-live.bin`.
Rework de referencia: 23c890f7dbd06b939be9951d282e6e948d9a6623, sin modificar.

## Checkpoint histórico de reconciliación shadow — 2026-09-11

En este checkpoint se había implementado la política compartida de plan/rebase,
rechazo físico y arrastre por locomoción. BP sólo planificaba el paso físico; el
desplazamiento nativo aceptado alimentaba el arrastre de ancla shadow y no
simulaba aceptación del plan. No había inyección física ni cambios de cámara.
El diagnóstico estaba desactivado
por defecto y registra un resumen cada 300 swap frames si se habilita mediante
`PVR_BP_RECONCILIATION_SHADOW=1` en el proceso del juego antes del attach.

El milestone está ahora **host-tested**. Tras el push, GitHub Actions pasó
metadata, configuración Win32 y build/CTest Debug+Release del Framework. También
se añadió y pasó un gate CI separado que reconstruye Overture Release con
`Build-OvertureProduct.ps1 -Configuration Release -Full`, conservando sus
comprobaciones históricas. Esto no promueve ningún estado live/headset de BP.

Este texto conserva el estado histórico del 11 de septiembre. Después se
live-testó el shadow en PID 28172 y la solicitud física X/Z separada en
`0xD7281` quedó live-tested en PID 26144. La traslación posicional estuvo a
cero en esa sesión. PID 21548 ejercitó después el consumidor room-scale X/Z
activo mediante doble opt-in y pasó queue/injection/reconciliation, las cuatro
clases de colisión y la recuperación tras `1.65 -> 0.95 -> 1.65 m`. La corrección
del carry evitó ya la locomoción al inclinar la cabeza. La experiencia siguió
fallando por temblor continuo y rechazo agresivo junto a paredes. El log mostró
cambios X/Z centimétricos e inversiones frecuentes aun sin stick. Rework coloca
la vista desde su ancla VR a 90 Hz y no conserva el suavizado/bob horizontal de
la cámara nativa; el consumidor BP retenía una muestra corporal de 60 Hz sobre
  esa cámara. Ahora el render coloca X/Z en el ancla reconciliada y la continúa
  con el delta HMD posterior a la muestra. PID 13672 confirmó con visor que el
  temblor continuo desapareció. PID 11804 confirmó después que la dirección del
  stick sigue el HMD sin recenter, pero expuso que el remap conservaba las
  velocidades distintas de los ejes/signos del cuerpo nativo. La ruta transitoria
  usa ahora la política directa `1.5/2.25 m/s` de Rework dentro del único request
  `0xD7281`; PID 8092 aportó evidencia de visor para esa ruta técnica. PID 20520
  detectó después que el primer crouch físico no sincronizaba la salida nativa y
  que el movimiento X/Z corto todavía podía sentirse como un pullback. PID 22096
  validó después la corrección de presentación de `7f84235` con 15.990 frames y
  cero fallos estéreo, y su log permitió aislar mejor el pullback: la predicción
  HMD entre ticks podía seguir avanzando en la dirección que el último solve
  físico acababa de rechazar. El filtro actual transporta esa reconciliación al
  render y elimina solo la componente hacia la dirección rechazada; slide y
  movimiento de salida se conservan. PID 25484 aportó evidencia positiva con
  visor para este camino y el usuario informó de buena sensación general. PID 23260
  confirmó que latch/altura y botón sí llegaban al runtime, pero la salida nativa
  seguía bloqueada por la semántica toggle del dispatch release. PID 24948 mostró
  que compensar con release/segundo press seguía alternando el collider sin
  mantener el crouch/sigilo real. La captura exact-build sitúa
  `cPlayer::ChangeMoveState` en `0x9C750`; los handlers originales prueban
  crouch=`4`, walk=`0`. El backend actual conserva el desired state de Rework y
  aplica esos estados directamente. El tracking Y continuo y esta adaptación
  están corregidos. PID 25484 registró tres entradas/salidas físicas, estado
  final de pie, `0.961 m` de rango Y renderizado y 650 muestras aceptadas de
  locomoción directa. Falta capturar el intervalo Hybrid con la fuente física
  ya liberada mientras el latch mantiene state `4`, además del stand bloqueado
  bajo techo y los casos deliberados de pared/slide. No ampliar reversing
  automáticamente.

## Player/body/movement/collision: mapa estático e instrumentación

El límite nativo se ha localizado en la captura inicializada, sin copiar RVAs
de Overture y sin habilitar traslación posicional:

```text
cButtonHandler::Update -> cPlayer (+0x38 en el handler)
                       -> iCharacterBody* en cPlayer+0x274
cPlayer::MoveForward/MoveSideways (9CBC0/9CC60)
                       -> iCharacterBody::Move (D4F50)
iPhysicsWorld::Update (D45B0), lista +0x20
                       -> call D460A -> iCharacterBody::Update (D6E00)
iCharacterBody::Update -> posición deseada
                       -> call D7312 -> CheckShapeWorldCollision (D4830)
                       -> step/gravedad/attachments -> posición final aceptada
```

El `Move` nativo no acepta un desplazamiento: integra `amount * acceleration *
frameTime` en los campos de velocidad direccional `+0x70/+0x74`, usa
aceleración `+0x78/+0x7C`, flags `+0x88/+0x89` y límites nativos
`+0x60..+0x6C`. El timestep físico relevante es el que `iPhysicsWorld::Update`
pasa una vez a cada character body en el callsite `D460A`; el contador previo de
`ButtonHandler` no era ese límite.

El `iCharacterBody` guarda posición actual en `+0x48`, posición anterior en
`+0x54`, tamaño activo en `+0xC4`, `iPhysicsBody*` activo en `+0x23C` y
`iPhysicsWorld*` en `+0x240`. `GetFeetPosition` (D50E0) devuelve la posición del
cuerpo menos media altura de la shape. El constructor D63A0 calcula radio como
`max(size.x,size.z)/2`; sólo usa esfera cuando el diámetro coincide con la
altura dentro de 0,01 y, en otro caso, crea un cilindro rotado 90 grados en Z.
No hay una cápsula separada ni un collider de cabeza del Framework.

`body_collision_probe.cpp` instala dos hooks de observación exact-build. El
primero envuelve únicamente el callsite `D460A`; el segundo envuelve únicamente
la primera resolución horizontal `D7312`. Por tick del player correlaciona:

- player, character body, physics body y physics world;
- tamaño/radio/tipo de shape, posición de cuerpo y pies antes/después;
- timestep de física;
- desplazamiento solicitado al solver, salida inmediata del solver y
  desplazamiento final aceptado tras step/gravedad;
- offset físico HMD/ancla, registrado como divergencia todavía no aplicada.

Las firmas estables y ambos `E8` se comprueban antes de tocar memoria. Una
imagen distinta conserva el comportamiento nativo. La prueba sintética cubre
aceptación parcial, fase vertical posterior, offsets, timestep, desmontaje y
rechazo cerrado de bytes desconocidos. La sesión manual del 2026-09-10
(`black-plague-probe-30896.log`) confirmó en vivo player/body/world estable,
cilindro `0.70 x 1.65 m` (radio `0.35 m`), `dt=1/60`, movimiento libre y tres
rechazos reales frente a geometría. La divergencia HMD/cuerpo no aplicada fue
0--0.436 m (mediana 0.105 m), incluido lean físico. Estado: `implemented ->
host-tested -> live-tested`; no implica validación de room-scale. La traslación
posicional continúa forzada a cero.
`tools/Test-BlackPlagueInputMap.ps1` verifica además estos callsites, entradas y
firmas directamente contra la captura inicializada, sin modificar procesos.

El milestone de probing corporal quedó cerrado después de las capturas PID
24780, 29672 y 8628. Sprint/crouch/jump ownership está caracterizado, el salto
tiene burst live completo y el primer `BlackPlagueBodyAdapter` está live-tested.
No se deben añadir más sondas corporales por defecto. La reconciliación shadow
sobre ese adapter ya está live-tested en PID 28172 con traslación HMD a cero. El
boundary dedicado de petición física X/Z collision-aware en metros está
live-tested en `0xD7281`, con clamp horizontal de `0,05 m`, Y a cero, consumo
one-shot ligado al body actual y sin duplicar `D6E00`. PID 26144 produjo
queue/injection/reconciliation y muestras free/blocked/slide; al corregir el
clasificador stationary para tolerar jitter sub-2 mm, el mismo log aporta 12
muestras stationary. Room-scale/traslación HMD activa ya está implementado
detrás del request de validación separado y debe validarse ahora a través de
este boundary. Cámara/bob sigue como pista de comfort separada.
El launcher dedicado ya exige telemetría fresca de queue/injection/collision/
reconciliation y clasifica muestras stationary/free/block/slide-or-partial a
partir del request físico y el desplazamiento aceptado desde la posición previa
a la inyección; no añade otro hook. PID 18392 confirmó que una sesión que solo
activa el mutex falla correctamente y no promociona este boundary a live-tested.

### Comparación concreta con Overture

Las primitivas neutrales de tracking/locomoción en `src/runtime` y la
orquestación probada de `OvertureBackend` siguen siendo la referencia para la
política compartida: escala 1 unidad/m, pasos físicos de 0,05 m, epsilon de
rechazo 0,002 m, rebase a 0,8 m, 1,5/2,25 m/s y reconciliación del head anchor.
El adapter de fuente de Overture puede escribir campos añadidos por Rework,
activar su modo static-only y controlar explícitamente el update; esos detalles
no existen como contrato binario en BP.

La primera extracción común ya existe: `runtime::VrAcceptedBodyMotion` recibe
posición antes/después y desplazamiento aceptado sin conocer RVAs, layouts ni
ownership del tick. Overture lo consume dentro de su comportamiento probado y
`BlackPlagueBodyAdapter` lo produce después del único tick nativo `D460A ->
D6E00`. La ruta normal de BP conserva sus límites 3.0/4.5 m/s y su vertical/jump
nativo. El gate room-scale transitorio evita esos ejes solo para el analog VR y
encola la política compartida `1.5/2.25 m/s` en `0xD7281`; no se emulan campos
añadidos por Rework como `vr_velocity` ni argumentos de solver que no existen en
la build binaria. La semántica de step físico de Rework se adapta en la frontera
exact-build: se observa el ray callback nativo y, en cualquier tick que contenga
traslación room-scale física, se descarta un ganador dinámico o uno cuya normal
no cumpla `normal.y >= 0.5`.
Esta segunda condición importa porque el `cCharacterBodyRay` binario de BP solo
conserva distancia/collide, mientras Rework `23c890f` sí aplica la normal
ascendente antes del step. El layout exacto de `cPhysicsRayParams` queda fijado
por el callback Newton de la build soportada; no se añade campo, segundo raycast
ni segundo update.

La segunda extracción común también está hecha: `PlanBodyReconciliation`,
`ReconcilePhysicalBodyMotion` y `CarryHeadAnchorWithLocomotion` viven en
`vr_locomotion.*`. Overture los ejecuta en el orden probado. BP conserva el
shadow sólo como observación y dispone de una ruta física separada, live-tested
en PID 26144, que produce la observación correspondiente a su request inyectado.
La locomoción directa reutiliza esas primitivas runtime y el mismo owner, pero su
composición single-tick y su partición de aceptación son mecanismo BP. No mezclar
crouch físico, jump, bob, estados o efectos de pasos dentro de la política común.

### Ownership del movimiento plano: estado confirmado

| Concern | Black Plague owner/evidence | Relación con `D6E00` | Estado |
|---|---|---|---|
| Intento plano | `cButtonHandler::Update`: `51CD/5227 -> cPlayer::MoveForward/MoveSideways` (`9CBC0/9CC60`) | Antes | Live-tested a través del adapter |
| Aceleración/objetivo | `cPlayer` comprueba state/ground y llama `iCharacterBody::Move(D4F50)`; éste suma `amount * acc * dt` en `+70..+7C`, marca `+88/+89` y limita por `+60..+6C` | Consumido por `D6E00` | Mapeado + comportamiento live |
| Locomoción VR directa transitoria | Runtime construye dirección HMD y `1.5/2.25 m/s`; los action states BP exactos `1=Push` y `2=Move` seleccionan `0.5 m/s`; `NativeInputBridge` replica el predicate pre-Move exact-build (dos gates de state + `+268/+26C`) y `BodyCollisionProbe` combina/acota físico+stick en `0xD7281` | Una inyección dentro del único `D6E00` | Ruta normal headset-validated en PID 8092; mapping restringido `0.5 m/s` host-tested solamente |
| Deceleración y velocidad final | `D6E00` consume flags, aplica deacc y convierte los campos de velocidad en request horizontal antes de `D7312` | Dentro, antes de colisión | Mapeado |
| Sprint | queries `52EB/5313` llegan a wrappers `9CF40/9CF70`, que delegan al move-state actual; inicialización de state aplica los límites por setters nativos | Antes | Live-characterized; ~3.0/4.5 m/s efectivos |
| Jump | `5299 -> 9CEA0` selecciona estado 3 (`cPlayerMoveState_Jump`); `52CF -> 9A890` gestiona hold `+1FC/+200/+204` | La fuerza/estado vertical se publica antes de `D6E00`; horizontal mantiene Y=0 y la vertical se aplica después | Live-characterized: ~5.53 m/s inicial, apex ~0.95 m, landing nativo y 3→0 |
| Crouch | Runtime posee latch/altura/Hybrid; el owner existente aplica el desired state mediante `cPlayer::ChangeMoveState` (`0x9C750`), con crouch=`4` y walk=`0`, y observa tanto `+0x2D0` como shape `+0xC8` | Antes | Mecánica nativa live-characterized; owner directo host-tested tras PID 20520/PID 23260/PID 24948 |
| Colisión/step/gravedad | `D6E00`, primer solver `D7312`, fases posteriores de step/gravedad | Dentro | Live-tested para el límite |
| `D790C/D7913` | Sync sólo de la rama con gravedad desactivada | Después | No son composición general de cámara; el player activo los evita |
| Head/footstep bob | No hay evidencia suficiente para atribuir todavía el efecto visual concreto | Pista de comfort separada | Pendiente, no bloquea el adapter/reconciliation inicial |

La captura exact-build fija también el gate usado por la locomoción directa.
`MoveForward` (`0x9CBC0`) y `MoveSideways` (`0x9CC60`) consultan primero el
estado indexado por `index +2BC / vector +2C4` (slots `+4C/+50`), después el
move-state indexado por `index +2D0 / vector +2D8` (slots `+0C/+10`), y requieren
`+268 > 0` o `+26C != 0`. Solo después
comparan el `amount` con `0.0`, llaman `iCharacterBody::Move` y finalmente
escriben `C6 86 64 02 00 00 01` (`cPlayer+0x264 = 1`).

PID 17612 demostró por qué `+0x264` no puede usarse como oracle de permiso una
vez que la locomoción VR se separa de los ejes nativos: el stick llegaba con
deflexión completa al runtime, pero el bridge entregaba `amount=0` a los métodos
nativos; éstos salían por su branch de cero antes de escribir el byte y nunca se
publicaba `locomotion_requested`. La adaptación corregida evalúa únicamente el
predicate anterior al `amount == 0`, conserva prioridad para cualquier eje
nativo real, publica el desplazamiento métrico por el owner `0xD7281` y refleja
`+0x264` después de una publicación directa correcta. El verificador exact-build
ancla esas instrucciones para que esta equivalencia no se convierta en una
suposición HPL genérica.

PID 28996 aportó la siguiente corrección de layout. El stick volvió a llegar al
runtime con valores cercanos a deflexión completa, pero `locomotion_requested`
continuó siempre a cero. Al desensamblar las dos funciones completas se comprobó
que la primera implementación del predicate había perdido el `0x200` alto de
los cuatro campos y además había intercambiado índice/vector al resolver los
estados. El bridge usa ahora exactamente `vector +2C4 / index +2BC` y
`vector +2D8 / index +2D0`; el verificador fija también esas cargas iniciales.

PID 8092 validó esa corrección con visor. El usuario confirmó movimiento correcto
por stick y una mejora clara de la sensación general. El helper registró
`direct_locomotion=True`, `queued/consumed/injected/matched=201/201/201/201`,
49 muestras stationary, 234 free, 2 blocked y 1 slide/partial, además de mirror
activo y la secuencia nativa `1.65 -> 0.95 -> 1.65 m` con recuperación. Esa
secuencia correspondía al crouch nativo por botón y no validaba altura física.

PID 20520 ejercitó la primera política por altura. La entrada física funcionó,
pero una salida de política no garantizaba el retorno inmediato a `1.65 m`; otro
gesto podía invertir la forma más tarde. El helper aprobó erróneamente al sumar
seis entradas/salidas y detectar una secuencia global sin correlación temporal.
La causa frente a Rework fue alimentar held/released en el toggle configurable
de BP en vez de conservar un único estado deseado. La política compartida posee
ahora el latch de botón y el OR con altura física. PID 23260 demostró una segunda
diferencia: `9CFA0/9CFD0` son dispatches pressed/released y release no hace stand
en modo toggle. PID 24948 demostró después que esos callbacks tampoco son el
owner adecuado para mantener una postura deseada: el log llegó a
`native_entries=14/native_exits=14` y el usuario vio solo una bajada/subida sin
crouch/sigilo persistente. El exact-build demuestra la frontera equivalente a
Rework: `0x9C750` cambia `cPlayer+0x2D0`; los handlers nativos llaman a esa
función con `4` para crouch y `0` para walk. `NativeInputBridge`, desde su hilo de
juego ya existente, aplica ahora directamente esos estados y sigue adoptando los
flancos legacy en la política compartida sin añadir otro hook.
`render_world_probe` usa `VrTrackingSpace` para Y continuo desde el
ancla de pies; solo el crouch no físico añade `-PhysicalCrouchDepth`. El helper
exige ahora política, move-state y shape correlacionados, button-only estable,
Hybrid, final de pie y rango Y de al menos `0.15 m`. Estado de esta corrección:
**host-tested**, pendiente de visor.
El pullback X/Z descrito en PID 20520 sigue abierto como gate subjetivo.

PID 29672 cerró el burst de salto: `+204=0.3` es umbral para la lógica de hold,
no máximo de `+200`; el contador alcanzó 2.233332 y al soltar volvió a 0.3.
`+268` es el contador de gracia de suelo 25→0→25; `+26C` permanece sin nombre
semántico. Los valores verticales derivados describen cinemática efectiva del
estado y no se reinterpretan como constantes de gravedad.

La primera instalación live de la sonda de ownership (PID 25776) falló cerrada
y no produjo telemetría accionable: `52CD` se había registrado erróneamente
como CALL, pero la captura exact-build demuestra `6A 01`; el CALL real es
`52CF -> 9A890`. Los otros siete pares son `52A5->9CEA0`,
`52F7->9CF40`, `531F->9CF70`, `5347->9CFA0`, `538A->9CFD0`,
`D790C->D5F00` y `D7913->D6120`. La corrección validó los ocho CALLs y firmas
de targets antes de parchear, refrescando identidades en cada callback. PID
24780 confirmó la instrumentación corregida y permitió cerrar sprint/crouch y
la no-utilidad de `D790C/D7913` para el player con gravedad activa.

Esto descarta de forma definitiva un adapter que llame directamente `D6E00`:
el tick nativo `D460A` lo invocaría de nuevo y consumiría otra vez deacc,
step/gravedad y estados. La operación segura ya está implementada: publicar
intent a través del owner de `MoveForward/MoveSideways`, dejar que el tick
nativo se ejecute una vez y observar posición/desplazamiento aceptado después.
No se debe reabrir esta frontera salvo evidencia contradictoria.

### Primer `BlackPlagueBodyAdapter` (live-tested; scope espacial aún desactivado)

La primera integración conserva ese límite. `black_plague_body_adapter.*`
exige que los owners de los dos `E8` de `cPlayer::MoveForward/MoveSideways` y
de `D460A -> D6E00` hayan validado la imagen exacta antes de habilitarse. Los
hooks de input ya existentes envían al adapter el mismo `amount, dt` que antes
pasaban directamente a cada método nativo; éste resuelve `cPlayer+0x274` en
cada llamada y reenvía una sola vez el método original. No usa offsets de
velocidad ni sustituye límites 3.0/4.5 m/s.

El hook existente en `D460A` sigue siendo el único wrapper de `D6E00`: llama al
original, lee body/feet después y entrega antes/después al adapter. El adapter
forma `runtime::VrAcceptedBodyMotion`, cuyo único resultado común es el
desplazamiento aceptado. Por diseño no puede llamar `D6E00`, por lo que la
garantía es una llamada nativa por tick. Las cadenas player/body se leen de
nuevo en publish y observe; una transición que reemplace el objeto deja de usar
el anterior sin cachearlo. Jump, gravedad/vertical y crouch siguen en sus
dispatches y tick nativos. HMD translation continúa a cero.

La primera instalación live falló antes de movimiento, no por una imagen
desconocida. `InstallNativeInputBridge` había validado los bytes vírgenes
`51CD -> 9CBC0` y `5227 -> 9CC60`, y luego los sustituyó correctamente por sus
wrappers `HookedForward/HookedSideways`. El adapter se instalaba después pero
requería por error otra vez los `E8` vírgenes. `D460A` todavía estaba virgen
porque body adapter se instalaba antes de `body_collision_probe`.

El orden ahora es input bridge (owner de 51CD/5227) → body/collision probe
(owner de D460A) → adapter (fan-out) → ownership/spatial probes. El adapter no
valida ni parchea un callsite ya poseído: exige que cada owner haya validado la
imagen exacta antes del parche y que su replacement siga presente. Si falla,
el error enumera concern, RVA, bytes pristine esperados/live y targets rel32
decodificados, además del estado del owner. Así no se relaja el gate exact-build
ni se permite un segundo hook.

La captura válida de PID 8628 confirmó `NativeInputBridge`,
`BodyCollisionProbe` y `BlackPlagueBodyAdapter` instalados a la vez, sin un
segundo owner. Hubo movimiento libre (`requested == solver == accepted`),
bloqueo total (`[-0.00135,0,+0.04998] -> [0,0,0]`) y sliding/aceptación parcial
(`[+0.02499,0,+0.00067] -> [+0.02499,0,0]`). La telemetría se mantuvo en
`dt=0.016667` y aproximadamente 60 updates/s: **native body update remains
exactly once per native physics tick**. `character_body` cambió de `1E841A98`
a `1A4BE610` y el fan-out siguió funcionando, evidencia live de resolución
dinámica sin cachear el body previo; la causa concreta del cambio no está
demostrada. `positional_translation_enabled=0` durante toda la sesión: no se
han validado room-scale, traslación posicional HMD, reconciliación activa,
velocidades VR, crouch físico, jump VR ni comfort/bob.

### Frontera exacta de contacto de mano (host-tested; sin conexión gameplay)

La captura inicializada soportada fija ahora la ABI que antes estaba abierta.
El slot `cPhysicsWorldNewton` `0x291BB0` apunta a `CreateBoxShape` en
`0x18AC10`; es `thiscall`, recibe `size` y matriz opcional y retorna con
`ret 8`. El constructor Newton en `0x19E5A0` deja el contador de usuarios en
`shape+0x54`, el mundo en `+0x58` y la vtable `0x692D40`. La retirada exacta
`iPhysicsWorld::DestroyShape` está en `0xD4210`: decrementa `+0x54` y, al llegar
a cero, quita el shape de la lista del mundo mediante `0xF9920`, que invoca su
destructor escalar. El destructor de shapes compuestos en `0x19E850` usa esa
misma retirada para sus hijos.

`CheckShapeWorldCollision` queda fijado en `0xD4830`, con nueve argumentos de
pila (`ret 0x24`). En `0xD49E3` comprueba el callback y llama a su slot virtual
cero con `(physics_body, cCollideData*)`. El `cCollideData` de esta build usa el
layout VC7: primer punto `+0x04`, último `+0x08`, contador `+0x10`; cada punto
mide `0x1C`, con normal en `+0x0C` y profundidad en `+0x18`. Los accessors
nativos de matriz local y shape del body están en `0xC9D90` (`body+0x34`) y
`0xCCC30` (`body+0x340`). El verificador fija todos esos bytes y retornos.

`hand_contact_probe.*` mantiene el diagnóstico default-off de query. Una petición
remota `--validate-palm-query` se atiende después del único `D460A -> D6E00`,
en el owner ya existente. Reutiliza el shape actual del cuerpo del jugador,
escribe el resultado en pila/memoria de la DLL y compara antes/después la lista
de shapes, posición del character body, matriz/puntero del physics body y
cabecera del shape. El harness sintético prueba espacio libre, callback con dos
contactos y rechazo de una mutación nativa. PID 28412 cerró este gate en proceso
real con un callback, ocho contactos y `native_memory_changed=false`.

El backend crea, reutiliza y destruye un box de palma propio mediante la ABI
exacta fijada y alimenta sus contactos al resolver común portado de Rework. Esa
lifecycle/resolution está live-tested en PID 8644 detrás de
`--validate-palm-resolver`: un create, reuse, seis queries y un destroy
equilibrados, `user_count=0` y sin cambios en la memoria gameplay seleccionada.
La integración posterior ya publica el body sujeto por cada mano en `skip_body`
y entrega el grip resuelto a manos visibles, objetos ya poseídos y herramientas;
el aim continúa usando tracking raw. La adquisición sigue ahora la separación de
Rework `23c890f`: parte de la palma resuelta pero puede seguir el controlador raw
hasta `0,18 m` para no perder asas/props cuando la mano visible queda detenida por
colisión. Esa conexión gameplay es host-tested.
El picking VR recorre ahora todos los hits de cada rayo antes de rankear la
selección ampliada; el callback proxy no corta el `CastRay` en el primer cuerpo.
La mano que originó el press también se conserva durante la transición native
Enter -> publicación del estado Grab/Move, aceptando el botón todavía held en el
siguiente service point. El nudge directo ignora esa mano mientras interact está
pulsado o la adquisición sigue pendiente.

La misma selección publica un target físico ordinario al resolver de palma por
una frontera de proveedor. Igual que Rework `23c890f`, la tolerancia de contacto
solo pasa de `0,002` a `0,008 m` cuando el target está a <= `0,40 m` y el raw
controller se mueve hacia él. La decisión se calcula después de elegir el start
real del resolver, incluidos recovery/reanchor. Los contadores
`interaction_assist`, `tracking_reanchors`, `recovery_anchors` y
`pullback_recoveries` permiten correlacionar el "snap" antes de tocar los
umbrales compartidos.
PID 23000 llegó a ejercitarla con visor, pero la misma sesión tuvo FPS muy bajos
y pérdida del controlador derecho, así que no sirve para promover contacto,
rendimiento ni sensación de agarre a headset-validated.

PID 4720 tampoco demuestra una regresión de room-scale: el log arrancó con
`physical_displacement_validation=0`, `room_scale_validation=0` y
`positional_translation_enabled=0`, mientras la colisión de palma sí se activó.
Era una composición incorrecta del helper de prueba. El helper de palms activa
ahora también los mutex de desplazamiento físico/room-scale y exige telemetría
de room-scale aplicado y crouch con tracking válido.

### Articulación de dedos: BP no debe degradarse

La comparación confirma dos capas distintas. BP ya usa la política neutral
`runtime::ArticulateVrHand`: cinco curls independientes, tres curvas por dedo,
distal progresiva, separación lateral y oposición del pulgar, con respuesta
instantánea. Es la conducta que el usuario identifica actualmente como mejor y
debe conservarse como candidata común.

Overture contiene integración valiosa pero ligada a su rig: índices/ejes y
bind poses de huesos, deadzone medida para su dispositivo, suavizado de 70 ms,
poses forzadas según radio del asa y callback posterior a animación. El límite
futuro pequeño es mantener `VrHandArticulation`/curls independientes en runtime
y dejar en un perfil/adapter de malla los ejes, deadzone/suavizado opcionales y
poses de agarre. La articulación normal de BP conserva sus cinco canales ricos;
únicamente la presentación mientras sostiene una herramienta consume ahora la
pose de agarre authored de Rework, porque esa pose pertenece al rig y no
reemplaza los curls libres de BP.

La geometría de Rework disponible en
`products/overture/data/models/hud_objects/hud_object_hand_rig.dae` y
`hud_object_hand_left_rig.dae` ya alimenta un consumidor real de malla/rig del
renderer. `tools/generate-rework-hand-mesh.py` valida la piel rígida de 3053
posiciones, 6034 triángulos, 17 joints, jerarquía/inverse binds y UVs, e incrusta
también una versión reducida del material difuso existente. En ejecución no se
parsea COLLADA ni JPEG. `DrawTrackedHands` conserva la palma resuelta como pose
mundial y aplica los curls/curvas/spread/oposición de pulgar de
`runtime::ArticulateVrHand` sobre los ejes de rig demostrados por Rework. La ruta
gráfica y la pose forzada de herramienta son host-tested; escala/orientación,
articulación visual de los cinco dedos y rendimiento aún necesitan la prueba de
visor combinada.

## Rutas integradas

`spatial_interaction.cpp` se compila y se instala después del puente de entrada.
Comprueba todos los slots/entradas usados antes de instalar ocho sustituciones de
slots/punteros para picking/Grab/Move/PlayerHands y una llamada rel32 de
herramientas.
Un fallo deja el comportamiento nativo y se registra; no invalida el estéreo.

| Límite | RVA | Evidencia y uso |
|---|---|---|
| PhysicsWorldNewton CastRay | slot 291BE8 → 189E30 | Solo se redirige la llamada AD84F, retorno AD852, del picking normal |
| Grab Update | slot 27D0D4 → ABA90 | Solo cuerpos adquiridos por un edge VR entran en seguimiento de palma |
| Grab Enter / Leave | slots 27D12C / 27D130 → AC900 / AA4C0 | Se ejecutan siempre las transiciones originales |
| Grab StopInteract | slot 27D0E4 → A9FD0 | Cambio de estado original para soltar desde el tick de juego |
| Move Update | slot 27CF74 → AA690 | Cuerpos libres poseídos por VR siguen el contacto elegido con fuerza derivada de Rework; mecanismos permanecen nativos |
| Move Enter / Leave | slots 27CFCC / 27CFD0 → AAC80 / AAED0 | La transición nativa se conserva y la adquisición VR solo ocurre para cuerpo libre elegible |
| Move StopInteract | slot 27CF84 → AA030 | La salida sigue usando el cambio de estado nativo |
| Normal Update | slot 27D13C → AD6C0 | Refresca picking antes del press; no integra tiempo |
| Entity SetMatrix | CA120 | Copia matriz local y notifica transformación; callbacks actualizan Newton y escena |
| Body GetJointNum | CCF00 | Se excluyen cuerpos con joints |
| Body velocities | 19C2A0 / 19C2C0 | Velocidad lineal / angular, slots +34 / +3C de vtable 292C08 |
| Body maximum velocities | 19C360 / 19C380 | Slots +54 / +5C; campos 42C / 430 restaurados al soltar |
| Body gravity | 19C590 | Slot +BC; la transición nativa conserva/restaura el estado previo |
| Body CollideCharacter | campo +3C8 | Constructor CD877; mundo D4952; rayo D4E0E; contactos Newton 19D2D0/19D2E4 |

Player: estado +2BC (Normal=0, Move=2, Grab=6), vector de estados +2C4.
Grab state: player +10, contacto +14/+18/+1C, cuerpo +20, pick-at-point +E1.
Las escrituras del contacto local están en ACBF0/ACBF6/ACBFF, después de invertir
la matriz del cuerpo y transformar el punto seleccionado.
Move state: player +10, contacto local +38/+3C/+40 y cuerpo seleccionado +54.
Las escrituras del contacto están en AAE94/AAE9A/AAEA0 y AA6C5 transforma ese
punto durante Update; el verificador fija esta ruta separadamente de Grab.
Body: vtable 292C08, matriz local +34, padre nodo +10, padre entidad +330,
masa Newton +434. Se rechazan padres, joints y masa no positiva/no finita.

### Item e interacción articulada: mapa exact-build offline

La clasificación real de inventario ya no depende de strings de icono/interacción.
La factoría `Item` construye `cGameItem` mediante `3554E -> 35040`, y el
constructor escribe **tipo de entidad `5` en `+0xC0`**. El valor `6` observado en
otros consumidores pertenece a otra clase y no debe reutilizarse como clasificador
de item. El loader convierte `ItemType` en `34AE0` y guarda el enum en
`cGameItem +0x250`. La build reconoce `normal=0`, `notebook=1`, `note=2`,
`battery=3`, `flashlight=4`, `food=5`, `map=6`, `glowstick=7`, `flare=8`,
`painkillers=9`, `weaponmelee=10`, `throw=11`, `gasmask=12` y `collectable=13`.
Los dos últimos no tienen equivalente demostrado en la política de Rework y se
mantienen fuera hasta clasificar su semántica.

`cGameItem::IsInView` está identificado en `35140`: replica la ruta conocida de
Overture con distancia, cono frontal de 43 grados, `SkipRayCheck` en `+0x27C` y
un callback propio en `+0x280` antes del `CastRay` virtual `+0x68`. Esta función
documenta el LOS nativo de item, pero no sustituye la adquisición magnética de
Rework.

La enumeración amplia ya está mapeada e integrada **host-tested** dentro del
owner existente de picking (`HookedRay`), sin instalar otro hook. `iPhysicsWorld
+0x14` contiene el sentinel de la lista de cuerpos y cada nodo publica el body en
`+0x08`; cada candidato exige vtable exacta, `Active +0x31`, `Collide +0x418`,
`!Character +0x3C7`, `!Player +0x3C9`, `UserData +0x414`, entidad activa en
`+0x14`, tipo `5` y un subtype Rework-compatible en `cGameItem +0x250`. El
`cBoundingVolume` embebido en `body +0xB4` usa los getters exactos
`D8A10/D8A40/D8A70/D8AF0` para max/min/world-centre/radius.

Si el contacto físico de palma no produce ganador, Black Plague consume
`vr_magnetic_pickup_policy`: clasifica 0-11 como las mismas familias demostradas
por Rework, mantiene 12/13 fuera, rankea como máximo cinco candidatos por cono y
score y ejecuta sight sólido tanto desde aim-controller como desde HMD, primero
contra el sample AABB y después contra el centro. El ganador se publica por el
mismo callback nativo de selección; no entra en `interaction_assist`. El
verificador exact-build fija toda la ABI consumida y CTest cubre el clasificador.
La ruta sigue pendiente de evidencia de visor y por tanto no supera
**host-tested**.

También queda fijado un mecanismo nativo representativo. La factoría `Lever`
reserva `0x324` bytes y llama `3D70C -> 3D500`; `cGameLever` instala vtable
`0x676608`, escribe tipo de entidad `0x12` en `+0xC0` y expone `Update=3C2B0`.
El base `iGameEntity` mantiene `mvBodies` en el vector `+0x14C` y `mvJoints` en
el siguiente vector `+0x15C`; `Lever::Update` exige al menos un joint, toma el
primero, consulta su valor escalar por el slot virtual `+0x38` y lo compara con
`MinLimit/MaxLimit` en `+0x284/+0x288`, actualizando el estado `0/1/2` en
`+0x280`. El loader convierte `MovementType` (`none/min/max/value`) y lo guarda
en `+0x31C`; `MovementValue` convertido a radianes queda en `+0x320`.

El mapa ya alcanza la frontera de consumo para una palanca de un solo joint. El
verificador fija las vtables concretas Newton de hinge/slider, sus métodos de
tipo, pin en `joint+0xB8`, pivot en `joint+0xC4` y los setters exactos de
velocidad lineal/angular del body. El análisis de `Move::Enter`/`Move::Leave`
confirma además que HPL pausa/reanuda allí los controladores del joint y conserva
scripts, gravedad y transiciones nativas. Por ello el adapter no reemplaza ese
ciclo: solo durante `Move::Update`, y únicamente para un `cGameLever` reconocido
con exactamente un joint soportado, consume `vr_mechanism_policy`. Hinge usa la
misma lightness por masa demostrada por Rework; slider usa el servo lineal
compartido. Tipos desconocidos y mecanismos multijoint hacen fallback al path
nativo. La prueba sintética cubre adquisición, update, restauración de límites de
velocidad y fallback. La integración sigue **host-tested** hasta comprobar una
palanca articulada real con visor.

Tras el incidente de la barra, la adquisición cinemática vuelve a estar habilitada
solo después de validar el filtro nativo `CollideCharacter`. Al adquirir, se guarda
el byte +3C8, se pone a falso durante el seguimiento y se restaura su valor exacto
al salir del estado Grab. Así, el cuerpo sujeto queda fuera de las consultas de
personaje y de ambas orientaciones del callback de contacto Newton; evita que la
teleportación controlada por la palma impulse al cuerpo del jugador. La prueba
sintética cubre restauración tanto de `true` como de un `false` definido por mapa.

Black Plague no contiene el `CollidePlayer` añadido posteriormente en el Rework:
el filtro disponible afecta a cualquier character, no solo al jugador. Es una
protección conservadora durante el agarre y todavía necesita prueba limpia en el
motor real. `Grab=6` mantiene el seguimiento rígido de cuerpos libres con
SetMatrix nativo. La observación de que numerosos props quedaban lejos de la
mano mientras algunas tablas largas se comportaban mejor llevó a separar
`Move=2`: los cuerpos libres de esa ruta conservan el punto de contacto realmente
seleccionado y lo llevan hacia la palma mediante la fuerza física derivada de
`cPlayerState_Move_VR` de Rework. Un `cGameLever` de un solo joint reconocido
usa ahora el adapter de hinge/slider compartido durante Update; puertas,
multijoint y mecanismos no reconocidos conservan su mecánica nativa. La colisión de palmas ya está
conectada host-side al grip resuelto. La selección usa la extensión raw limitada
de Rework y la adquisición revalida contra esa misma pose; el hold continúa en
la palma resuelta. PID 23000 fue inconcluso para visor por FPS/controlador y PID
4720 no tenía room-scale activo por el helper antiguo. El rayo secundario de
examinar durante Grab sigue pendiente.

La liberación ya no usa una única lectura instantánea del mando. Conserva las
cinco últimas muestras finitas y aplica la mediana por componente; con menos de
dos muestras no transfiere momento. Esto rechaza picos aislados sin eliminar un
lanzamiento deliberado y sostenido. La velocidad lineal queda limitada a 9 m/s
tanto durante el seguimiento como al soltar, y la angular a 6 rad/s. Un salto de
palma superior a 0,35 m entre ticks cancela el agarre sin momento, igual que UI,
pérdida de foco o tracking. Los contadores `grabs_acquired`, `grabs_released`,
`guarded_releases` y `collision_restore_failures` quedan en el log periódico.

El adaptador tiene un contador de callbacks activos y se niega a retirar hooks
mientras queda un cuerpo propio. Solo el hilo de actualización del juego puede
ejecutar la liberación. No se invocan métodos de física desde el hilo remoto.
La DLL permanece residente al desactivar, como en el resto del framework.

## Herramientas HUD: integración experimental activada

Constructor `cPlayerHands` A4A30: usa el nombre heredado `FadeHandler`, escribe
vtable 27CB5C, guarda init +20, modelos actuales +6C/+70 y número de slots +74=2.
La vtable +14 (27CB70) apunta a Update A3DE0. Dentro de ese Update, la llamada
**A4313 → CA120** aplica la matriz al entity +118 del modelo HUD. El bucle procesa
ambos slots y conserva animaciones/equipado. Esta es una frontera candidata para
anclar herramientas sin sustituir carga de recursos ni batería/lógica de luces.
Se instala un hook de Update para delimitar el PlayerHands actual y otro de
A4313 para sustituir exclusivamente la matriz de Flashlight/Glowstick. Se
identifica el modelo por su entity +118 y nombre +4 mediante el operador nativo
de MSVCP71 (IAT 272138), sin interpretar ni construir/destruir std::string viejo.
La dirección importada se contrasta con GetProcAddress antes de instalar.

La herramienta sigue la mano izquierda del perfil diestro. La linterna conserva
su socket medido de BP `(0,-0.016669,0)` y la rotación que alinea su `-Y` nativo
con el `-Z` del mando. El glowstick ya no usa el antiguo socket provisional que
lo dejaba dentro de la palma. Aunque los DAE completos difieren, la lista de
posiciones de su cilindro principal de agarre es numéricamente idéntica a la de
Rework; por tanto se reutiliza exactamente el perfil demostrado para esa
geometría: `VrScale=1.55`, `VrGripPoint=(0,0.0078,-0.078)` y rotación X `4.71`,
compuesto sobre la pose authored de dedos largos. SetMatrix nativo conserva la
propagación hacia las luces. Modelos desconocidos, UI o tracking inválido
mantienen su matriz nativa. No hay colisión de herramienta con paredes todavía.
La nueva colocación sigue siendo host-tested hasta verla en visor.

`cPlayerFlashLight::Update`, zona A8A60–A8F2D, obtiene el modelo Flashlight y usa
su matriz de mundo para la dirección de luz hacia enemigos. La llamada A8B23
obtiene la matriz; el vector local usado después es (0,-1,0).

Los DAE instalados NO son idénticos a Rework:

| Recurso | Black Plague SHA256 | Rework SHA256 |
|---|---|---|
| hud_object_flashlight.dae | 562758F8E6D281448EFB222D7C338650D5BF4BC9723ADB758DA417B035834A06 | BCD47760A310A91A4FC0BD121534F2A466540A8F45C065D7D5F0F3B413C38236 |
| hud_object_glowstick.dae | F4B816FF02426B2D2B16632792E2127BE804D9634207780E873ACE7B80F39E51 | B8BECAFE57E591DF8AA4B24B62EA45870ACAF5D677D01B3E982573B94EB0A716 |

Hay diferencias en normales y posiciones/escalas de luces. Por ejemplo, el spot
de linterna instalado está en (0,-0.103966,0), frente a (0,-0.092,0) en Rework.
La diferencia de hash impide copiar perfiles enteros por nombre o sustituir los
DAE. La excepción demostrada es el cilindro principal del glowstick descrito
arriba, cuya geometría coincide y permite transferir su escala/grip/rotación.
Flashlight y cualquier otro nodo/luz permanecen BP-specific y requieren su propia
evidencia y validación física.

## Verificación y límites

La configuración raíz contiene ahora 38 tests. Los gates Release y SDK-less
actuales pasan 38/38; esos resultados host no promueven por sí solos ninguna
capacidad a evidencia live/headset.
El test `opengl_eye_targets` se ejecuta también en la configuración SDK-less
actual usando el driver WGL real. El test
corporal ejecuta la sonda exact-build sobre una imagen sintética y el test
espacial ejecuta el código del adaptador en una imagen sintética con trampolines
a dobles nativos; no prueba Newton ni el juego real. La prueba matemática de
agarre inyecta un pico extremo en una ventana estable y comprueba que la
estimación conserva la mediana; una sola muestra produce cero. El verificador
PowerShell local contrasta la captura inicializada sin modificar procesos.

El boundary corporal/adapter, la reconciliación shadow y la petición física X/Z
collision-aware en `0xD7281` ya están live-tested, pero los hitos amplios de
jugabilidad todavía no están certificados. PID 13672 confirmó que la corrección
de presentación eliminó el temblor continuo; PID 8092 validó técnicamente la
ruta stick/collision. PID 20520 dejó abiertos el pullback de movimientos físicos
cortos y la salida de crouch. La transacción por tick y la política/correlación
de crouch que corrigen esas fronteras son host-tested y necesitan visor.

La ABI/contactos/lifetime base de shapes BP ya está demostrada de forma estática.
PID 28412 live-tested el diagnóstico sin escrituras y PID 8644 live-tested el
shape de palma propio más el resolver Rework aislado. El binario inicializado
fija ahora además dos filtros independientes de `D4830`: con
`collideCharacter=false` se descartan los cuerpos de personaje y `skip_body`
descarta exactamente el cuerpo suministrado. El harness host verifica que el
resolver usa ese contrato. La integración gameplay ya publica por mano el cuerpo
sostenido, entrega la palma resuelta a manos/objetos/herramientas y suministra al
resolver el target físico seleccionado para la asistencia limitada de Rework.
Esa integración, la nueva adquisición y la colocación corregida del glowstick
recibieron por fin evidencia parcial de visor el 2026-09-19: room-scale abierto,
mirror, colocación/feedback del glowstick y empuje directo de props fueron
positivos; la palma llegó a bloquear/deslizar parcialmente. La misma prueba
expuso cuatro fallos distintos que no se deben confundir con tracking global:
el nudge expulsaba el cuerpo seleccionado antes de poder adquirirlo; cambios de
`world_yaw` podían dejar una pose resuelta de la época anterior y hacer que el
resolver interpretase el salto de coordenadas como discontinuidad; la malla
importada de Rework mostraba peor articulación de pulgar/meñique que la mano
provisional previa de BP pese a que esta última respondía bien a los cinco
canales esqueléticos del mando, por lo que no debe sustituirse esa semántica por
el fallback agrupado de Rework; y los props dinámicos bloqueaban al
personaje porque `cCharacterBodyCollidePush::OnCollision` (`0xD5A60`, vtable
`0x67F7AC`) solo considera movimiento si `CharacterBody+0x70/+0x74` no son cero,
mientras la inyección VR BP llega después de ese cálculo. El mismo callback usa
`+0x98` como masa máxima empujable y `+0x9C` como fuerza.

La corrección de dedos restaura además la articulación libre anterior a
`868482c`: oposición explícita del pulgar, apertura lateral espejada y flexión
progresiva independiente de los cuatro dedos largos. La pose dedicada al
agarre de herramientas permanece separada. Esto está host-tested; la malla
importada todavía necesita validación en visor con los PS VR2 Sense.

La candidata actual corrige esas diferencias sin cambiar room-scale, mirror ni
el socket del glowstick: el cuerpo ganador de selección queda excluido del nudge
solo durante su ventana fresca de adquisición; un cambio de época de yaw invalida
de inmediato la pose resuelta antigua y resetea el historial del resolver antes
de la siguiente muestra; el bridge conserva los cinco canales esqueléticos
independientes de BP y recupera las amplitudes de la mano provisional que ya
habían dado mejor respuesta en visor; y el primer solver horizontal presenta
temporalmente al callback nativo una señal
de movimiento junto con `0.2x` de su fuerza de empuje, igual que el branch
`vr_velocity` de Rework, restaurando `+0x70/+0x74/+0x9C` al retornar. Build Release,
tests focales y suite completa 38/38 pasan. Todo este bloque nuevo sigue
`host-tested`; necesita la prueba dirigida de visor antes de cualquier promoción.
