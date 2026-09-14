# Handoff técnico — Penumbra VR Framework

**Auditoría de** **`main`****, commit** **`73c70f0aad6fd2f33dc27983a44971ec44215f4a`****. No he modificado código, documentación ni configuración, ni creado commits.** He ejecutado compilaciones y verificaciones; sus salidas son artefactos de build.

La conclusión principal es que **no hay evidencia de un único world scale incorrecto que explique todos los síntomas**. Sí hay tres grupos importantes de problemas:

1. **La integración temporal entre tracking, cuerpo y render no conserva completamente la semántica de Rework.**
2. **La interacción de Black Plague reutiliza las fórmulas de agarre, pero todavía no reproduce el contrato completo de contacto y palma de Rework.**
3. **El ownership de estados persistentes y la inicialización siguen teniendo transiciones incompletas**, particularmente crouch, cambios de jugador y recuperación tras fallos parciales.

Hay infraestructura válida que debe conservarse: el producto autónomo de Overture, las primitivas comunes de tracking y locomoción, el límite físico de Black Plague en `0xD7281`, la propiedad única del tick nativo y la articulación de manos de Black Plague.

---

## 1. Estado real actual

### Repositorios y baseline

| ElementoEstado comprobado              |                                                                                                                          |
| -------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| Framework                              | `E:\penumbra_vr`                                                                                                         |
| Branch                                 | `main`                                                                                                                   |
| HEAD                                   | `73c70f0aad6fd2f33dc27983a44971ec44215f4a`                                                                               |
| Working tree                           | Limpio al comenzar y al terminar                                                                                         |
| Relación con `origin/main`             | `0/0` respecto a la referencia local; no he hecho fetch                                                                  |
| Rework local                           | `E:\penumbra_vr_rework`, working tree limpio                                                                             |
| HEAD del checkout Rework               | `31e02767babf9f072b438e2ca4e89a1bfa781716`                                                                               |
| Referencia conductual utilizada        | **`23c890f7dbd06b939be9951d282e6e948d9a6623`**, mediante lectura de esa revisión; no confundida con el HEAD del checkout |
| Validación live durante esta auditoría | Ninguna; no he iniciado el juego ni una sesión de casco                                                                  |

### Build y verificaciones ejecutadas

| VerificaciónResultado                              |                                                                                                                 |
| -------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| Configuración `vs2022-win32`                       | Correcta                                                                                                        |
| Build Release                                      | Correcto                                                                                                        |
| CTest Release                                      | **30/30**                                                                                                       |
| Build Debug                                        | Correcto                                                                                                        |
| CTest Debug                                        | **30/30**                                                                                                       |
| Configuración y build Release sin SDK OpenVR       | Correctos                                                                                                       |
| CTest sin SDK OpenVR                               | **30/30**                                                                                                       |
| `Test-PenumbraVrMetadata.ps1`                      | Correcto: 6 entradas de catálogo, 2 manifests, 42 acciones, 6 action sets, 8 bindings                           |
| `Test-BlackPlagueInputMap.ps1` sobre captura local | Correcto para las fronteras que comprueba                                                                       |
| Overture `Release -Full`                           | Correcto                                                                                                        |
| Verificaciones adicionales de Overture             | 289 checks; 16 shaders; 8.752 comprobaciones visuales CPU; selección/decodificación de 231 texturas; sin fallos |

La compilación completa de Overture produjo advertencias de dependencias y código heredado —incluyendo conversiones, APIs obsoletas y PDB ausentes—, pero ningún fallo.

**No hay un fallo automático reproducido que invalide el baseline.** Esto no significa que los defectos descritos después estén cubiertos por los tests.

### Estado funcional y de validación

