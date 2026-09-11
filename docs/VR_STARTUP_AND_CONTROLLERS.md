# Arranque VR y estado de los mandos

Actualizado: 2026-09-08. Backend **Black Plague FD316F…** únicamente.

La prueba de visor del 2026-09-06 confirmó el arranque mediante BAT, recentrado,
menús, inventario/libreta, dedos, movimiento, giro, correr/agacharse y luces
estables en la zona probada. También dejó problemas concretos que no deben
considerarse resueltos: mirror negro/con artefactos, rumbo inicialmente
desacoplado del HMD, interacción en la mano izquierda cuando no correspondía,
agarres nativos y glowstick sin anclaje espacial correcto.

Las correcciones posteriores se han incorporado en código y requieren una nueva
prueba de visor para certificarlas. El glowstick ya siguió la mano en la tanda
posterior, pero el socket provisional lo dejó dentro de la mano; el ajuste visual
se aplaza hasta integrar las manos definitivas para no calibrarlo dos veces.
Los cambios de dedos pasan las pruebas matemáticas, pero no se consideran una
mejora visual validada hasta una nueva prueba.

### Estado actual tras la prueba

- **Validado en visor:** BAT, recentrado, menús/inventario/libreta, dedos,
  locomoción básica, giro, correr/agacharse y estabilidad de luces en la zona
  probada.
- **Pendiente de nueva prueba:** mirror desde el ojo izquierdo, propiedad diestra
  y L2/R2, movimiento relativo al HMD, adquisición de agarre tras confirmar el
  estado nativo, ciclo L1 Rework y anclaje de herramientas.
- **Deliberadamente no terminado:** room-scale, colisión de palmas, puertas/
  palancas/joints, modelos HPL de Rework y Enhanced visuals GPU.

La tanda posterior añadió `native_update_timing`, contadores de herramientas y
agarres rechazados por seguridad, además de ampliar el buffer de log. No se
modifican velocidad de simulación, gravedad ni duración del salto para ocultar
un posible problema temporal.

El usuario reportó salida del mapa al caminar con una barra agarrada. La ruta se
bloqueó y solo se ha reactivado tras mapear el indicador nativo +3C8 que excluye
el cuerpo sujeto de consultas de personaje, rayos y contactos Newton. Se guarda
y restaura su valor original al soltar. La regresión cubre ambos valores iniciales,
pero la ruta debe seguir certificándose en el motor real: usar primero un objeto
pequeño, lentamente y sin lanzamiento ni barras largas.

El stick limita su vector a longitud 1 para evitar sobrevelocidad diagonal. El
perfil local aplica además `MoveSpeed=0.85` sobre la entrada analógica, sin cambiar
reloj, gravedad ni física. El contador `native_update_timing` sigue siendo la
evidencia para separar velocidad física de una anomalía temporal.

El launcher imprime la ruta efectiva de settings y el valor del mirror tanto en
preflight como al activarlo. Los ajustes Black Plague que el backend consume
también pueden editarse sin arrancar el juego con
`PenumbraVR.ProbeLauncher.exe --configure-vr black-plague`. El mirror continúa
pendiente de certificación física.

## Arrancar sin adjuntar el mod a mano

Desde la carpeta de instalación del framework, ejecuta `Start-Black-Plague-VR.cmd`.
Usa el ejecutable Release y la instalación Steam habitual. También admite la ruta
del ejecutable como primer argumento si Steam está en otra biblioteca.

El lanzador comprueba el hash admitido y los archivos VR, abre Black Plague
mediante Steam (appid 22120), espera la inicialización del ejecutable protegido,
adjunta el framework y activa la presentación continua con el mirror guardado.
No modifica el ejecutable del juego ni sus opciones de lanzamiento en Steam.
Abrir directamente el juego en Steam **no** ejecuta este lanzador automáticamente.
SteamVR debe estar instalado y los dispositivos configurados; OpenVR solicita
su inicialización. El usuario confirmó el arranque directo con el BAT.

### Estado de la validación shadow y próximas pruebas

La reconciliación shadow ya fue live-tested en PID 28172 mediante
`tools/Start-BlackPlagueShadowValidation.ps1`: el proceso confirmó
`body_reconciliation_shadow enabled=1 source=mutex`, mantuvo la traslación
posicional a cero y conservó el único tick nativo de cuerpo durante movimiento
libre, bloqueo/deslizamiento, recentrado y sustitución de cuerpo. No es necesario
repetir esa tanda salvo para investigar una regresión.

El helper se conserva como ruta reproducible de diagnóstico. Ejecuta primero el
verificador exact-build y mantiene el mutex temporal durante la sesión; el
fallback `PVR_BP_RECONCILIATION_SHADOW=1` sigue reservado a procesos que hereden
realmente esa variable.

La próxima tanda con visor se divide en dos grupos. La presentación puede
revalidarse ya: con mirror desactivado el monitor debe permanecer negro sin el
artefacto del punto blanco creciente; además hay que volver a comprobar
Alt+Tab/foco porque actualmente menú e inventario pueden quedar negros en el
visor al perder foco la ventana. La prueba de room-scale debe esperar a que el
backend implemente y demuestre una petición física X/Z collision-aware en metros;
hasta entonces la traslación posicional del HMD permanece desactivada.

