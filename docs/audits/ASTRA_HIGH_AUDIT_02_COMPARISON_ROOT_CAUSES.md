## 3. Comparación Overture Rework vs Framework

| SistemaReferencia `23c890f`Estado actualDiferencia y consecuenciaAcción |                                                                    |                                                                                          |                                                                                |                                                              |
| ----------------------------------------------------------------------- | ------------------------------------------------------------------ | ---------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------ | ------------------------------------------------------------ |
| Tracking y escala                                                       | `HPL1Engine/include/game/VRTracking.h`                             | Fórmula equivalente en `runtime::VrTrackingSpace`; implementación HPL también conservada | Dos implementaciones equivalentes, no un único consumidor                      | Mantener resultados; proteger con comparación diferencial    |
| Altura                                                                  | `(HMD.y − 0.2) × 1.065`, más calibración/postura/seated            | Preservado en común; BP lo utiliza en el nuevo placement posicional                      | `1.065` **no es un escalado global del mundo**                                 | No retocar world scale como compensación                     |
| Distancias mano/cabeza                                                  | Transformación rígida tracking→world                               | Primitivas comunes conservan ese contrato                                                | No evidencia de un factor global erróneo                                       | Tests métricos entre varios consumidores                     |
| Locomoción                                                              | `1.5 m/s`, sprint `2.25`, restringida `0.5`                        | Común conserva constantes; BP utiliza normal/sprint en gate                              | BP pasa `constrained=false`; la ruta normal fuera del gate sigue siendo nativa | Compartir política completa; adaptar clasificación de estado |
| Paso físico                                                             | Clamp horizontal `0.05 m`                                          | Conservado en BP                                                                         | No equivale al tamaño de peldaño nativo                                        | Mantener ambos conceptos separados                           |
| Reconciliación                                                          | Petición, actualización física y reconciliación con orden definido | BP planifica tras el tick usando `body_before` y encola para el siguiente                | Mezcla de épocas del cuerpo                                                    | Transacción explícita por tick                               |
| Físico + stick                                                          | Resolución secuencial en Rework                                    | Una resolución combinada en BP                                                           | El reparto de aceptación es derivado, no observado por separado                | Conservar frontera; explicitar y probar esa adaptación       |
| Body proxy                                                              | Overture reduce anchura/profundidad a la mitad                     | BP conserva cuerpo nativo observado de anchura `0.7 m`                                   | Diferencia plausible para rechazo anticipado; no prueba de radio correcto BP   | Medir antes de modificar dimensiones                         |
| Seated                                                                  | Política funcional                                                 | `OvertureBackend::UpdatePlayMode`; BP no la aplica                                       | Comportamiento neutral sigue en un backend                                     | Extraer y reutilizar sin reinterpretación                    |
| Crouch                                                                  | Deseo persistente, bloqueo al levantarse y baseline protegido      | Overture conserva original; BP usa nueva política común y estado `4/0`                   | Falta feedback de techo en política común; ownership de sesión incompleto      | Completar contrato y conectar ambos consumidores             |
| Yaw                                                                     | Ownership de tracking-world-yaw                                    | BP continúa acoplado al yaw de cámara/jugador nativo                                     | Muestras y giro no tienen época común                                          | Unificar contrato; validar cambio de owner por separado      |
| Palma y contacto                                                        | Overlap, resolución y recuperación de mano                         | BP redirige un ray corto desde aim                                                       | Aproximación distinta del sistema de contacto probado                          | Portar algoritmo; adaptar únicamente queries físicas         |
| Free-body grab                                                          | Seguimiento cinemático de la palma resuelta                        | BP usa composición común y `SetMatrix`                                                   | El seguimiento rígido no es por sí mismo una regresión                         | Conservar; corregir selección, palma y lifecycle             |
| Herramientas                                                            | Grip y cierre ligados al modelo/rig                                | BP tiene sockets medidos y articulación rica                                             | Falta un contrato único entre grip, palma visual y cierre por herramienta      | Perfiles coherentes, no offsets globales                     |
| Mecanismos                                                              | Hinge/slider y contacto de superficie específicos                  | BP excluye joints del free-body path                                                     | Correcta exclusión; equivalencia VR todavía ausente                            | Mantener comportamiento nativo hasta adaptar sus estados     |
| Loop Overture                                                           | `Game.cpp`                                                         | Sin diferencias frente a referencia en la comparación realizada                          | Conservado                                                                     | No reescribir                                                |
| Interacción Overture                                                    | `PlayerState_Interact_VR.cpp`                                      | Sin diferencias frente a referencia                                                      | Conservada                                                                     | Fuente conductual, fuera de retoques locales                 |

