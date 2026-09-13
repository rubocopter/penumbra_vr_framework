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

PID 24956 ejecutó por primera vez el camino room-scale activo en visor. El log
contiene 1159 resúmenes corporales: 90 stationary/jitter, 963 free, 10 blocked y
95 slide/partial. También demuestra `1.65 -> 0.95 -> 1.65 m`, aplicación de
cámara tras volver de pie y frames gameplay con `monitor_mirror=1`. El helper
falló únicamente porque exigía repetir también un bloqueo después de levantarse;
después de recuperar la forma sí registró 414 free y 25 slide/partial.

La sesión no valida todavía el resultado visual. El movimiento comunicado
permitió localizar una diferencia respecto a Rework `23c890f`: Black Plague
resuelve stick y petición física dentro de su único tick nativo, y el shadow
arrastraba el ancla con el vector combinado después de haber reconciliado ya la
parte física. La corrección conserva el body final real, resta la parte física
emparejada antes del arrastre reservado a locomoción y expone
`locomotion_carry` en telemetría. Está implementada y compilada; falta repetirla
en juego y visor.

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
5. **Regresión de partición stick/HMD.** Primero camina solo con stick y detente.
   Después mantén el stick hacia delante y desplaza la cabeza unos centímetros
   hacia delante; repite con la cabeza hacia atrás. En el comportamiento de
   referencia los dos vectores reales pueden sumarse si coinciden y reducir el
   avance si se oponen. No debe aparecer un salto adicional de cámara, una
   amplificación mayor que esos dos aportes ni movimiento residual al detener
   ambos. La telemetría nueva debe mostrar `native_accepted` combinado y
   `locomotion_carry` sin la parte `physical_accepted` ya reconciliada.
6. **Recenter.** Ejecuta un recenter, espera unos segundos y repite `free` y
   `blocked`. No debe aparecer un salto persistente, offset antiguo ni pérdida de
   manos.
7. **Cambio nativo de forma.** Agáchate, mantén la postura unos segundos y vuelve
   a levantarte mediante el control normal del juego. El `character_body` se
   conserva mientras su cuerpo físico cambia de `1.65 m` a `0.95 m` y vuelve a
   `1.65 m`. Después da un paso físico claro en cualquier dirección; el helper
   exige la secuencia completa, una muestra room-scale fresca y movimiento
   físico significativo tras volver de pie. Las cuatro clases de colisión solo
   se exigen una vez en el conjunto de la sesión.
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
- al menos una muestra física significativa después de volver de pie.

La aprobación automática demuestra que el camino técnico estuvo activo y dejó
evidencia. Para promoverlo a **headset-validated** también necesito tu resultado
visual de los puntos 1–10.

## Qué devolver después de la prueba

Conserva y comunica:

- PID de la sesión;
- salida final completa del helper;
- si `free`, `blocked` y `slide` se sintieron correctos;
- si hubo doble movimiento, salto del mundo o deriva;
- qué ocurrió al combinar stick y desplazamiento de cabeza en el mismo sentido
  y en sentidos opuestos, y si el movimiento cesó al soltar el stick y volver a
  la postura inicial;
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
prueba del punto 5 usa movimientos pequeños y continuos; los pasos completos ya
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
