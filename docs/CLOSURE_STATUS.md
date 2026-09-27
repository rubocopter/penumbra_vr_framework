# Penumbra VR Framework: estado de cierre

Actualizado: 2026-09-27. Fuente de verdad para la secuencia de cierre. `SUPPORTED_BUILDS.md` registra evidencia por build; `TRILOGY_PARITY_PLAN.md` y `ROADMAP.md` registran capacidades y trabajo técnico.

## Hito actual

**Hito 2: Requiem.** Hito 0 auditado; Hito 1 delimitado por pruebas de visor y evidencia runtime pendientes. El agente no ejecuta juegos ni SteamVR. Installer, packaging y candidatos de release están congelados hasta Hito 4. El bloqueo activo es la paridad de adquisición/manipulación física de Requiem. El lote posterior a RQ-14 porta el resolvedor de palma y el ciclo de vida de agarre de Black Plague; está validado sólo en host y requiere un único gate de visor antes de ampliar el scope.

## Estado de los juegos

| Sistema | Overture | Black Plague | Requiem |
| --- | --- | --- | --- |
| Arranque, menú y gameplay | Validado antes en PSVR2; build actual requiere regresión | BP-01 llegó a gameplay sin crash; arranque completo pendiente | RQ-03 permitió boot, menú y gameplay con visor |
| Estéreo, cámara y tracking | Validado antes; regresión pendiente | Gameplay observado; rayos y humo defectuosos | RQ-02 corrigió zonas sin renderizar al girar sólo la cabeza; RQ-04 confirmó mirror |
| Locomoción, crouch y salto | Validado antes; regresión pendiente | Validación parcial anterior; cambio final de crouch requiere visor | Sense, movimiento físico, paredes y crouch físico observados; salto mejor tras ajuste local. El helper actual fija 1,5 m/s de marcha y 4,5 m/s de sprint y está host-tested; la sensación final requiere visor |
| Manos y herramientas | Validado antes | Validación parcial; mano fullbright observada | Manos visibles; herramientas aún pueden desplazarse respecto a la mano |
| Interacción y física | Agarre próximo, alcance a distancia para ítems, mecanismos y lanzamiento validados antes | Validación parcial; fixes recientes requieren regresión | **Bloqueador de visor:** RQ-13 mostró agarre difícil. El lote actual porta colisión/resolución de palma, adquisición por solape antes del rayo, punto de superficie, exclusión del cuerpo sostenido, refresh síncrono de palma y continuidad Grab/Move con snap; 44/44 host tests, sin validación runtime todavía |
| UI, subtítulos y diario | Validado antes; regresión pendiente | UI/cursor y subtítulos requieren regresión | Menú, inventario y libreta visibles; subtítulos/diario demasiado grandes en RQ-13. Plano reencuadrado en RQ-14, pendiente de visor |
| Transiciones, guardado/carga y cierre | Ejercitados antes; build actual pendiente | Parcialmente ejercitados; guardado/carga desconocido; crash SDL histórico | Menú→gameplay observado; transición de nivel, guardado/carga y cierre sin validar |

El runtime actual usa **OpenVR mediante SteamVR**. La mención de OpenXR en el objetivo original no prueba un backend OpenXR nativo. La evidencia de visor no se transfiere automáticamente entre builds.

## Evidencia de visor y bloqueadores