| ComponenteClasificación actual         |                                                                                                                                   |
| -------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| Overture autónomo                      | Integrado y con evidencia previa de prueba funcional en casco de un ejecutable identificado; regresión automática actual correcta |
| Tracking/locomoción comunes            | Implementados y host-tested; reutilizados de forma real, aunque todavía incompleta                                                |
| Body adapter de Black Plague           | Implementado y con evidencia live                                                                                                 |
| Shadow de reconciliación               | Implementado, default-off y con evidencia live; no equivale a validación de cámara posicional                                     |
| Inyección física `0xD7281`             | Implementada y live-tested                                                                                                        |
| Locomoción métrica de Black Plague     | Evidencia de casco en PID 8092, **dentro del gate room-scale**                                                                    |
| Corrección actual de crouch, `73c70f0` | Implementada y host-tested; **sin evidencia posterior de casco encontrada**                                                       |
| Room-scale completo                    | Parcial; continúa abierto el malestar de desplazamientos físicos cortos                                                           |
| Free-body grab de Black Plague         | Implementado parcialmente respecto al contrato completo de Rework                                                                 |
| Mecanismos VR de Black Plague          | No equivalentes todavía al sistema de mecanismos de Rework                                                                        |
| Mirror                                 | Experimental; ownership de presentación conocido parcialmente, problemas de foco abiertos                                         |
| Requiem                                | Reconocimiento y preparación de catálogo; no backend funcional demostrado                                                         |

La evidencia histórica de Overture identifica el ejecutable con SHA-256 `D4FAC244E73729966C8B9BF42F4A9BBFF9BD42F02710DCB1EACA3B668F1A9EE1`. **No transfiero esa validación automáticamente a cualquier ejecutable recompilado hoy.**

### Problemas históricos: qué cambió realmente

- **La ausencia de body adapter y de frontera horizontal segura ya no es un pendiente.** No debe reabrirse su descubrimiento.
- **La locomoción métrica que no llegaba a encolarse sí recibió correcciones y evidencia posterior.** Los errores del predicado y sus campos indexados se corrigieron; PID 8092 demuestra funcionamiento del recorrido técnico.
- **El movimiento inducido por inclinar la cabeza y la sacudida continua anterior recibieron correcciones y evidencia favorable.** Eso no cierra el posterior efecto de “el mundo tira hacia atrás”.
- **La clasificación de bloqueo por magnitud total fue corregida para usar proyección sobre la petición.** Fue un defecto del validador, no una nueva corrección de colisiones.
- **La solución actual de crouch ya no depende de compensar toggles mediante pulsaciones adicionales.** Usa `ChangeMoveState(4/0)`. Falta validar esta revisión concreta.
- **No he confirmado que el crash histórico de SDL haya quedado explicado o resuelto causalmente.** Existen sesiones exitosas posteriores, pero eso no identifica su causa.

En los logs locales de PID 8092, 20520, 23260 y 24948 aparecen desplazamientos de locomoción inyectados compatibles con `1.5/2.25 m/s`. Son medidas de **petición/inyección**, no una medición independiente de velocidad aceptada sostenida.

---

## 2. Mapa de arquitectura

### Existen dos formas de integración

No hay un único flujo universal «DLL → selector → backend» para todos los juegos.

**Overture:**

```
Producto autónomo HPL1/Overture
  → Game.cpp: adquisición y actualización
  → ButtonHandler / Player
  → OvertureSourceIntegration
  → OvertureBackend
  → políticas runtime
  → HplOvertureBodyAdapter y objetos HPL
  → render y juego fuente
```

**Black Plague:**

```
ProbeLauncher
  → arranque Steam / detección del proceso
  → comprobación del ejecutable y disponibilidad de fronteras
  → carga remota de la DLL
  → PenumbraVR_Initialize
  → instalación ordenada de owners
  → NativeInputBridge + BodyCollisionProbe
  → BlackPlagueBodyAdapter
  → políticas runtime y snapshots
  → interacción / cámara estéreo / SDL swap
```

### Responsabilidades concretas

