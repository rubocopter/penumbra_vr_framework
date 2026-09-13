# Black Plague — checklist de validación con visor

Actualizado: 2026-09-13.

## Estado antes de la siguiente tanda

La ruta física X/Z collision-aware de `0xD7281` está **live-tested** en PID
26144: queue, injection, resolución nativa y reconciliación emparejada, con
`12 stationary/jitter`, `37 free`, `2 blocked` y `15 slide/partial`. El jitter
horizontal de hasta `2 mm` cuenta como stationary; no necesitas mantener el
visor matemáticamente inmóvil.

PID 19192 también aclaró el comportamiento de mirror apagado. En gameplay el
pase de mundo del monitor se suprime y la ventana queda negra; en los menús el
framebuffer 2D del juego continúa visible porque no existe un `RenderWorld` que
suprimir. Ese resultado es coherente con la implementación y no indica por sí
solo un fallo.

PID 21548 repitió el camino activo después de corregir la partición del tick
combinado. El helper terminó correctamente: `queued=201 consumed=201
injected=201 matched=201`, 92 stationary, 477 free, 1 blocked, 45 slide/partial,
mirror encendido, secuencia `1.65 -> 0.95 -> 1.65 m` y recuperación posterior.
La inclinación de cabeza en el sitio ya no movió al personaje, por lo que esa
regresión concreta queda confirmada como corregida en visor.

La sesión todavía **no** valida room-scale. El mundo se veía continuamente como
en un pequeño terremoto y el rechazo al acercar la cabeza a una pared seguía
siendo agresivo. En 539 frames muestreados sin stick, el offset X/Z tuvo una
mediana de 9,65 mm y cambió 15,77 mm entre muestras; 430 ventanas invirtieron al
menos una dirección. Solo 4 de 64 resúmenes periódicos contenían rechazo físico,
así que el temblor no dependía únicamente de estar junto a una pared.

La comparación con Rework `23c890f` encontró la diferencia de presentación:
Rework coloca la vista desde su ancla VR a 90 Hz y no hereda el suavizado/bob de
posición de la cámara nativa. Black Plague retenía una muestra corporal de ~60
Hz y sumaba el offset sobre esa cámara. La nueva corrección coloca X/Z
directamente en el ancla reconciliada y, entre ticks físicos, la continúa con el
delta HMD más reciente. Cámara, visibilidad y manos usan la misma colocación. Y,
salto y altura física siguen siendo nativos. Compila en las dos configuraciones
Release, pero aún no se ha probado live ni con visor.

## Siguiente tanda — room-scale activo, colisiones y mirror encendido

Desde la raíz del repositorio ejecuta:

```powershell
tools\Start-BlackPlagueRoomScaleValidation.ps1
```

El helper:

- valida la imagen exact-build antes de iniciar;
- exige un proceso nuevo de `penumbra.exe`;
- activa los mutex transitorios de physical displacement y room-scale;
- guarda `MonitorMirror=true` antes del arranque;
- rechaza la sesión si el probe no confirma los dos modos;
- al cerrar el juego analiza únicamente el log fresco de ese PID.

Mantén abierta la consola durante toda la sesión. El log queda en:

`%LOCALAPPDATA%\PenumbraVR\logs\black-plague-probe-<PID>.log`

### Pruebas obligatorias

1. **Baseline quieto, gate principal.** Ya dentro de una partida y en una zona
   despejada, no uses sticks durante 10–15 segundos. Mantén una postura normal;
   el movimiento mínimo del visor es esperado. El mundo debe quedar estable, sin
   vibración rítmica, saltos de centímetros ni el “pequeño terremoto” de PID
   21548. Si sigue ocurriendo, anota si es horizontal, vertical o ambos.
2. **Rotación e inclinación en el sitio.** Sin desplazar pies ni torso, mira a
   izquierda/derecha, arriba/abajo e inclina lateralmente la cabeza. El mundo y
   la cámara deben rotar, pero el personaje no debe empezar a andar, avanzar,
   retroceder ni desplazarse lateralmente de forma apreciable. Anota por separado
   cualquier bob, paso, deriva o movimiento corporal. Esta es la regresión
   principal de la tanda.
3. **Free X/Z lento, gate de 60/90 Hz.** Desplaza cabeza y torso lentamente y de
   forma continua unos centímetros hacia delante, atrás y ambos lados, sin
   stick. El punto de vista debe seguir el gesto de forma continua, sin escalones
   a ~60 Hz, tirones ni oscilación al detenerte. El cuerpo nativo debe recuperar
   la separación sin mover la vista dos veces.
4. **Blocked y confort.** Acércate lentamente a una pared y desplázate
   físicamente hacia ella. La
   cámara no debe atravesarla ni permitir que la cabeza gane distancia ilimitada
   respecto al cuerpo. Mantén el caso varios segundos hasta que la consola
   anuncie `blocked`. Describe la intensidad del retroceso: debe impedir el paso
   sin alejar el mundo de forma brusca o repetitiva.
5. **Slide/partial.** Muévete físicamente en diagonal contra la pared. Debe
   conservarse la componente tangencial y rechazarse la componente que entra en
   la geometría. Espera a que la consola anuncie `slide/partial`.