Los logs se crean automáticamente en `%LOCALAPPDATA%\PenumbraVR\logs`, con un
`black-plague-probe-<PID>.log` para cada ejecución. La lista ordenada de pruebas
pendientes se mantiene en `docs/VR_HEADSET_TEST_CHECKLIST.md`.

Si ya está abierto el mismo ejecutable, reutiliza ese proceso. No elige procesos
por nombre solamente: Overture, Black Plague y Requiem deben distinguirse por
ruta y huella. Si hay dos instancias coincidentes, se detiene. En caso de error
no mata el juego ni la sesión de SteamVR. El detalle queda en la consola y en
`%LOCALAPPDATA%\PenumbraVR\logs`.

Comprobación sin abrir el juego ni SteamVR:

```powershell
& E:\penumbra_vr\build\bin\Release\PenumbraVR.ProbeLauncher.exe --check-vr 'C:\Program Files (x86)\Steam\steamapps\common\Penumbra Black Plague\redist\Penumbra.exe'
```

## Qué está implementado

1. Lector real de acciones OpenVR: gameplay/UI, poses grip/aim de ambas manos,
   curls de dedos y salida háptica limitada. Bindings de PSVR2 y demás perfiles
   se copian a `build/bin/<config>/vr` en cada compilación. No se sondea por ojo:
   lo hace una vez `cButtonHandler::Update`.

   El paquete compartido incluye ocho perfiles OpenVR: PS VR2 Sense, HTC Vive,
   Valve Index/Knuckles, Oculus/Meta Touch, Pico 4, Pico Neo 3 y las dos variantes
   Windows Mixed Reality (`microsoft/motion_controller` y
   `holographic_controller`). La presencia del JSON no implica paridad. Cada
   perfil debe validar las mismas acciones lógicas disponibles en Overture,
   handedness, poses grip/aim, navegación y apuntado de menús, haptics y, cuando
   el hardware lo exponga, articulación de dedos. Esta matriz debe cerrarse en
   Black Plague antes de declarar paridad de controles y reutilizarse después en
   Requiem con validación específica de su backend.
2. Puente nativo exacto: movimiento analógico combinado con teclado, giro por
   pasos configurable o suave integrado por tiempo, salto, correr, agacharse,
   interactuar, examinar, guardar objeto, inventario, libreta, pausa y ciclo de
   luz rápido apagado → glowstick → linterna → apagado, como Rework. El stick
   sigue el yaw horizontal del HMD; velocidad/deadzone, modo/ángulo de giro y
   mano dominante se leen del INI del framework. El teclado conserva sus ejes
   nativos. Tracking obsoleto inhibe movimiento VR. No equivale a cuerpo completo
   ni room-scale. No usa emulación de teclas de Windows. Los nombres de acción
   conservan su ABI antiguo y siempre se ejecuta la consulta original del juego.
3. Menú inicial, pausa/inventario/libreta: captura del escritorio presentada en
   ambos ojos como panel configurable (2,4 metros de ancho a 1,75 metros con el
   perfil actual), orientado según el yaw al abrirlo. Apuntado de la mano dominante
   en menú principal, inventario y libreta, coordenadas nativas 800×600,
   recentrado y prioridad temporal del ratón cuando se mueve. Los diálogos
   especiales conservan sus rutas nativas y aún necesitan cobertura en ejecución.
   Se restauran matrices, texturas, programas GL, viewport, scissor y demás estado
   gráfico tras dibujar el panel.
4. Adaptador espacial integrado en la DLL y compilación: selección desde aim
   dominante (derecho por defecto) en el estado normal, refresco antes de pulsar,
   agarre relativo a la palma de cuerpos libres y lanzamiento limitado a 9 m/s.
   Las transiciones originales conservan la gestión de masa/gravedad; el
   adaptador restaura los límites de velocidad que cambia. Un botón de la otra
   mano no transfiere el objeto ni oculta la liberación de la mano propietaria.
   Pérdida de tracking/foco, UI o salto de pose sueltan sin impulso de lanzamiento.
   Se emiten pulsos hápticos cortos al coger/soltar. Son rutas verificadas con
   dobles de las funciones nativas, no una simulación del motor Newton real.
   Mientras se sostiene el cuerpo, su `CollideCharacter` nativo se desactiva para
   impedir que empuje al jugador y se restaura exactamente al soltar. La instalación
   exige firmas de constructor, mundo, rayo y las dos ramas de contacto Newton.
   La velocidad de lanzamiento usa una mediana móvil de hasta cinco muestras:
   ignora picos aislados y una liberación sin historial suficiente no transfiere
   momento. Un salto de tracking mayor de 35 cm suelta de forma segura.
   Corregido el orden de Enter: el juego publica el estado Grab después de
   regresar de Enter. La adquisición espera al estado confirmado; antes podía
   quedar en el agarre nativo de escritorio. Regresión añadida con ese orden.