Dos detalles evitan diagnósticos equivocados:

- **`0.05 m`** **es un límite de desplazamiento horizontal por operación**, no la altura de escalón. El `MaxStepSize` nativo observado en Black Plague es otro parámetro.
- Rework también coloca cinemáticamente cuerpos agarrados. Sustituirlo por un muelle porque una mesa “se pega” a la mano inventaría otro comportamiento antes de explicar la diferencia real.

---

## 4. Causas raíz

### R1. Planificación física con una época de cuerpo incorrecta

**Prioridad: máxima. Confianza alta en el defecto temporal; media en que explique el malestar reportado.**

Evidencia:

- [BodyReconciliationShadow::Observe (line 29)]\(E:/penumbra\_vr/src/backends/black\_plague/body\_reconciliation\_shadow\.cpp:29) se ejecuta después del tick.
- En la rama normal llama a `PlanBodyReconciliation(anchor_, checked.body_before, physical_delta)`.
- El adapter publica ese plan como petición de un tick posterior.
- La reconciliación anterior y el carry se mezclan alrededor de esa planificación.

Una reproducción algebraica unidimensional, sin colisión ni stick, con HMD avanzando `1 cm` por muestra, produce peticiones sucesivas como:

```
1 cm, 2 cm, 2 cm, 1 cm, 0 cm, 0 cm, 1 cm, 2 cm…
```

El cuerpo alterna retraso y adelanto pese a que la entrada es uniforme. Esta reproducción fue analítica; **no la presento como un nuevo test C++ ni como una prueba de casco**.

**Síntomas compatibles:** retroceso percibido en desplazamientos físicos cortos, comportamiento irregular cerca de una pared, sensibilidad a la relación entre frecuencia física y render.

**Historia:** la estructura procede del shadow y su activación posterior. `1dbbd10` corrigió el doble carry de movimiento físico, pero no cambió esta época de planificación.

**Alternativa descartada como explicación suficiente:** retocar velocidad o world scale no corrige que una petición se calcule contra una posición que ya dejó de ser actual.

### R2. Cámara, culling, manos y movimiento no comparten una muestra y una época de yaw

**Confianza alta en la inconsistencia; media en sus efectos perceptivos concretos.**

Evidencia:

- HMD fresco adquirido en `RenderWorld`.
- Visibilidad anterior utilizando `g_stereo_latest_pose` y `g_stereo_room_scale_sample`.
- Controllers adquiridos desde el update de input mediante la ruta de pose de acciones.
- Placement extrapolado con yaw calculado desde la cámara nativa actual.
- La muestra reconciliada no identifica de forma suficiente la época de yaw y pose.
- El contador de tick en la estructura consumible se reinicia al vaciarla; no sirve como identidad monotónica global.

Archivos principales: [render\_world\_probe.cpp (line 476)]\(E:/penumbra\_vr/src/backends/black\_plague/render\_world\_probe.cpp:476), `native_input_bridge.cpp`, `black_plague_body_adapter.*`, `openvr_controller_input.cpp`.

**Síntomas compatibles:** divergencia momentánea entre mano visual y contacto, cambios alrededor de giros/recenter, culling desfasado y amplificación del problema R1.

**Alternativa descartada:** no basta con que cada matriz individual sea rígida y correcta. Su composición puede ser temporalmente incoherente.

### R3. Extracción incompleta de la política de movimiento y postura

**Confianza alta.**

Evidencia:

