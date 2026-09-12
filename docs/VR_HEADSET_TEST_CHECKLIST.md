# Black Plague — checklist de validación con visor

Actualizado: 2026-09-12.

La tanda shadow de tracking/body ya está completada y **live-tested** en PID
28172. No la repitas como siguiente gate salvo que aparezca una regresión.

La petición física X/Z collision-aware en `0xD7281` está **implementada y
host-tested**, pero todavía no está live-tested. La traslación posicional del HMD
sigue desactivada y debe permanecer a cero durante esta prueba. El objetivo de
la siguiente sesión con visor es validar únicamente:

`queue -> injection -> native collision resolution -> matched reconciliation`

con casos stationary/free/block/slide y manteniendo un único tick nativo del
cuerpo.

## Próxima prueba obligatoria: physical displacement live gate

Desde la raíz del repositorio ejecuta:

```powershell
tools\Start-BlackPlaguePhysicalDisplacementValidation.ps1
```

El helper valida primero la imagen exact-build, exige un proceso nuevo de
`penumbra.exe`, activa temporalmente el modo de validación y mantiene
`positional_translation_enabled=0`. Mantén abierta la consola durante toda la
sesión. El log queda en:

`%LOCALAPPDATA%\PenumbraVR\logs\black-plague-probe-<PID>.log`

No des por válida la prueba solo porque el juego arranque o se sienta correcto.
El helper debe reconocer los cuatro escenarios y, al cerrar el juego, aprobar
la evidencia completa.

### Escenarios que debes hacer, en este orden

1. **Stationary.** Quédate quieto varios segundos, sin stick y sin intentar
   desplazarte físicamente. La consola debe terminar registrando `stationary`.
2. **Free.** En una zona despejada, desplázate físicamente unos centímetros en
   X/Z sin usar el stick. Repite delante/atrás y lateralmente durante varios
   segundos. Debe aparecer `free` y no debe haber salto del mundo ni movimiento
   vertical añadido.
3. **Blocked.** Sitúate junto a una pared u obstáculo sólido y realiza un pequeño
   desplazamiento físico hacia él, sin stick. El componente que entra en la
   pared debe rechazarse: no atravieses la geometría y no debe producirse un
   desplazamiento brusco del mundo. La consola debe registrar `blocked`.
4. **Slide/partial.** Haz el mismo movimiento en diagonal contra una pared. Debe
   rechazarse parte del movimiento y conservarse una componente tangencial. La
   consola debe registrar `slide/partial`.

Mantén cada caso varios segundos. El helper muestrea telemetría periódica y una
pasada demasiado breve puede no dejar evidencia suficiente aunque el
comportamiento visual parezca correcto.

### Comprobaciones adicionales antes de cerrar

Cuando ya hayan aparecido los cuatro casos:

- mueve con el stick unos segundos y confirma que la locomoción nativa sigue
  funcionando normalmente;
- recentra una vez y repite un caso `free` corto para comprobar que la sesión no
  queda en un estado incoherente;
- juega un par de minutos sin forzar nuevas pruebas y observa si aparecen
  tirones, desplazamientos del mundo, pérdida de tracking o comportamiento de
  colisión claramente distinto del esperado.

No uses esta tanda para validar velocidad final, crouch físico, salto VR,
cámara/bob, manos, agarres, mecanismos, mirror o Enhanced Visuals. Son gates
independientes.

### Qué debe decir el resultado para considerarlo válido

Al cerrar el juego, el helper debe terminar sin error y demostrar en el log
fresco:

- plan físico X/Z no nulo encolado;
- petición consumida e inyectada en `0xD7281`;
- reconciliación emparejada con la misma petición/body generation;
- telemetría corporal con request e injection no nulos;
- tick nativo aproximadamente `dt=1/60`;
- al menos una muestra `stationary`;
- al menos una muestra `free`;
- al menos una muestra `blocked`;
- al menos una muestra `slide/partial`.

Si falta cualquiera de esos puntos, la sesión sigue siendo **incompleta** y el
boundary permanece `host-tested`. Conserva el mensaje final de la consola y el
log del PID; indican exactamente qué evidencia faltó.

Si todos pasan, esta prueba permite promocionar el boundary físico de
`0xD7281` a **live-tested**. No significa todavía que room-scale activo o la
traslación posicional del HMD estén headset-validated; esos son el siguiente
gate una vez incorporado de forma deliberada el consumo activo de esta ruta.

## Prueba separada: presentación, mirror y pérdida de foco

Haz esta tanda en otra sesión usando la ruta normal `Start-Black-Plague-VR.cmd`.
No la mezcles con el physical displacement gate porque sus fallos pertenecen a
otro límite.

1. **Mirror apagado.** Confirma que la ventana del PC permanece negra y estable;
   no debe reaparecer el punto blanco que crecía hasta ocupar casi toda la
   ventana.
2. **Mirror encendido.** Comprueba que la copia del ojo funciona y que alternarlo
   no rompe la presentación del visor.
3. **Alt+Tab durante gameplay.** Cambia a otra ventana y continúa unos segundos
   en el visor. Comprueba mundo, tracking y mandos.
4. **Menú sin recuperar primero el foco.** Abre pausa, inventario/libreta y vuelve
   a gameplay. Anota exactamente qué superficies quedan negras.
5. **Recuperación de foco.** Devuelve el foco al juego y comprueba si los menús se
   recuperan sin reiniciar la sesión.
6. **Salida normal.** Cierra desde el juego y conserva el log para correlacionar
   la transición de foco/presentación.

La regresión de menú negro tras pérdida de foco sigue abierta y no se considera
resuelta por la limpieza del monitor.

## Gates posteriores, solo después del physical displacement live pass

Mantén separados, en este orden aproximado, los siguientes hitos:

1. room-scale activo y traslación posicional del HMD;
2. locomoción VR frente a la referencia probada de Overture (`1.5 / 2.25 m/s`);
3. crouch físico;
4. salto/estado vertical, manteniendo la propiedad nativa ya demostrada;
5. cámara/bob y confort;
6. manos, agarres y geometría de herramientas;
7. mecanismos articulados;
8. presentación/mirror si sigue pendiente;
9. Enhanced Visuals y otros extras no necesarios para el gate corporal.

No promociones ningún sistema por compilación, tests host o análisis estático:
mantén siempre separados `implemented`, `host-tested`, `live-tested`,
`headset-validated` y `supported`.