5. Guantes geométricos provisionales para ambas manos, dedos según curls OpenVR
   (o postura aproximada sin skeleton), rayo de apuntado y oclusión por profundidad
   en cada ojo. No son las mallas/esqueletos HPL de Rework y no proyectan sombras.
   Las manos se sitúan respecto al HMD actual para convivir con el tracking
   rotacional existente; esto no añade room-scale ni colisión de las palmas.
6. Mirror en partida: copia de la textura del ojo izquierdo al escritorio justo
   antes del swap, conservando proporción y bandas negras. No renderiza un tercer
   mundo nativo. Los menús conservan la captura/presentación nativa del escritorio.
   Prueba WGL de píxeles, orientación, bandas y restauración de viewport/scissor.
   La implementación existe, pero sigue siendo experimental hasta la nueva
   validación física.

La numeración anterior describe módulos implementados, **no certifica que los
hitos de gameplay estén terminados**.

## Lo que NO está terminado

- El tercer hito original sigue parcial: puertas/palancas y cuerpos con joints
  o padres conservan el comportamiento nativo. Falta la colisión de palmas,
  validar físicamente la exclusión conservadora cuerpo/character, las mallas HPL
  de Rework y calibrar físicamente las herramientas/luces ancladas. Durante un
  agarre espacial todavía no se actualiza el rayo secundario de examinar como hace Rework.
- `PlayerState_Interact_VR.cpp` es la referencia, pero no se copian offsets de
  Overture. Véase `BLACK_PLAGUE_SPATIAL_NOTES.md` para llamadas verificadas,
  pruebas, límites y el siguiente punto de integración de herramientas.
- El perfil predeterminado sigue siendo diestro y de giro por pasos, pero la mano,
  deadzones, escala de movimiento, giro, escala de render y geometría UI ya se
  persisten/aplican desde `%LOCALAPPDATA%\PenumbraVR\settings.ini`. Se desactiva
  el action set offhand para evitar propiedad mezclada. En el perfil PSVR2 diestro
  R2 es interactuar; el perfil zurdo usa L2 y cambia también puntero y mano de
  herramientas.
- Tracking corporal posicional y Enhanced visuals GPU no están conectados.
- Falta validar transiciones mientras se mantiene un botón, la pérdida de
  tracking/foco en una partida real y la convivencia de mandos y teclado al
  mantener ambos la misma acción. Las pruebas puras no sustituyen esa prueba.
- También falta cerrar la matriz de paridad por perfil de mando. PS VR2 Sense es
  el perfil usado en la validación live actual; los demás bindings compartidos
  todavía requieren validación funcional equivalente antes de considerarse al
  mismo nivel que Overture.
- La iluminación pasó la zona probada por el usuario; falta cobertura en otras zonas.

A petición del usuario se prepara una prueba experimental de lo incorporado,
sin marcar los hitos como completos. Véase la
[lista para el visor](VR_HEADSET_TEST_CHECKLIST.md). Rework permanece intacto.

## Evidencia reproducible

La preparación técnica de la tanda posterior obtuvo **22/22 tests en Release,
22/22 en Debug y 22/22 en Release sin SDK OpenVR**, además del verificador binario
y de metadatos. Estas cifras son pruebas host/locales y no sustituyen la prueba
con visor.

La prueba de visor del 2026-09-06 es la evidencia independiente de los elementos
marcados como validados arriba. Las correcciones posteriores descritas en este
documento deben tener una nueva sesión de visor antes de cambiar su estado a
validado.

El workflow de GitHub compila Debug/Release sin SDK y ejecuta 21 pruebas por
configuración: excluye explícitamente `opengl_eye_targets`, que requiere el
driver WGL con FBO/programas y se verifica localmente. El resultado de CI es
independiente de los 22/22 locales; ninguno certifica una prueba con visor.

- CTest incluye `vr_action_input`, `vr_native_intents` y `vr_math` con foco,
  poses, límites, liberación, consumo único de edges, giro, rayo del menú y ABI
  x86. El test OpenGL comprueba píxeles de ambos ojos y restauración de estado.
- `spatial_interaction` ejecuta el adaptador real contra una imagen sintética y
  funciones nativas dobles: restauración de límites/masa/gravedad, lanzamiento,
  foco/tracking/UI, cero tiempo, salto de pose, joints, retirada segura y
  restauración de `CollideCharacter` para cuerpos originalmente true/false.
  `vr_grab_pose` cubre transformaciones, límites y filtrado robusto de lanzamiento;
  OpenGL comprueba además píxeles de manos y que no atraviesen una profundidad más cercana.
- `tools/Test-BlackPlagueInputMap.ps1 -ImagePath <captura-inicializada>` contrasta
  los bytes de 76 consultas, 2 movimientos, 3 cursores y la vtable de Update.
  También contrasta 11 slots espaciales, las entradas SetMatrix/GetJointNum,
  la llamada de picking y las escrituras del contacto local de agarre.
  La captura local es `artifacts/black-plague-22000-live.bin`, imagen virtual con
  base 0x00400000; **no** usar los offsets como posiciones en el PE de disco.
- `--check-vr` se usa como preflight sin juego/SteamVR; la evidencia de visor del
  2026-09-06 es la que respalda los estados de validación indicados arriba.