- **BP-01:** el usuario jugó sin crash y aportó `C:/Users/onita/Videos/clip_1.790.421.411.686.mp4`. Se ve un rectángulo central en el efecto de sigilo y fondo rectangular oscuro en vapor/humo; rayos rojos/azules ya estaban documentados como defectuosos. No se presume una causa común. Los minidumps SDL existentes no bastan para atribuir causa; no repetir arranques para provocarlo. Si reaparece espontáneamente, volcado completo y log serían evidencia discriminante.
- **RQ-08/RQ-09:** movimiento físico y con stick, pared, crouch, manos y menú/inventario/libreta funcionaron en las rutas observadas. R2 no adquiría objetos. Hubo caídas de cadencia en algunas pruebas, sin causa atribuida. El salto mejoró en RQ-11.
- **RQ-10 a RQ-13:** vídeos en `C:/Users/onita/Videos`, incluido `testagarre.mp4` y el último clip de piedra/subtítulos `clip_1.790.504.413.806.mp4`. El usuario no pudo manipular piedras/cuadernos de forma jugable; una piedra pareció impulsarse al retroceder con stick. El log `requiem-probe-23032.log` registró candidatos de masa 26 a 0,007–0,101 m, pero `native_grab_enter=native_move_enter=0` en toda la sesión. El impulso no procede de la ruta VR Grab/Move en esa prueba. Subtítulos y animación de diario eran demasiado grandes/cercanos.
- **RQ-14:** se añadió anclaje al punto de superficie tras aceptación nativa, se reencuadró el plano conjunto de texto/animación a 2,5 m y se añadió telemetría de selección/aceptación. Fue el último candidato probado sólo en host antes del lote de paridad de interacción actual.
- **Lote de paridad de interacción posterior a RQ-14, sólo host-tested:** Requiem crea su propio box de palma mediante la ABI exacta del build y resuelve la posición contra el mundo en el único tick nativo ya propietario del character body. La selección intenta primero solape de palma con la elegibilidad nativa y conserva el punto real de contacto. `Grab` y `Move` publican el cuerpo sostenido como exclusión antes de refrescar la palma, esperan una generación nueva, limpian esa propiedad al fallar/salir y usan el mismo epoch de yaw que el resolvedor para el snap turn. `Move` deja de depender de la palma cruda/stale que diferenciaba Requiem de Black Plague. Build Release y **44/44** pruebas host pasaron. Con juego/SteamVR cerrados se respaldó la DLL previa como `work/rq01-stage/requiem-probe.pre-interaction-parity.dll` (SHA-256 `535F31C5F9524BB6A45B09E1528567D3E2F501307C8A9A3436D7B94B37E2C6C3`) y se desplegó únicamente `PenumbraVR.Requiem.Probe.dll`; build e instalación coinciden en SHA-256 `FB91F2F16EF735B49C772B49BA28908F3CE0FC17CA19BE7CEDC29A450770C162`. Esto no demuestra aún agarre jugable en visor.

## Investigación offline del primer bloqueo

La imagen inicializada canónica `local/captures/requiem-steam-observed-memory-text.exe` demuestra que `Normal::Update` llama al rayo; el callback nativo copia el candidato aceptado a `pick+4`; `cPlayer::GetPickedBody` lee `player+0x280 → pick+4`; y `Normal::OnStartInteract` (`0xADFA0`) entrega la entidad de ese cuerpo a `PlayerInteract`. En RQ-13, `winner=1` sólo indicaba un candidato VR reenviado, **no** que el callback nativo lo aceptara. La comparación posterior con Black Plague aisló otra diferencia material una vez iniciada la interacción: Requiem todavía usaba palma cruda/stale en parte de `Move`, no excluía el cuerpo sostenido de la colisión de mano y no exigía un refresh posterior a esa exclusión. El lote actual porta ese ciclo de vida probado y mantiene las fuerzas/mecánicas nativas de mecanismos.

El alcance magnético probado en Rework/Overture y adaptado en BP se limita a **ítems de inventario**: cono de mano, alcance por subtipo y visibilidad sólida desde mano/cabeza. No se aplica a piedras, puertas ni mecanismos. Requiem necesita demostrar su lista de cuerpos, volumen y subtipo exactos antes de consumir esa política; un rayo largo genérico podría recoger a través de una pared. Permanece en Hito 2 después de resolver la selección próxima.

## Validación y próximo gate único

