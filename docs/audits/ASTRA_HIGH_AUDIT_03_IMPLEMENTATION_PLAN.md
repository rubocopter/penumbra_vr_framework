## 6. Plan de implementación definitivo

### Fase 1 — Proteger ownership de input y postura

**Dependencia:** ninguna. Debe poder entregarse sin cambiar locomoción ni render.

**Componentes:**

- `native_input_bridge.*`
- `vr_crouch_policy.*`
- tests de input y nuevo harness del bridge real.

**Cambios:**

1. Introducir ownership explícito: nativo, VR activo y liberación pendiente.
2. Asociarlo a sesión y generación del jugador.
3. Devolver el resultado nativo de crouch cuando VR no sea owner.
4. Adoptar pulsaciones nativas en el latch común únicamente cuando VR sea owner.
5. Aplicar la liberación en el game thread.
6. Distinguir deseo de crouch, move state efectivo y bloqueo de stand.
7. Invalidar o transferir explícitamente el estado al cambiar jugador, mapa o sesión; no reutilizarlo por coincidencia de puntero.
8. Añadir a la política común el feedback de stand bloqueado necesario para preservar el baseline de Rework.

**Reutilizar:** latch, umbrales, histéresis y `ChangeMoveState(4/0)` actuales.

**Eliminar:** interceptación incondicional y cualquier compensación residual mediante toggles.

**Tests de contrato:**

- Bridge instalado sin sesión: crouch nativo intacto.
- VR activo: una pulsación, una transición lógica.
- Focus/UI/disconnect: política y ownership explícitos.
- Cambio de jugador: ningún estado antiguo aplicado al nuevo.
- Techo bajo: baseline estable y stand reintentado al recuperar espacio.

**Aceptación:** input nativo conservado fuera de VR; estado `4/0` aplicado sólo por el owner correcto.

### Fase 2 — Hacer transaccional el lifecycle

**Dependencia:** independiente de la fase 3; completar antes de validar repetidamente arranques/paradas.

**Componentes:** probe, hooks y respuestas del launcher.

**Cambios:**

1. Registrar componentes instalados, activos y desmontados.
2. Publicar `ready` sólo cuando esté completo el conjunto requerido.
3. Si rollback falla, conservar estado parcial explícito y capabilities reales.
4. No cerrar la observabilidad mientras puedan quedar callbacks propios.
5. Hacer shutdown reintentable desde un estado parcial.
6. Preparar handles y almacenamiento antes de suspender threads.
7. Evitar asignaciones, logs y formateo de errores durante el intervalo suspendido.
8. Tratar timeout remoto como resultado indeterminado, no como prueba de que no ocurrió nada.

**Reutilizar:** verificaciones de bytes, publicación previa de originales, contadores y protección de llamadas existentes.

**Eliminar:** retornos falsos a “limpio/listo” y asignaciones en la región suspendida.

**Tests:** fallo inyectado en cada instalación y retirada; callback durante inicialización; segundo initialize; shutdown parcial y retry; ownership de originales y memoria retenida.

**Aceptación:** el estado publicado describe siempre los hooks realmente residentes.

### Fase 3 — Restaurar una transacción coherente de movimiento por tick

**Dependencia:** baseline y tests de contrato; no necesita cambios de mirror ni mecanismos.

**Componentes:**

- `body_collision_probe.*`
- `black_plague_body_callbacks.hpp`
- `body_adapter_boundary.*`
- `black_plague_body_adapter.*`
- `body_reconciliation_shadow.*`
- `native_input_bridge.*`
- primitivas existentes de `vr_locomotion`.

**Contrato propuesto:**

```
Antes del único update nativo:
  resolver identidad y B0
  obtener último snapshot de tracking publicado
  integrar una sola vez el delta de esa pose
  consumir intención lógica de locomoción
  calcular petición usando dt físico
  asociar petición a tick + cuerpo + generación + época de tracking

Dentro de D7281:
  consumir esa petición una sola vez
  preservar Y y solver nativos

Después del update:
  observar B1
  comprobar correspondencia de petición
  reconciliar una vez
  aplicar carry una vez
  publicar resultado de cuerpo y ancla
```

No bloquear el tick físico esperando al compositor: consume la muestra publicada más reciente y su identidad.

**Cambios:**

- Añadir preparación antes de `g_original_update` en el wrapper existente.
- Reubicar la planificación actualmente post-tick.
- Publicar desde input intención/permiso, no metros ya integrados con otro `dt`.
- Introducir secuencias monotónicas independientes de los acumuladores de telemetría.
- Mantener shadow como ejecución observadora del mismo planner.
- Conservar clamp combinado `0.05 m`, cuerpo/generación y prioridad de ejes nativos.
- Mantener explícita la distinción entre aceptación total observada y reparto físico/stick estimado.

**Reutilizar:** `PlanBodyReconciliation`, `ReconcilePhysicalBodyMotion`, `CarryHeadAnchorWithLocomotion` y frontera `D7281`.

**Eliminar:** el plan del siguiente tick calculado contra `body_before` de un tick terminado.

**Tests imprescindibles:**

- Rampa HMD continua libre.
- Movimiento corto seguido de parada.
- Render 72/90/120 Hz con cuerpo a 60 Hz.
- Repetición y pérdida de muestras.
- Pared, slide y jitter menor de 2 mm.
- Stick y físico simultáneos, incluidos vectores opuestos.
- Conservación del desplazamiento total al repartir aceptación.
- Recenter, body replacement y pose caducada.
- Una llamada nativa y una inyección como máximo por tick.

**Aceptación:** entrada uniforme libre no genera el patrón de acumulación/cancelación descrito en R1; no se integra dos veces una muestra o intención.

