# Arranque VR y estado de los mandos

Actualizado: 2026-09-06. Backend **Black Plague FD316F…** únicamente.
Esta tanda tiene pruebas de código/OpenGL, no una prueba nueva con visor.

## Arrancar sin adjuntar el mod a mano

Desde `E:\penumbra_vr`, doble clic en `Start-Black-Plague-VR.cmd`.
Usa el ejecutable Release y la instalación Steam habitual. También admite la
ruta del ejecutable como primer argumento si Steam está en otra biblioteca.

El lanzador comprueba el hash admitido y los archivos VR, abre Black Plague
mediante Steam (appid 22120), espera la inicialización del ejecutable protegido,
adjunta el framework y activa la presentación continua con el mirror guardado.
No modifica el ejecutable del juego ni sus opciones de lanzamiento en Steam.
Abrir directamente el juego en Steam **no** ejecuta este lanzador automáticamente.
SteamVR debe estar instalado y los dispositivos configurados; OpenVR solicita
su inicialización. No se ha comprobado esta secuencia completa en vivo todavía.

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
2. Puente nativo exacto: movimiento analógico combinado con teclado, giro por
   pasos de 45 grados con retorno a neutro, salto, correr, agacharse, interactuar,
   examinar, guardar objeto, inventario, libreta, pausa y alternancia de linterna.
   No usa emulación de teclas de Windows. Los nombres de acción conservan su
   ABI antiguo y siempre se ejecuta la consulta original del juego.
3. Menú inicial, pausa/inventario/libreta: captura del escritorio presentada en
   ambos ojos como panel de 2,4 metros de ancho a 2 metros, orientado según el
   yaw al abrirlo. Apuntado derecho en menú principal, inventario y libreta,
   coordenadas nativas 800×600, recentrado y prioridad temporal del ratón cuando
   se mueve. Los diálogos especiales conservan sus rutas nativas y aún necesitan
   cobertura en ejecución. Se restauran matrices, texturas, programas GL,
   viewport, scissor y demás estado gráfico tras dibujar el panel.
4. Adaptador espacial integrado en la DLL y compilación: selección desde aim
   derecho/izquierdo en el estado normal, refresco de selección antes de pulsar,
   agarre relativo a la palma de cuerpos libres y lanzamiento limitado a 9 m/s.
   Las transiciones originales conservan la gestión de masa/gravedad; el
   adaptador restaura los límites de velocidad que cambia. Un botón de la otra
   mano no transfiere el objeto ni oculta la liberación de la mano propietaria.
   Pérdida de tracking/foco, UI o salto de pose sueltan sin impulso de lanzamiento.
   Se emiten pulsos hápticos cortos al coger/soltar. Son rutas verificadas con
   dobles de las funciones nativas, no una simulación del motor Newton real.
5. Guantes geométricos provisionales para ambas manos, dedos según curls OpenVR
   (o postura aproximada sin skeleton), rayo de apuntado y oclusión por profundidad
   en cada ojo. No son las mallas/esqueletos HPL de Rework y no proyectan sombras.
   Las manos se sitúan respecto al HMD actual para convivir con el tracking
   rotacional existente; esto no añade room-scale ni colisión de las palmas.

La numeración anterior describe módulos implementados, **no certifica que los
tres hitos pedidos por el usuario estén terminados**.

## Lo que NO está terminado

- El tercer hito original sigue parcial: puertas/palancas y cuerpos con joints
  o padres conservan el comportamiento nativo. Falta la colisión de palmas,
  la exclusión de colisión cuerpo/jugador durante el agarre, las mallas HPL de
  Rework y herramientas/luces ancladas a la mano. Durante un agarre espacial
  todavía no se actualiza el rayo secundario de examinar como hace Rework.
- `PlayerState_Interact_VR.cpp` es la referencia, pero no se copian offsets de
  Overture. Véase `BLACK_PLAGUE_SPATIAL_NOTES.md` para llamadas verificadas,
  pruebas, límites y el siguiente punto de integración de herramientas.
- La configuración de este puente es diestra y de giro por pasos. El lector
  soporta acciones zurdas, pero falta persistir/aplicar esa preferencia al puente.
  Quick light alterna la linterna nativa; falta el ciclo linterna/glowstick de
  Rework. La locomoción sigue los ejes del jugador, no una calibración corporal
  completa del HMD.
- Tracking corporal posicional y Enhanced visuals GPU no están conectados.
- Falta validar transiciones mientras se mantiene un botón, la pérdida de
  tracking/foco en una partida real y la convivencia de mandos y teclado al
  mantener ambos la misma acción. Las pruebas puras no sustituyen esa prueba.
- La corrección de iluminación a media distancia sigue pendiente de visor.

No se solicita una prueba intermedia al usuario ni se marca el trabajo como
completo. Rework permanece intacto en `E:\penumbra_vr_rework`.

## Evidencia reproducible

Resultado de esta tanda: **21/21 tests en Release, 21/21 en Debug y 21/21
en Release sin SDK OpenVR**, además del verificador binario y de metadatos.

El workflow de GitHub compila Debug/Release sin SDK y ejecuta 20 pruebas por
configuración: excluye explícitamente `opengl_eye_targets`, que requiere el
driver WGL con FBO/programas y se verifica localmente. El resultado de CI es
independiente de los 21/21 locales; ninguno certifica una prueba con visor.

- CTest incluye `vr_action_input`, `vr_native_intents` y `vr_math` con foco,
  poses, límites, liberación, consumo único de edges, giro, rayo del menú y ABI
  x86. El test OpenGL comprueba píxeles de ambos ojos y restauración de estado.
- `spatial_interaction` ejecuta el adaptador real contra una imagen sintética y
  funciones nativas dobles: restauración de límites/masa/gravedad, lanzamiento,
  foco/tracking/UI, cero tiempo, salto de pose, joints y retirada segura.
  `vr_grab_pose` cubre transformaciones y límites; OpenGL comprueba además píxeles
  de manos y que no atraviesen una profundidad más cercana.
- `tools/Test-BlackPlagueInputMap.ps1 -ImagePath <captura-inicializada>` contrasta
  los bytes de 76 consultas, 2 movimientos, 3 cursores y la vtable de Update.
  También contrasta 11 slots espaciales, las entradas SetMatrix/GetJointNum,
  la llamada de picking y las escrituras del contacto local de agarre.
  La captura local es `artifacts/black-plague-22000-live.bin`, imagen virtual con
  base 0x00400000; **no** usar los offsets como posiciones en el PE de disco.
- Solo `--check-vr` se ejecutó contra la instalación en esta tanda. El juego y
  SteamVR estaban cerrados; no se afirma haber probado arranque, controles o
  manos en el visor.