- `HookedSideways` llama a `LocomotionDisplacement(..., false, ...)`: no aplica movimiento restringido.
- Calcula desplazamiento con el `dt` del callback de input, no bajo el owner del tick físico.
- Seated sigue en `OvertureBackend`.
- Overture conserva `UpdateVRCrouch`; la política BP no recibe el bloqueo de levantarse que protege el baseline en Rework.
- La locomoción métrica depende del gate room-scale.

**Síntomas explicados:** diferencias de velocidad entre rutas, ausencia de equivalencia seated y estados de postura no totalmente coordinados.

**Límite:** no he demostrado una velocidad duplicada por el `dt` actual. Los logs disponibles muestran peticiones correctas en sesiones concretas. El problema confirmado es la ausencia del contrato de integración por tick.

**Jump:** permanece en un pipeline nativo distinto. No hay evidencia suficiente para atribuir el salto agresivo a estas mismas causas ni para cambiar su fuerza.

### R4. El owner VR de crouch intercepta también la ruta sin VR

**Confianza alta por flujo de código; falta reproducción live específica.**

En [HookedQuery (line 299)]\(E:/penumbra\_vr/src/backends/black\_plague/native\_input\_bridge.cpp:299):

1. Se ejecuta la consulta nativa.
2. Toda consulta de crouch devuelve `false`, exista o no una sesión VR.
3. La pulsación nativa se difiere a la política común.
4. Sin sesión enfocada, esa política no procesa el toggle como gameplay activo.

Por tanto, mantener el probe/bridge instalado sin presentación VR puede suprimir el crouch nativo. Además, el deseo y `g_vr_crouch_owned` no están ligados a una identidad completa jugador/mundo.

**Síntomas explicados:** input nativo perdido, estado persistente aplicado en una transición incorrecta, crouch que sobrevive a un cambio de ownership.

**Alternativa descartada:** volver a pulsaciones compensatorias o al toggle configurable. El acceso directo `ChangeMoveState(4/0)` es la frontera correcta ya identificada; falta gobernar cuándo la posee VR.

### R5. El contrato de interacción termina demasiado pronto: fórmula de agarre compartida, contacto incompleto

**Confianza alta en la diferencia; media para cada síntoma visual histórico.**

En [spatial\_interaction.cpp (line 136)]\(E:/penumbra\_vr/src/backends/black\_plague/spatial\_interaction.cpp:136):

- Picking usa aim con alcance limitado a aproximadamente `0.18 m`.
- Adquisición del agarre utiliza grip como palma.
- No vuelve a comprobar distancia/oclusión entre el contacto seleccionado y esa palma.
- Si no hay pose válida, el ray vuelve al origen nativo.
- La representación visual del ray puede extenderse mucho más que el alcance efectivo.
- La palma no reproduce la resolución de colisión de Rework.
- Ante mismatch de jugador/estado, un hold puede conservarse sin poder continuar ni liberarse normalmente.

**Síntomas compatibles:** objetos seleccionados desde una posición inesperada, contacto que parece flotar, mesas/cuerpos largos con pivote sorprendente, agarres retenidos en transiciones.

Los sockets de glowstick y flashlight existen y están expresados como datos específicos. **No he encontrado evidencia de que la fórmula común del socket esté mal.** La relación entre esos sockets, la palma procedural y el cierre de dedos sigue necesitando validación geométrica y de casco.

**Alternativa descartada:** no se deben forzar puertas/joints al free-body path ni compensar cada modelo con offsets globales.

### R6. Inicialización y recuperación no son transacciones completas

**Confianza alta en las rutas defectuosas; no son una explicación demostrada del APPCRASH.**

Evidencia:

- Algunas ramas de inicialización ignoran errores de `Remove*`, limpian capabilities y vuelven a estado `0`.
- Algunas ramas de shutdown vuelven a estado `2` aunque ya hayan desmontado componentes anteriores.
- Eso permite describir como “limpio” o “listo” un conjunto parcial.
- [SuspendedThreads::SuspendOthers (line 29)]\(E:/penumbra\_vr/src/hooks/rel32\_call\_hook.cpp:29) puede hacer `vector::push_back` y construir errores después de suspender threads. Una asignación de memoria puede bloquearse si un thread suspendido retenía un lock necesario.