6. **Partición stick/traslación HMD.** Primero camina solo con stick y detente.
   Después mantén el stick hacia delante y desplaza deliberadamente cabeza y
   torso unos centímetros hacia delante; repite trasladándolos hacia atrás. Solo
   aquí existen dos desplazamientos reales que pueden combinarse. No debe
   aparecer un tercer aporte, salto de cámara ni movimiento residual al detener
   ambos. La telemetría debe mostrar `native_accepted` combinado y
   `locomotion_carry` sin la parte `physical_accepted` ya reconciliada.
7. **Recenter.** Ejecuta un recenter, espera unos segundos y repite `free` y
   `blocked`. No debe aparecer un salto persistente, offset antiguo ni pérdida de
   manos.
8. **Cambio nativo de forma.** Agáchate, mantén la postura unos segundos y vuelve
   a levantarte mediante el control normal del juego. El `character_body` se
   conserva mientras su cuerpo físico cambia de `1.65 m` a `0.95 m` y vuelve a
   `1.65 m`. Después da un paso físico claro en cualquier dirección; el helper
   exige la secuencia completa, una muestra room-scale fresca y movimiento
   físico significativo tras volver de pie. Las cuatro clases de colisión solo
   se exigen una vez en el conjunto de la sesión.
9. **Manos y coherencia espacial.** Mira ambas manos mientras inclinas la cabeza
   y durante un movimiento lateral. Las palmas no deben quedarse en el anclaje
   anterior ni separarse del punto de vista.
10. **Mirror y menús.** Confirma que el monitor muestra gameplay con mirror
   encendido. Abre pausa, inventario/libreta y vuelve al juego. Haz un Alt+Tab,
   abre esos menús antes de devolver el foco y anota qué superficie queda negra,
   si ocurre. Devuelve el foco y comprueba si se recupera.
11. **Estabilidad breve.** Solo si los puntos 1–4 son cómodos, juega 2–3 minutos
    y vigila tirones, world wobble,
    clipping, pérdida de tracking, desajuste de manos o colisiones distintas de
    las observadas antes.

No uses esta tanda para juzgar velocidad final `1.5/2.25 m/s`, crouch físico por
altura real, salto VR, bob vertical/altura, agarres, mecanismos o Enhanced
Visuals. Siguen siendo gates independientes. La estabilidad horizontal de cámara
sí pertenece a esta tanda porque es la corrección que se valida. Debes usar el
crouch nativo una vez para probar la sustitución del body bajo room-scale.

### Resultado automático exigido

Al cerrar el juego, el helper solo termina con éxito si el log fresco demuestra:

- plan físico X/Z no nulo encolado;
- petición consumida e inyectada en `0xD7281`;
- reconciliación emparejada con el mismo body y generación;
- tick nativo aproximado `dt=1/60`;
- muestras `stationary`, `free`, `blocked` y `slide/partial`;
- sample room-scale fresco aplicado a la cámara;
- offset horizontal de cámara no nulo observado;
- offset reconciliado no nulo y continuación HMD entre ticks registrada en
  `room_scale_reconciled_offset_m` y `room_scale_render_prediction_m`;
- mirror activo en un frame de gameplay;
- secuencia nativa de altura `1.65 m -> 0.95 m -> 1.65 m` y nueva aplicación
  room-scale tras volver de pie;
- al menos una muestra física significativa después de volver de pie.

La aprobación automática demuestra que el camino técnico estuvo activo y dejó
evidencia. Para promoverlo a **headset-validated** también necesito tu resultado
visual de los puntos 1–11.

## Qué devolver después de la prueba

Conserva y comunica:

- PID de la sesión;
- salida final completa del helper;
- si `free`, `blocked` y `slide` se sintieron correctos;
- si hubo doble movimiento, salto del mundo o deriva;
- si desapareció el temblor continuo de PID 21548 y si cualquier resto fue
  horizontal, vertical o ligado únicamente a locomoción;
- si rotar/inclinar la cabeza en el sitio produjo cualquier desplazamiento del
  personaje, separado del lean/step deliberado;
- qué ocurrió al combinar stick y traslación física deliberada en el mismo
  sentido y en sentidos opuestos, y si el movimiento cesó al soltar el stick y
  volver a la postura inicial;
- si las manos siguieron a la cabeza;
- resultado del mirror en gameplay y menús;
- resultado de Alt+Tab y recuperación de foco;
- cualquier diferencia antes y después de crouch/stand y recenter.

Si el helper falla, no repitas a ciegas. El mensaje enumera la evidencia ausente;
conserva ese texto y el log del PID para separar un escenario no capturado de un
fallo real de cámara, reconciliación, ownership o presentación.

Los movimientos grandes causados al quitarse parcialmente el visor no sirven
para juzgar sensibilidad ni comfort, pero tampoco invalidan las muestras
collision-aware: el límite sigue acotando cada petición a `0.05 m`. Para la
prueba del punto 6 usa movimientos pequeños y continuos; los pasos completos ya
aparecen impresos en la propia terminal para evitar consultar otra pantalla.

## Gate anterior — repetir solo ante regresión

La prueba sin traslación activa sigue disponible:

```powershell
tools\Start-BlackPlaguePhysicalDisplacementValidation.ps1
```

Esa ruta mantiene `positional_translation_enabled=0` y sirve para aislar una
regresión en `0xD7281`. No es la siguiente tanda normal porque PID 26144 ya la
promocionó a **live-tested**.

Mantén separados los estados `implemented`, `host-tested`, `live-tested`,
`headset-validated` y `supported`. El código nuevo permanece en `implemented`
hasta ejecutar las pruebas host y completar la sesión con visor descrita arriba.
