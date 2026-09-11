# Black Plague: integración espacial y punto de continuación

2026-09-06. Solo imagen inicializada FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF.
Base virtual 0x00400000. Los números siguientes son RVAs, nunca offsets del PE
protegido en disco. Captura local: `artifacts/black-plague-22000-live.bin`.
Rework de referencia: 23c890f7dbd06b939be9951d282e6e948d9a6623, sin modificar.

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

`Move` no acepta todavía un desplazamiento: integra `amount * acceleration *
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
La shape del jugador concreto se clasificará en el log por su tamaño activo.
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

La siguiente prueba pendiente queda limitada al salto: tras el dispatch nativo
`5299 -> 9CEA0`, la misma sonda `D460A` captura 240 body updates para relacionar
take-off, impulso, hold, apex, caída y aterrizaje con los campos crudos ya
confirmados. Cámara/head-bob, footstep-bob, room-scale y multiplicadores quedan
fuera de esta medición.

### Comparación concreta con Overture

Las primitivas neutrales de tracking/locomoción en `src/runtime` y la
orquestación probada de `OvertureBackend` siguen siendo la candidata correcta:
escala 1 unidad/m, pasos físicos de 0,05 m, epsilon de rechazo 0,002 m, rebase
a 0,8 m, 1,5/2,25 m/s y reconciliación del head anchor. Su adapter de fuente puede escribir `vr_velocity`, activar
`vr_stepstaticonly`, llamar `iCharacterBody::Update(dt)` y leer la posición
resuelta. BP conserva el character body HPL base, pero no posee esos dos campos
añadidos por Rework ni los argumentos `CollidePlayer/IsPlayer` de su solver.

Por tanto, posición de cuerpo/pies, timestep y resultado aceptado encajan ya en
el contrato conceptual estrecho de una futura `BlackPlagueBodyAdapter`. La
política de Overture debe extraerse mecánicamente al runtime cuando el adapter
pueda devolver desplazamiento aceptado: no debe quedar una segunda política BP.
Aún faltan inyección, static-only, jump/crouch y propietarios de bob; conectar
la política ahora podría duplicar `iCharacterBody::Update`. No se debe emular
`vr_velocity` escribiendo offsets inexistentes ni llamar dos veces a `Update`
sin demostrar su secuencia.

### Ownership del movimiento plano: mapa estático pendiente de sonda live

| Concern | Black Plague owner/evidence | Relación con `D6E00` | Estado |
|---|---|---|---|
| Intento plano | `cButtonHandler::Update`: `51CD/5227 -> cPlayer::MoveForward/MoveSideways` (`9CBC0/9CC60`) | Antes | Mapeado |
| Aceleración/objetivo | `cPlayer` comprueba state/ground y llama `iCharacterBody::Move(D4F50)`; éste suma `amount * acc * dt` en `+70..+7C`, marca `+88/+89` y limita por `+60..+6C` | Consumido por `D6E00` | Mapeado |
| Deceleración y velocidad final | `D6E00` consume flags, aplica deacc y convierte los campos de velocidad en request horizontal antes de `D7312` | Dentro, antes de colisión | Mapeado |
| Sprint | queries `52EB/5313` llegan a wrappers `9CF40/9CF70`, que delegan en el move-state virtual (`+30/+34`) | Antes | Semántica/state concreta pendiente |
| Jump | `5299 -> 9CEA0` selecciona estado 3 (`cPlayerMoveState_Jump`); `52CF -> 9A890` escribe `+1FC` y acumula `+200` mientras se mantiene | La fuerza/estado vertical se publica antes de `D6E00`; éste resuelve horizontal primero y vertical después | Live-characterized: impulso efectivo inicial ~5.53 m/s, apex ~0.95 m, landing nativo y 3→0 |
| Crouch | queries `533B/536A` llegan a wrappers `9CFA0/9CFD0`, que delegan en move-state (`+38/+3C`) | Antes | Shape/state/transición pendientes |
| Colisión/step/gravedad | `D6E00`, primer solver `D7312`, fases posteriores de step/gravedad | Dentro | Live-tested para el límite |
| Cámara corporal | calls `D790C -> D5F00` y `D7913 -> D6120` después de resolución; por estructura corresponden a composición de cámara y entidad | Después | Requiere lectura live de cámara/offset |
| Head/footstep bob | No hay evidencia que permita atribuirlo aún a `iCharacterBody`, `cPlayer` o una animación visual | Posiblemente posterior | Pendiente |

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
`D790C->D5F00` y `D7913->D6120`. No solapan los hooks existentes: el bridge
de entrada posee las consultas vecinas (`5299`, `52C1`, `52EB`, etc.), no estos
despachos. La corrección valida los ocho CALLs y firmas de targets antes de
parchear y refresca punteros en cada callback; queda `implemented`, no
`live-tested`. El cambio observado de body/world coincidió con una transición
gameplay→menú→gameplay; es compatible con recreación o carga de nivel, pero su
causa no está demostrada.

Esto descarta un adapter que llame directamente `D6E00`: el tick nativo
`D460A` lo invocaría de nuevo y consumiría otra vez deacc, step/gravedad y/o
acumuladores. La operación futura segura debe publicar una petición antes de
ese único tick y leer su posición aceptada después; no llamar `Update` desde el
Framework. La sonda siguiente debe enganchar de forma reversible los cinco
callsites de acciones, `D790C/D7913` y el límite de render para registrar orden,
state, tamaño del body y transform final de cámara, sin cambiar argumentos ni
resultados.

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
poses de agarre. Overture debería adaptarse a esa salida común cuando exista
una segunda malla real; BP no debe copiar ahora la pose rígida de Overture.

## Rutas integradas

`spatial_interaction.cpp` se compila y se instala después del puente de entrada.
Comprueba todos los slots/entradas usados antes de instalar cinco hooks de
vtable y una llamada rel32 de herramientas.
Un fallo deja el comportamiento nativo y se registra; no invalida el estéreo.

| Límite | RVA | Evidencia y uso |
|---|---|---|
| PhysicsWorldNewton CastRay | slot 291BE8 → 189E30 | Solo se redirige la llamada AD84F, retorno AD852, del picking normal |
| Grab Update | slot 27D0D4 → ABA90 | Solo cuerpos adquiridos por un edge VR entran en seguimiento de palma |
| Grab Enter / Leave | slots 27D12C / 27D130 → AC900 / AA4C0 | Se ejecutan siempre las transiciones originales |
| Grab StopInteract | slot 27D0E4 → A9FD0 | Cambio de estado original para soltar desde el tick de juego |
| Normal Update | slot 27D13C → AD6C0 | Refresca picking antes del press; no integra tiempo |
| Entity SetMatrix | CA120 | Copia matriz local y notifica transformación; callbacks actualizan Newton y escena |
| Body GetJointNum | CCF00 | Se excluyen cuerpos con joints |
| Body velocities | 19C2A0 / 19C2C0 | Velocidad lineal / angular, slots +34 / +3C de vtable 292C08 |
| Body maximum velocities | 19C360 / 19C380 | Slots +54 / +5C; campos 42C / 430 restaurados al soltar |
| Body gravity | 19C590 | Slot +BC; la transición nativa conserva/restaura el estado previo |
| Body CollideCharacter | campo +3C8 | Constructor CD877; mundo D4952; rayo D4E0E; contactos Newton 19D2D0/19D2E4 |

Player: estado +2BC (Normal=0, Grab=6), vector de estados +2C4.
Grab state: player +10, contacto +14/+18/+1C, cuerpo +20, pick-at-point +E1.
Las escrituras del contacto local están en ACBF0/ACBF6/ACBFF, después de invertir
la matriz del cuerpo y transformar el punto seleccionado.
Body: vtable 292C08, matriz local +34, padre nodo +10, padre entidad +330,
masa Newton +434. Se rechazan padres, joints y masa no positiva/no finita.

Tras el incidente de la barra, la adquisición cinemática vuelve a estar habilitada
solo después de validar el filtro nativo `CollideCharacter`. Al adquirir, se guarda
el byte +3C8, se pone a falso durante el seguimiento y se restaura su valor exacto
al salir del estado Grab. Así, el cuerpo sujeto queda fuera de las consultas de
personaje y de ambas orientaciones del callback de contacto Newton; evita que la
teleportación controlada por la palma impulse al cuerpo del jugador. La prueba
sintética cubre restauración tanto de `true` como de un `false` definido por mapa.

Black Plague no contiene el `CollidePlayer` añadido posteriormente en el Rework:
el filtro disponible afecta a cualquier character, no solo al jugador. Es una
protección conservadora durante el agarre y todavía necesita prueba en el motor
real. Tampoco existe colisión de palmas. Los cuerpos libres siguen la palma con
SetMatrix nativo; puertas, palancas y estados Push/Move conservan su mecánica
nativa. El rayo secundario de examinar durante Grab sigue pendiente.

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

La herramienta sigue la mano izquierda del perfil diestro. Rotación X de +90
grados: el -Y nativo de la linterna queda hacia el -Z del mando. Los sockets
provisionales usan nodos de los DAE instalados: linterna (0,-0.016669,0), glowstick
(0,0.059722,0.00504). Requieren ajuste visual; no son calibraciones certificadas.
SetMatrix nativo conserva la propagación hacia las luces. Modelos desconocidos,
UI o tracking inválido mantienen su matriz nativa. No hay colisión de herramienta
con paredes todavía. Las pruebas cubren sockets, dirección y estos fallbacks.

Prueba física: el glowstick acompañó correctamente a la mano, pero quedó
literalmente dentro de ella. No se modifica aún el socket porque la geometría de
mano es provisional; se recalibrarán palma, herramienta y luz como una unidad
cuando se integren las mallas definitivas.

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
No aplicar automáticamente VrGripPoint/VrRotOffset de Rework ni copiar sus DAE
al juego. Primero identificar el modelo por una frontera ABI verificada,
calcular el agarre con sus nodos/escala y probar la transformación de la luz.

## Verificación y límites

Los 26 tests pasan en la configuración Release actual. El test corporal
ejecuta la sonda exact-build sobre una imagen sintética y el test espacial
ejecuta el código del adaptador en una imagen sintética con trampolines a dobles
nativos; no prueba Newton ni el juego real. El test OpenGL usa el driver WGL,
verifica píxeles de guantes y oclusión/restauración de estado en ambos ojos.
La prueba matemática de agarre inyecta un pico extremo en una ventana estable y
comprueba que la estimación conserva la mediana; una sola muestra produce cero.
El verificador PowerShell contrasta la captura inicializada sin modificar procesos.

No se ha iniciado el juego/SteamVR ni solicitado una prueba física en esta tanda.
Los tres hitos completos de jugabilidad todavía no están certificados: herramientas,
colisión y mecanismos articulados siguen pendientes, además de la prueba con visor.