**Intervención:** ledger de componentes, estado parcial explícito y región suspendida sin asignaciones.

**Alternativa descartada:** “SDL falló porque acabábamos de activar el shadow”. La secuencia y la evidencia no lo sostienen.

### R7. El crash SDL histórico sigue sin causa atribuible

**Confianza alta en la localización del offset en la DLL local; baja en la causa del crash original.**

En la SDL instalada inspeccionada:

- SHA-256: `0a48932c999ff1279c7bfa9e1117f49bf0f52cee4077edaf5858d28c7af93d84`.
- `SDL_mutexP`: RVA `0x28bf0`.
- `0x28c09`: instrucción `mov eax,[eax]` dentro de esa función.
- `SDL_GL_SwapBuffers`: RVA `0x2bd20`.

Eso apunta a la desreferencia de un argumento/objeto de mutex SDL inválido **si la DLL del crash era exactamente ésta**.

No permite distinguir entre puntero caducado, corrupción o llamada incorrecta sin stack, registros y módulos del crash original. El mutex SDL no es el mutex Win32 nominal usado para activar el gate.

No encontré el dump original ni un log local suficiente de aquel intento. **La causa permanece abierta.**

---

## 5. Clusters de problemas

| ClusterProblemas agrupadosIntervención     |                                                                                      |                                                                                      |
| ------------------------------------------ | ------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------ |
| **A — Integración cuerpo/tracking/render** | Pullback físico, petición oscilante, épocas mezcladas, predicción y yaw incoherentes | Transacción por tick y snapshot de tracking identificado                             |
| **B — Movimiento y postura comunes**       | Restricted movement ausente, seated no compartido, crouch/altura incompletos         | Completar políticas Rework y adaptar exclusivamente estados nativos                  |
| **C — Contacto, palma y attachments**      | Alcance/pivote inconsistentes, herramientas desalineadas, hold huérfano              | Un único resultado de contacto/palma consumido por selección, visualización y agarre |
| **D — Ownership y lifecycle**              | Crouch interceptado fuera de VR, estado parcial tras fallo, suspensión insegura      | Ownership explícito, generaciones y rollback verificable                             |
| **E — Presentación experimental**          | Mirror-off negro, foco/Alt+Tab, culling desfasado                                    | Corregir muestra de culling dentro de A; mantener mirror como gate independiente     |
| **F — Evidencia de crash insuficiente**    | APPCRASH SDL histórico                                                               | Captura diagnóstica discriminante; ninguna atribución preventiva                     |

**No agrupo salto, radio del body y glowstick bajo una transformación global incorrecta:** la evidencia no permite hacerlo.

### Tratamiento de hacks y workarounds relevantes

| ElementoDecisión                              |                                                                                                           |
| --------------------------------------------- | --------------------------------------------------------------------------------------------------------- |
| Flags env/mutex y default-off                 | Conservar hasta cerrar gates; identificar fuente de activación                                            |
| Shadow                                        | Conservar como modo observador del mismo coordinador; no mantener dos algoritmos                          |
| Ray corto `0.18 m`                            | Mantener como limitación provisional; sustituir por contrato de contacto cuando exista adapter comprobado |
| Radio nativo BP                               | Conservar por ahora; medir diferencia frente a Overture                                                   |
| Sockets medidos                               | Conservar como perfiles de modelo                                                                         |
| Ajustes temporales de cuerpo durante hold     | Conservar con restauración y ownership verificables                                                       |
| Fallback nativo por ausencia de pose          | Conservar para input nativo; impedir que una adquisición VR lo confunda con contacto VR válido            |
| Estado global de hold sin generación          | Sustituir por ownership de jugador/mundo                                                                  |
| Capturas con hooks instalados                 | Conservar como evidencia, etiquetadas; no tratarlas como imágenes prístinas                               |
| Comentarios/documentación antiguos            | Corregir únicamente donde contradigan el flujo y estado final                                             |
| Código neutral duplicado pero validado en HPL | No eliminar por estética; extracción sólo con equivalencia demostrada                                     |

---