Automático: identidad del ejecutable, patrones/slots de imagen inicializada, compilación, contratos host de tracking/locomoción/contacto/render y hashes de DLL. Esto no demuestra interacción física ni comodidad de lectura. Visor/runtime: adquisición y manipulación real, alcance desde superficie, continuidad con stick y snap, ítems/cuadernos, subtítulos, herramientas, pacing, transición/carga, guardado y cierre.

**Próximo gate único — paridad de interacción:** en un único arranque, probar una piedra y el diario/ítem desde borde, superficie o suelo; mantener uno agarrado mientras se da un paso corto con stick; hacer un snap turn; soltar/lanzar con un gesto corto; y, si hay uno a mano, probar una puerta/cajón/palanca representativa. Informar sólo de: facilidad para adquirir desde el borde, si la mano se bloquea/desliza contra el mundo de forma comparable a Overture/BP, si el objeto permanece unido al moverse/girar y si el lanzamiento/mecanismo responde de forma estable. Si la adquisición falla o el objeto se propulsa, adjuntar el último `%LOCALAPPDATA%/PenumbraVR/logs/requiem-probe-*.log`. No ampliar la prueba a otros sistemas hasta superar o delimitar este gate.

## Auditoría del exceso de candidatos

La auditoría inicial encontró `main` y checkouts auxiliares limpios, 24 commits desde `origin/main` y **32 ZIP locales** en `build/release`: 17 Framework, 8 Overture y 7 Black Plague. Los scripts `Package-FrameworkCandidate.ps1`, `Package-OvertureCandidate.ps1` y `Package-BlackPlagueCandidate.ps1` los generaron con sufijos de commit; varios ZIP tenían contenido idéntico. Esto es historia de packaging, no evidencia de gameplay. Se conservaron identificación exacta de builds, restore/rollback, logs y checks útiles; no se revirtieron cambios válidos. Los cambios de runtime valiosos incluyen supresión del log síncrono por draw en Overture y corrección de rechazo tardío de stand en BP. No se demostró una regresión causada por aquellos commits. `main` estaba limpio al retomar la presente sesión.

| ZIP | Sufijos de commit inventariados en la auditoría |
| --- | --- |
| Framework (17) | `03a36a3`, `07ce96f`, `2f43567`, `52bfa2f`, `583b98a`, `74fe342`, `794955b`, `7deb1d9`, `7fa158d`, `8137c15`, `8a45b4a`, `98f6011`, `a745f90`, `c0bd099`, `dec7add`, `dece881`, `e5ebd29` |
| Overture (8) | `03a36a3`, `07ce96f`, `74fe342`, `7deb1d9`, `8137c15`, `c0bd099`, `dec7add`, `e5ebd29` |
| Black Plague (7) | `03a36a3`, `07ce96f`, `583b98a`, `76a145c`, `7deb1d9`, `8137c15`, `dece881` |

## Secuencia obligatoria

1. **Hito 0 — cerrado:** auditoría y conservación de cambios útiles.
2. **Hito 1 — delimitado:** Overture/BP conservados; BP-01 sin crash. Regresiones de visor y fallo SDL histórico necesitan evidencia runtime; no prolongar la reproducción.
3. **Hito 2 — actual:** hacer jugable la interacción física de Requiem con su estado nativo y el comportamiento probado de Overture/BP; portar alcance magnético sólo a ítems cuando el límite exacto esté probado. Detenerse ante la evidencia que requiera visor.
4. **Hito 3 — después:** batería pequeña de regresión para boot/UI, gameplay, tracking, locomoción/crouch, manos, interacción, transición/carga y estabilidad, con evidencia por juego.
5. **Hito 4 — después:** revisar el instalador prototipo y cerrar una sola implementación de selección múltiple, validación, dependencias, reparación, actualización, restore/uninstall y logs.
6. **Hito 5 — final:** tests, release reproducible, README, problemas conocidos y candidato final; distinguir automático, hardware, pendiente y aceptado.
