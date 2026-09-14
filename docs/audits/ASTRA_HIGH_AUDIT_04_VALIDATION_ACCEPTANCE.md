## 7. Archivos afectados

Todas las rutas siguientes son absolutas. Los comodines indican el par fuente/header cuando exista.

### Modificar

| RutaMotivo                                                                        |                                                                          |
| --------------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| `E:\penumbra_vr\src\backends\black_plague\native_input_bridge.*`                  | Ownership crouch, intención por tick, restricciones y muestra            |
| `E:\penumbra_vr\src\backends\black_plague\body_collision_probe.*`                 | Preparación/finalización en el owner existente                           |
| `E:\penumbra_vr\src\backends\black_plague\black_plague_body_callbacks.hpp`        | Contrato de transacción                                                  |
| `E:\penumbra_vr\src\backends\black_plague\body_adapter_boundary.*`                | Peticiones identificadas y estado verificable                            |
| `E:\penumbra_vr\src\backends\black_plague\black_plague_body_adapter.*`            | Matching, generaciones y publicación                                     |
| `E:\penumbra_vr\src\backends\black_plague\body_reconciliation_shadow.*`           | Planner único con modos observador/activo                                |
| `E:\penumbra_vr\src\backends\black_plague\render_world_probe.*`                   | Muestra compartida, culling, predicción y yaw                            |
| `E:\penumbra_vr\src\backends\black_plague\spatial_interaction.*`                  | Contacto, hold y attachments                                             |
| `E:\penumbra_vr\src\backends\black_plague\vr_settings_capabilities.*`             | Separar capacidad implementada de activación y validación                |
| `E:\penumbra_vr\src\runtime\vr_tracking_types.hpp`                                | Identidad y épocas de muestras                                           |
| `E:\penumbra_vr\src\runtime\vr_crouch_policy.*`                                   | Feedback de stand y equivalencia Rework                                  |
| `E:\penumbra_vr\src\runtime\openvr_session.*`                                     | Publicación coherente de poses, si requiere ampliar API                  |
| `E:\penumbra_vr\src\runtime\openvr_controller_input.cpp`                          | Separar poses de edges                                                   |
| `E:\penumbra_vr\src\backends\overture\overture_backend.*`                         | Extraer seated sin cambiar resultados                                    |
| `E:\penumbra_vr\src\adapters\overture_source\overture_source_integration.*`       | Conexión de políticas compartidas                                        |
| `E:\penumbra_vr\src\adapters\overture_source\PenumbraVR.Framework.Overture.props` | Compilar nuevas extracciones comunes                                     |
| `E:\penumbra_vr\products\overture\PenumbraOverture\ButtonHandler.cpp`             | Sustituir únicamente núcleo neutral de crouch cuando exista equivalencia |
| `E:\penumbra_vr\products\overture\PenumbraOverture\Player.cpp`                    | Extracción limitada de política de palma en fase 5B                      |
| `E:\penumbra_vr\src\probe\black_plague_probe.cpp`                                 | Lifecycle transaccional                                                  |
| `E:\penumbra_vr\src\hooks\rel32_call_hook.cpp`                                    | Región suspendida sin asignaciones                                       |
| `E:\penumbra_vr\src\launcher\main.cpp`                                            | Resultados parciales/indeterminados                                      |
| `E:\penumbra_vr\src\graphics\opengl_menu_frame.cpp`                               | Alcance visual y aplicación de perfil de mano                            |
| `E:\penumbra_vr\CMakeLists.txt`                                                   | Harnesses y políticas nuevas                                             |
| `E:\penumbra_vr\tools\Test-BlackPlagueInputMap.ps1`                               | Fronteras nuevas y procedencia de captura                                |
| `E:\penumbra_vr\tools\Start-BlackPlaguePhysicalDisplacementValidation.ps1`        | Correlación temporal y postura                                           |
| Tests existentes de cuerpo, tracking, input, interacción y hooks                  | Contratos nuevos y regresiones                                           |

### Crear — nombres propuestos

- `E:\penumbra_vr\src\runtime\vr_play_mode_policy.hpp/.cpp`: extracción de seated.
- `E:\penumbra_vr\src\runtime\vr_hand_contact.hpp/.cpp`: sólo para el port real del algoritmo de palma.
- `E:\penumbra_vr\tests\black_plague_input\native_input_bridge_test.cpp`: probar el bridge, no solamente `VrNativeIntents`.
- `E:\penumbra_vr\tests\black_plague_tracking\tracking_pipeline_test.cpp`: secuencias entre fases y frecuencias.
- `E:\penumbra_vr\tests\probe_lifecycle\probe_lifecycle_test.cpp`: fallos parciales.

### Eliminar

**No propongo eliminar archivos completos ahora.** Eliminar bloques duplicados sólo después de sustituirlos por la política común y demostrar equivalencia.

### Inspeccionar sin cambiar inicialmente