**Límite explícito:** una resolución combinada no proporciona dos resoluciones físicas independientes. No falsear esa equivalencia con nombres de telemetría ni añadir un segundo `D6E00`.

### Fase 4 — Unificar muestras y completar movimiento/postura comunes

**Dependencia:** fases 1 y 3.

**Componentes:** tracking types, sesión/input OpenVR, renderer BP, backend Overture y source integration.

**Cambios:**

1. Extender los tipos existentes con identidad de muestra, tiempo, generación de tracking y época de yaw.
2. Adquirir/publicar la muestra de presentación en el owner anterior de visibilidad; reutilizarla en los ojos.
3. Proporcionar fallback explícito para menús o frames sin ese owner.
4. Separar lectura de poses de consumo de edges de acciones: compartir poses no debe duplicar input.
5. Cámara, manos, herramientas y picking deben declarar qué muestra y transformación consumen.
6. Predicción: añadir sólo delta HMD posterior a la muestra del body, dentro de la misma época de yaw.
7. Extraer `UpdatePlayMode` hacia política común y conectarla a BP.
8. Completar la extracción de crouch conservando el feedback de techo y la aplicación nativa en adapters.
9. Mapear estados BP a restricciones de locomoción mediante evidencia exacta.
10. Dar a VR un owner coherente de tracking-world-yaw; adaptar cambios nativos externos con una regla explícita de delta/rebase.

**Yaw debe tener un gate independiente.** No cambiar simultáneamente su ownership y declarar que el protocolo anterior demuestra su corrección.

**Reutilizar:** fórmulas exactas de Rework y semántica de Overture actual.

**Eliminar:** cálculo independiente de postura/yaw por consumidor y comentarios que describen un orden de render inexistente.

**Tests:**

- Un metro entre dos poses sigue siendo un metro tras yaw/recenter.
- Calibración Y no escala la distancia mano–cabeza.
- Culling y ambos ojos consumen el mismo frame.
- Giro más stick en el mismo update usa la orientación definida por contrato.
- Seated y crouch coinciden con trazas de la referencia.
- Movimiento restringido usa `0.5 m/s` sólo en estados equivalentes comprobados.

**Aceptación:** Overture conserva resultados; BP utiliza la misma política, con diferencias de aplicación únicamente en el adapter.

### Fase 5 — Completar contacto/palma y lifecycle de interacción

**Dependencia:** snapshot de fase 4 y ownership de fase 2.

**Componentes:** `spatial_interaction.*`, `vr_grab_pose.*`, políticas de mano y source adapter.

**5A. Cambios implementables con la evidencia actual**

- Unificar la referencia grip/palma usada por selección, adquisición y visualización.
- Revalidar al adquirir: identidad de selección, contacto, distancia y validez de pose.
- No convertir un fallback de cámara nativa en una adquisición VR válida.
- Dibujar el alcance real de la selección.
- Ligar hold a generación del jugador/mundo.
- Resolver terminación de hold antes de perder ownership cuando sea posible.
- Si el mundo ya destruyó el cuerpo, descartar estado mediante evidencia de destrucción; nunca restaurar campos en memoria caducada.
- Mantener sockets medidos y definir la relación grip→palma→modelo en perfiles.
- Conservar la articulación BP y añadir el cierre por herramienta donde esté respaldado por su geometría.

**5B. Port fiel del contacto de Rework**

Referencia concreta:

- `Player.cpp::CheckVRHandWorldCollision`
- `FindVRHandRecoveryPose`
- actualización de colisión de manos;
- callbacks y tolerancias asociados.

Extraer matemática, selección de contactos y recuperación neutral. Mantener creación de shapes, consultas HPL y acceso a cuerpos en adapters.

**Frontera que todavía necesita evidencia:** la API binaria BP equivalente a `CheckShapeWorldCollision`, sus callbacks, creación/lifetime de shape y filtros. La auditoría no ha demostrado esa ABI.

Antes de ejecutar esa llamada en BP:

1. Localizar su equivalente a partir de implementación HPL y xrefs del ejecutable soportado.
2. Documentar firma, ownership y campos mínimos.
3. Añadir comprobación de imagen y harness sintético.
4. Validar la query sin escrituras de gameplay.
5. Conectar el algoritmo común.

Esto es investigación binaria **acotada**, no redescubrimiento del body adapter. Inventar una dirección para hacer el plan parecer completo sería incorrecto.

**Mecanismos:** conservar el path nativo. No presentar free-body como soporte de puertas, levers o joints. Su adaptación necesita clasificación y estados nativos propios.

**Tests:**

- Contacto local permanece sobre la superficie al rotar un cuerpo largo.
- Aim/grip separados no amplían el alcance de palma.
- Mano contra pared: visual, selección y hold consumen la misma palma resuelta.
- Body replacement no deja hold bloqueando adquisiciones.
- Release restaura propiedades una vez y no escribe en un cuerpo destruido.
- Socket y articulación se verifican por geometría, no por offsets de prueba y error.

**Aceptación:** desaparecen las discrepancias de contacto demostrables; la equivalencia completa de palma sólo se declara tras validar 5B.

### Fase 6 — Validación integrada y promoción de estado

**Dependencia:** fases anteriores según componente.

- Ejecutar baseline completo.
- Extender verificador y helpers para los contratos nuevos.
- Registrar build/hash y resultado por gate.
- Actualizar documentación contradictoria.
- Mantener separado `implemented`, `host-tested`, `live-tested`, `headset-validated` y `supported`.
- No activar room-scale por defecto hasta cerrar movimiento físico, postura y yaw.
- Mirror y jump conservan gates independientes.

---
