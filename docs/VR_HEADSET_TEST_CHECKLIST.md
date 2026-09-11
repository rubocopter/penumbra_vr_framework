# Black Plague — checklist de validación con visor

Actualizado: 2026-09-12.

La tanda shadow de tracking/body ya está completada y live-tested en PID 28172.
No la repitas como siguiente gate salvo que aparezca una regresión. La traslación
posicional del HMD sigue desactivada porque todavía falta una petición física X/Z
collision-aware en metros separada del movimiento analógico nativo.

## Próxima tanda disponible: presentación y foco

Usa la ruta normal `Start-Black-Plague-VR.cmd`. Los logs se guardan en
`%LOCALAPPDATA%\PenumbraVR\logs\black-plague-probe-<PID>.log`.

1. **Mirror apagado.** Confirma en gameplay que la ventana del PC permanece negra
   y estable; no debe reaparecer el punto blanco que crecía hasta ocupar casi toda
   la ventana.
2. **Mirror encendido.** Actívalo solo para comprobar que la copia del ojo sigue
   funcionando y que alternarlo no rompe la presentación del visor.
3. **Alt+Tab durante gameplay.** Cambia a otra ventana y continúa unos segundos en
   el visor. Comprueba mundo, tracking y mandos.
4. **Menú tras perder foco.** Sin devolver primero el foco al juego, abre pausa,
   inventario/libreta y vuelve a gameplay. Anota exactamente qué superficies se
   ven negras. Este punto sigue siendo una regresión abierta; no se considera
   arreglado por la limpieza del monitor.
5. **Recuperación de foco.** Devuelve el foco al juego y comprueba si los menús se
   recuperan sin reiniciar la sesión.
6. **Salida normal.** Cierra desde el juego y deja el log intacto para correlacionar
   la transición de foco/presentación.

## Gate posterior: room-scale físico

Esta parte solo debe ejecutarse cuando exista una build que implemente y haya
host-testado la nueva petición física X/Z collision-aware. En ese momento:

1. baseline quieto y movimientos de cabeza pequeños en espacio libre;
2. desplazamiento físico hacia delante/atrás/lados sin stick;
3. bloqueo frontal contra pared sin atravesarla ni desplazar el mundo;
4. desplazamiento diagonal contra pared conservando el componente tangencial;
5. esquinas y obstáculos con aceptación parcial;
6. locomoción nativa con stick después de desplazamiento físico;
7. recentrado y repetición de movimiento físico;
8. agacharse/levantarse nativo para forzar sustitución del cuerpo y repetir
   bloqueo/deslizamiento;
9. varios minutos de juego normal para comprobar que sigue existiendo un único
   tick nativo cercano a 60 Hz;
10. salida normal y revisión del log.

No mezcles en ese gate ajuste final de `1.5 / 2.25 m/s`, crouch físico, salto VR,
cámara/bob, agarres, mecanismos o Enhanced Visuals. Esos hitos siguen separados.