- `E:\penumbra_vr_rework`, revisión `23c890f`.
- `E:\penumbra_vr\products\overture\HPL1Engine\include\game\VRTracking.h`.
- `E:\penumbra_vr\products\overture\HPL1Engine\sources\game\Game.cpp`.
- `E:\penumbra_vr\products\overture\PenumbraOverture\PlayerState_Interact_VR.cpp`.
- Código HPL de colisiones, callbacks y shapes.
- Catálogo, manifests y ejecutable exacto.
- Logs locales y capturas conservadas.

Documentación: actualizar los handoffs y documentos técnicos afectados al finalizar cada cambio relevante; evitar reescribir toda la documentación por uniformidad.

---

## 8. Qué NO tocar

- El loop de Overture conservado de Rework.
- `PlayerState_Interact_VR.cpp`, salvo una necesidad concreta demostrada durante extracción.
- World scale y calibración vertical como compensación de bugs de contacto o timing.
- Fuerza de salto, gravedad o pipeline vertical nativo de BP.
- Número de actualizaciones de `D6E00`.
- Owner del límite `D7281`.
- Predicados de movimiento ya corregidos, salvo ampliar su resultado con semántica comprobada.
- RVAs de Overture como sustitutos de los de Black Plague.
- Dimensiones del cuerpo BP sin una prueba discriminante.
- Doors/levers/joints mediante free-body grab.
- Articulación rica de dedos de Black Plague.
- Shaders, calibración visual CPU y audio espacial por este diagnóstico.
- Instalador de producción, packaging nuevo o soporte especulativo de Requiem.
- Pulido del mirror como requisito para corregir movimiento.
- Eliminación estética de fallbacks o código histórico funcional.

---

## 9. Validación automática

Desde `E:\penumbra_vr`:

```
cmake --preset vs2022-win32

cmake --build --preset debug
ctest --preset debug --output-on-failure

cmake --build --preset release
ctest --preset release --output-on-failure

cmake -S . -B build-no-openvr -G "Visual Studio 17 2022" -A Win32 -DBUILD_TESTING=ON -DPENUMBRA_VR_OPENVR_SDK=
cmake --build build-no-openvr --config Release
ctest --test-dir build-no-openvr -C Release --output-on-failure

.\tools\Test-PenumbraVrMetadata.ps1
.\tools\Test-BlackPlagueInputMap.ps1 -ImagePath .\artifacts\black-plague-22000-live.bin

.\tools\Build-OvertureProduct.ps1 -Configuration Release -Full

git diff --check
git status --short
```

### Límites de cobertura actuales

- `vr_native_intents` no prueba el flujo real de `NativeInputBridge`.
- Las pruebas de cámara no reproducen todo el orden visibilidad→RenderWorld.
- Los tests SDL utilizan stub; no reproducen el mutex interno de la SDL del juego.
- `openvr_session` cubre principalmente fallos/inicialización sin demostrar presentación válida en un casco.
- Los tests del adapter no detectan por sí solos secuencias continuas a frecuencias distintas.

### Precaución con el verificador de imagen

La captura `black-plague-22000-live.bin` contiene callsites de render ya redirigidos. El verificador actual pasa porque comprueba otras fronteras relevantes.

**No debe describirse ese resultado como validación de todos los bytes de una imagen prístina.**

Ampliar el verificador exige:

- procedencia y hash de captura;
- clasificación explícita de callsite original o hook conocido;
- rechazo de destinos desconocidos;
- captura limpia para comprobar fronteras que ésta ya tiene modificadas.

---

## 10. Validación live

Usar el ejecutable soportado y registrar hash de DLL/EXE, PID, settings y fuente del gate. Conservar el mirror encendido para la prueba room-scale actual.

El helper existente es:

```
.\tools\Start-BlackPlagueRoomScaleValidation.ps1
```

Ese helper inicia una sesión y puede preparar ajustes; pertenece a la fase live, no al baseline automático.

No he identificado un save único verificable que deba imponer por nombre. Seleccionar y registrar uno que contenga suelo plano, pared, esquina, paso bajo y cuerpos manipulables; repetir siempre ese save.

