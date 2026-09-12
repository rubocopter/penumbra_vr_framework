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

El nuevo camino de room-scale activo está **implemented** y sus targets Release
afectados compilan. No se han ejecutado sus tests y todavía no está
host/live/headset-validated. Solo se activa
durante esta prueba y exige simultáneamente la ruta física probada. Aplica X/Z
reconciliado a cámara, culling y base de las manos; no añade Y, no cambia salto
y no crea otro tick de física.

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

### Pruebas obligatorias, en este orden

1. **Baseline quieto.** Ya dentro de una partida y en una zona despejada, no
   uses sticks durante 5–10 segundos. Mantén una postura normal; el movimiento
   mínimo del visor es esperado. Confirma que el mundo no deriva ni tiembla.
2. **Free X/Z.** Desplaza la cabeza y el torso unos centímetros hacia delante,
   atrás y ambos lados, sin stick. El punto de vista debe acompañar el movimiento
   de forma natural y el personaje debe recuperar la separación mediante su
   cuerpo nativo sin saltos visibles.
3. **Blocked.** Acércate a una pared y desplázate físicamente hacia ella. La
   cámara no debe atravesarla ni permitir que la cabeza gane distancia ilimitada
   respecto al cuerpo. Mantén el caso varios segundos hasta que la consola
   anuncie `blocked`.
4. **Slide/partial.** Muévete físicamente en diagonal contra la pared. Debe
   conservarse la componente tangencial y rechazarse la componente que entra en
   la geometría. Espera a que la consola anuncie `slide/partial`.
5. **Locomoción con stick.** Camina y gira durante al menos 20 segundos. Repite
   un pequeño desplazamiento físico mientras avanzas. Comprueba que no se suma
   dos veces el movimiento, que no aparece deriva y que la dirección relativa al
   HMD sigue siendo coherente.
6. **Recenter.** Ejecuta un recenter, espera unos segundos y repite `free` y
   `blocked`. No debe aparecer un salto persistente, offset antiguo ni pérdida de
   manos.
7. **Cambio nativo de forma.** Agáchate, mantén la postura unos segundos y vuelve
   a levantarte mediante el control normal del juego. El `character_body` se
   conserva mientras su cuerpo físico cambia de `1.65 m` a `0.95 m` y vuelve a
   `1.65 m`. Después repite `free`, `blocked` y `slide/partial`; el helper exige
   la secuencia completa y una muestra room-scale fresca tras volver de pie.
8. **Manos y coherencia espacial.** Mira ambas manos mientras inclinas la cabeza
   y durante un movimiento lateral. Las palmas no deben quedarse en el anclaje
   anterior ni separarse del punto de vista.
9. **Mirror y menús.** Confirma que el monitor muestra gameplay con mirror
   encendido. Abre pausa, inventario/libreta y vuelve al juego. Haz un Alt+Tab,
   abre esos menús antes de devolver el foco y anota qué superficie queda negra,
   si ocurre. Devuelve el foco y comprueba si se recupera.
10. **Estabilidad breve.** Juega 2–3 minutos y vigila tirones, world wobble,
    clipping, pérdida de tracking, desajuste de manos o colisiones distintas de
    las observadas antes.

No uses esta tanda para juzgar velocidad final `1.5/2.25 m/s`, crouch físico por
altura real, salto VR, bob/cámara, agarres, mecanismos o Enhanced Visuals. Siguen
siendo gates independientes. Sí debes usar el crouch nativo una vez para probar
la sustitución del body bajo room-scale.

### Resultado automático exigido

Al cerrar el juego, el helper solo termina con éxito si el log fresco demuestra:

- plan físico X/Z no nulo encolado;
- petición consumida e inyectada en `0xD7281`;
- reconciliación emparejada con el mismo body y generación;
- tick nativo aproximado `dt=1/60`;
- muestras `stationary`, `free`, `blocked` y `slide/partial`;
- sample room-scale fresco aplicado a la cámara;
- offset horizontal de cámara no nulo observado;
- mirror activo en un frame de gameplay;
- secuencia nativa de altura `1.65 m -> 0.95 m -> 1.65 m` y nueva aplicación
  room-scale tras volver de pie;
- nuevas muestras `free`, `blocked` y `slide/partial` después de volver de pie.

La aprobación automática demuestra que el camino técnico estuvo activo y dejó
evidencia. Para promoverlo a **headset-validated** también necesito tu resultado
visual de los puntos 1–10.

## Qué devolver después de la prueba

Conserva y comunica:

- PID de la sesión;
- salida final completa del helper;
- si `free`, `blocked` y `slide` se sintieron correctos;
- si hubo doble movimiento, salto del mundo o deriva;
- si las manos siguieron a la cabeza;
- resultado del mirror en gameplay y menús;
- resultado de Alt+Tab y recuperación de foco;
- cualquier diferencia antes y después de crouch/stand y recenter.

Si el helper falla, no repitas a ciegas. El mensaje enumera la evidencia ausente;
conserva ese texto y el log del PID para separar un escenario no capturado de un
fallo real de cámara, reconciliación, ownership o presentación.

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