| ResponsabilidadPropietario actual                          |                                                               |
| ---------------------------------------------------------- | ------------------------------------------------------------- |
| Identificación exacta de build                             | `src/common/build_catalog.cpp`                                |
| Arranque, espera y llamadas remotas                        | `src/launcher/main.cpp`                                       |
| Orquestación del probe                                     | `src/probe/black_plague_probe.cpp`                            |
| Transformaciones, locomoción, input lógico, crouch, agarre | `src/runtime`                                                 |
| Integración fuente de Overture                             | `src/adapters/overture_source/overture_source_integration.*`  |
| Política de backend Overture                               | `src/backends/overture/overture_backend.*`                    |
| Consultas y movimiento nativos BP                          | `src/backends/black_plague/native_input_bridge.*`             |
| Tick y frontera de colisión BP                             | `src/backends/black_plague/body_collision_probe.*`            |
| Binding del cuerpo y reconciliación BP                     | `black_plague_body_adapter.*`, `body_reconciliation_shadow.*` |
| Picking, agarres y herramientas BP                         | `spatial_interaction.*`                                       |
| Cámara, visibilidad y estéreo BP                           | `render_world_probe.*`                                        |
| Hook SDL                                                   | `src/hooks/sdl_frame_hook.*`                                  |
| Escritura segura de callsites                              | `src/hooks/rel32_call_hook.*`                                 |

### Secuencia actual de inicialización de Black Plague

En [PenumbraVR\_Initialize (line 890)]\(E:/penumbra\_vr/src/probe/black\_plague\_probe.cpp:890):

1. Transición atómica de estado.
2. Identificación del ejecutable y carga de settings.
3. Telemetría de matrices OpenGL.
4. Hooks de `RenderWorld` y visibilidad.
5. Hook de `SDL_GL_SwapBuffers`.
6. `NativeInputBridge`.
7. `BodyCollisionProbe`.
8. `BlackPlagueBodyAdapter`.
9. `MovementOwnershipProbe`.
10. `SpatialInteraction`.
11. Publicación de estado listo.

La presentación persistente inicializa después OpenVR, acciones, óptica, targets y conexión de input.

**Consecuencia:** recibir callbacks SDL antes de que esté instalado el bridge es compatible con el orden actual. Dos callbacks no demuestran dos inicializaciones ni dos hooks sobre el mismo sitio.

### Owners binarios que deben conservarse

- `D460A → D6E00`: actualización nativa del character body.
- `D7312 → D4830`: frontera observada del solver horizontal.
- `D7281`: inyección X/Z antes de la resolución nativa.
- `NativeInputBridge`: propietario de los callsites de movimiento y consultas.
- `EDF84 → 12A8F0`: actualización de lista de render/visibilidad.
- `EE010 → 12CB10`: `RenderWorld`.

El body adapter comprueba y consume esos owners; **no instala otro hook sobre ellos**.

### Render: orden importante

El callsite de visibilidad ocurre **antes** del callsite de `RenderWorld`.

Sin embargo, el HMD fresco se adquiere en `ProcessControlledStereoMatrices`, dentro del segundo. `HookedUpdateRenderList` utiliza el estado cacheado anteriormente.

El comentario que afirma que `UpdateRenderList` sucede dentro de los pases de ojo no describe el orden del callsite interceptado. Esto importa: el frustum y los ojos pueden construirse con muestras diferentes.

### IPC y configuración

El control actual combina:

- exports remotos y threads remotos;
- mutex de activación;
- configuración persistida en archivos;
- telemetría en logs.

No encontré un servicio IPC genérico cuya sustitución sea necesaria.

Los manifests y verificadores documentan/comprueban fronteras; buena parte del ABI efectivo sigue expresada mediante constantes dentro del backend. **No conviene convertir esas direcciones en una supuesta API HPL común.**

### Dónde deben quedar las responsabilidades

**Runtime común:**

- espacios y unidades;
- política de tracking, yaw, recenter y postura;
- velocidad y reconciliación;
- input lógico;
- contacto/palma y composición de agarre, una vez extraídos fielmente;
- semántica de articulación independiente del rig.

**Backend/adaptador:**

- adquisición de cuerpo y cámara;
- ejecución de consultas físicas;
- aplicación de move state;
- clasificación nativa de entidades y mecanismos;
- layouts, RVAs, firmas y ABI;
- integración con las fases reales del motor.

**Perfiles/datos:**

- sockets de herramientas;
- correspondencias de huesos;
- dimensiones de modelos;
- capacidades y diferencias demostradas entre builds.

No encontré una invasión generalizada de RVAs de juego en `runtime`. La deuda principal es **comportamiento neutral que todavía permanece en Overture o se reproduce parcialmente en Black Plague**.

---