| OrdenEscenario y acciónObservación/telemetríaQué demuestra |                                                                   |                                                            |                                                               |
| ---------------------------------------------------------- | ----------------------------------------------------------------- | ---------------------------------------------------------- | ------------------------------------------------------------- |
| 1                                                          | Probe instalado sin presentación VR; caminar y usar crouch nativo | Owner nativo, consultas preservadas                        | Refuta/regresa R4                                             |
| 2                                                          | Activar, parar y reactivar presentación; repetir con menú/foco    | Estado de componentes y original por callback              | Lifecycle coherente; no usar sólo número de frames            |
| 3                                                          | Suelo libre, quieto 10 s; inclinaciones sin traslación            | Pose/tick/época; jitter; petición X/Z                      | No caminar por tilt ni acumular muestras                      |
| 4                                                          | Traslación física corta y lenta, parar y volver                   | Petición, inyección, aceptación y ancla correlacionadas    | Detecta oscilación temporal y pullback                        |
| 5                                                          | Repetir a distintas frecuencias disponibles del visor             | Misma trayectoria física y política de tick                | Independencia de frecuencia                                   |
| 6                                                          | Pared frontal, después avance oblicuo por esquina                 | Proyección aceptada sobre petición y slide                 | Bloqueo real frente a clasificador incorrecto                 |
| 7                                                          | Stick adelante/atrás/lateral, yaw distinto, caminar/sprint        | Metros pedidos y aceptados por tiempo físico               | `1.5/2.25`; sin dependencia de eje nativo oculto              |
| 8                                                          | Stick y movimiento físico en igual y opuesta dirección            | Total observado y reparto derivado diferenciados           | Conservación y límites de la adaptación combinada             |
| 9                                                          | Dos crouches físicos completos                                    | Deseo, move state `4/0`, shape `0.95/1.65` correlacionados | Cierra corrección actual; contadores agregados no bastan      |
| 10                                                         | Crouch sólo botón, Hybrid y levantarse bajo techo                 | Latch, baseline, bloqueo, retry y estado final             | Equivalencia con postura Rework                               |
| 11                                                         | Seated/standing y recenter; giro con stick                        | Época nueva, sin petición residual; altura continua        | Tracking y postura coherentes                                 |
| 12                                                         | Caja pequeña y cuerpo largo: tocar extremos, rotar y soltar       | Contacto local, palma visual/resuelta y hold generation    | Contrato de contacto y pivote                                 |
| 13                                                         | Mano contra pared, objeto detrás, pérdida de tracking             | Ninguna adquisición VR por fallback remoto                 | Alcance/oclusión reales                                       |
| 14                                                         | Flashlight/glowstick, ambas manos y giros de muñeca               | Socket, geometría y articulación                           | Attachment correcto sin offsets globales                      |
| 15                                                         | Abrir menú/cambiar mapa mientras se sostiene un cuerpo            | Terminación o invalidación segura del hold                 | Sin hold huérfano ni escritura caducada                       |
| 16                                                         | Alt+Tab y mirror on/off, en prueba separada                       | Mundo frente a UI y recuperación del foco                  | Delimita presentación sin contaminar aceptación de movimiento |

Para investigar rechazo de “cabeza” anticipado, registrar **posición del cuerpo, radio, ancla de cabeza, pared/contacto y petición rechazada**. Si el rechazo corresponde al proxy nativo de radio `0.35 m`, investigar su adaptación; si aparece sin contacto físico válido, investigar reconciliación. No introducir un collider ficticio.

### Telemetría mínima nueva

Una línea por transición y muestras acotadas durante cada escenario:

```
session_generation, player_generation, body_generation
pose_sequence, tracking_epoch, yaw_epoch, native_tick_sequence
B0, B1, physical_request, locomotion_request, total_accepted
physical_attribution, carry, reconciled_anchor, render_prediction
crouch_desired, native_move_state, body_height, stand_blocked
selection_id, hold_generation, release_reason
```

No volcar todo cada frame indefinidamente. Usar una ventana circular para anomalías y resúmenes correlacionados.

Si reaparece el APPCRASH: conservar dump con stack, registros y módulos. El objetivo es identificar el argumento que llega a `SDL_mutexP` y su caller, no aumentar logs del shadow.

---

## 11. Criterios de aceptación

La intervención se considera terminada únicamente cuando:

1. Todos los comandos automáticos pasan.
2. Overture conserva sus trazas de tracking, locomoción, crouch y contacto, además de su prueba funcional de casco.
3. El bridge sin VR no altera crouch nativo.
4. Cada tick nativo tiene como máximo una petición consumida y una reconciliación correspondiente.
5. Una rampa física libre no produce peticiones oscilantes por usar una posición anterior.
6. No hay integración duplicada de HMD, carry o stick.
7. Cámara, culling y manos usan muestras identificadas con épocas compatibles.
8. La relación métrica de poses se conserva; la calibración Y no se aplica como escala global.
9. Black Plague utiliza `1.5/2.25 m/s` en la ruta validada y la restricción equivalente cuando corresponda.
10. Crouch físico completa dos ciclos correlacionados, mantiene stealth por botón y termina de pie cuando hay espacio.
11. Seated y recenter no dejan desplazamientos pendientes.
12. La selección VR no adquiere cuerpos a través de un fallback nativo remoto.
13. Contacto, palma, herramientas y visualización coinciden; los cuerpos largos mantienen un contacto explicable.
14. Ningún hold sobrevive como estado huérfano a un lifecycle terminado.
15. Los estados de inicialización y shutdown describen los componentes realmente instalados.
16. No aparece crash en el protocolo relevante repetido. Esto cierra ese gate de estabilidad, **no demuestra retrospectivamente la causa del crash histórico**.
17. El usuario confirma que el desplazamiento físico corto ya no produce pullback o movimiento prematuro del mundo.
18. Las capacidades no realizadas —mecanismos VR completos, mirror estable, Requiem— siguen identificadas como pendientes.

Si no se completa la frontera de queries de palma de la fase 5B, se puede cerrar la mejora de ownership/selección de 5A, **pero no declarar equivalencia completa de interacción con Rework**.

---
