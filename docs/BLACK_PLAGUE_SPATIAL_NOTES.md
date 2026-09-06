# Black Plague: integración espacial y punto de continuación

2026-09-06. Solo imagen inicializada FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF.
Base virtual 0x00400000. Los números siguientes son RVAs, nunca offsets del PE
protegido en disco. Captura local: `artifacts/black-plague-22000-live.bin`.
Rework de referencia: 23c890f7dbd06b939be9951d282e6e948d9a6623, sin modificar.

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

Player: estado +2BC (Normal=0, Grab=6), vector de estados +2C4.
Grab state: player +10, contacto +14/+18/+1C, cuerpo +20, pick-at-point +E1.
Las escrituras del contacto local están en ACBF0/ACBF6/ACBFF, después de invertir
la matriz del cuerpo y transformar el punto seleccionado.
Body: vtable 292C08, matriz local +34, padre nodo +10, padre entidad +330,
masa Newton +434. Se rechazan padres, joints y masa no positiva/no finita.

**Tras el incidente de la barra, la adquisición cinemática está bloqueada en
producción.** El seguimiento se conserva en tests sintéticos, pero necesita
filtros completos de colisión antes de volver a habilitarlo.

No se escribe un supuesto campo CollidePlayer: ese añadido de Rework no está
mapeado en Black Plague. La ausencia de esa exclusión y de colisión de palmas es
una limitación real del prototipo. Los cuerpos libres siguen la palma con
SetMatrix nativo; no se garantiza resolución de contactos ni ausencia de
penetración. Puertas, palancas y estados Push/Move conservan su mecánica nativa.
El rayo secundario de examinar durante Grab sigue pendiente.

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

21 tests pasan en Release, Debug y Release sin SDK OpenVR. El test espacial
ejecuta el código del adaptador en una imagen sintética con trampolines a dobles
nativos; no prueba Newton ni el juego real. El test OpenGL usa el driver WGL,
verifica píxeles de guantes y oclusión/restauración de estado en ambos ojos.
El verificador PowerShell contrasta la captura inicializada sin modificar procesos.

No se ha iniciado el juego/SteamVR ni solicitado una prueba física en esta tanda.
Los tres hitos completos de jugabilidad todavía no están certificados: herramientas,
colisión y mecanismos articulados siguen pendientes, además de la prueba con visor.
