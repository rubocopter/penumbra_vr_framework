# Black Plague — checklist de validación con visor

Actualizado: 2026-09-11.

El siguiente gate de gameplay es **reconciliación tracking/body en modo shadow**.
La prueba observa tracking, locomoción nativa y movimiento aceptado por el cuerpo,
pero no inyecta desplazamiento físico ni activa traslación posicional del HMD.

## Arranque obligatorio para esta tanda

Con el juego cerrado, el visor y los mandos disponibles, abre PowerShell en la
raíz del repositorio y ejecuta:

```powershell
.\tools\Start-BlackPlagueShadowValidation.ps1
```

Si Black Plague está instalado fuera de la biblioteca Steam habitual:

```powershell
.\tools\Start-BlackPlagueShadowValidation.ps1 -GamePath "RUTA\A\Penumbra.exe"
```

El helper ejecuta primero el verificador de la imagen exacta, crea la petición
shadow temporal y lanza el juego por la ruta VR normal. No abras esta tanda
directamente desde Steam ni con `Start-Black-Plague-VR.cmd`, porque esas rutas no
demuestran que la petición shadow esté activa.

Mantén abierta la ventana de PowerShell durante toda la sesión. Antes de contar
la prueba como válida debe mostrar que detectó el proceso y confirmó:

```text
body_reconciliation_shadow enabled=1 source=mutex
```

Si esa confirmación no aparece, no continúes la tanda como validación shadow.
La ventana mantiene vivo el mutex hasta que termina `penumbra.exe`.

Los logs se guardan automáticamente en:

```text
%LOCALAPPDATA%\PenumbraVR\logs
```

El log principal de esta tanda será `black-plague-probe-<PID>.log`. No hace falta
copiarlo ni prepararlo manualmente: al terminar basta con indicar que has cerrado
el juego y cualquier anomalía observada; Codex puede revisar el log nuevo.

## Secuencia de pruebas

Hazlas en este orden. Prioriza una zona sencilla con paredes y obstáculos claros.
No hace falta dedicar tiempo a menús, mirror, agarres o herramientas en esta tanda.

1. **Baseline quieto.** Quédate varios segundos sin tocar sticks. Mueve después
   la cabeza solo unos centímetros hacia delante, atrás, izquierda y derecha.
   El mundo debe permanecer estable.
2. **Rotación de cabeza.** Haz yaw, pitch y roll normales. Comprueba que no aparece
   un desplazamiento extraño del cuerpo o del mundo.
3. **Movimiento nativo libre.** Camina con stick hacia delante, atrás y ambos
   lados en una zona despejada.
4. **Diagonales y cambios de dirección.** Combina ejes y cambia de dirección varias
   veces para registrar aceptación parcial normal del movimiento nativo.
5. **Bloqueo frontal.** Avanza despacio contra una pared hasta que el cuerpo deje
   de progresar.
6. **Deslizamiento.** Avanza en diagonal contra una pared. El componente bloqueado
   debe detenerse mientras el movimiento tangencial permitido continúa.
7. **Esquinas y obstáculos sólidos.** Busca varios casos de bloqueo total y
   aceptación parcial sin forzar situaciones incómodas.
8. **Recentrado.** Recentrar durante la partida y repetir brevemente movimiento de
   cabeza y locomoción.
9. **Agacharse/levantarse nativo.** Haz varios ciclos. Black Plague sustituye el
   cuerpo físico al agacharse; esta prueba comprueba que el adapter sigue el cuerpo
   actual y no conserva un puntero obsoleto.
10. **Salto nativo.** Haz uno o dos saltos normales. Solo interesa comprobar que
    convive con el seguimiento horizontal; no evalúa room-scale vertical.
11. **Repetición tras cambios de cuerpo.** Después de agacharte y saltar, repite
    bloqueo frontal y deslizamiento para detectar regresiones de ownership/body.
12. **Juego normal.** Juega varios minutos con locomoción corriente. Esto permite
    recoger suficiente telemetría para comprobar el tick nativo cercano a 60 Hz.
13. **Salida normal.** Sal desde el menú del juego. El script debe terminar después
    de `penumbra.exe` y liberar la petición shadow temporal.

## Qué anotar durante la sesión

Solo hace falta comunicar observaciones que ayuden a relacionar lo visible con
la telemetría: número de prueba, estancia/mapa, qué estabas haciendo y qué ocurrió.
Son especialmente útiles desplazamientos del mundo al mover la cabeza, movimiento
que atraviese o se pegue de forma extraña a colisiones, pérdida de movimiento
tangencial al deslizar, cambios tras recentrar/agacharse y cualquier cierre o
crash.

Si todo parece normal, al acabar basta con indicar que completaste la lista y que
el juego está cerrado. El log permite verificar frecuencia de tick, cuerpo actual,
movimiento nativo solicitado/aceptado y telemetría shadow.

## Fuera de este gate

No interpretes todavía estas funciones como parte de la validación actual:

- caminar físicamente por la habitación o aceptación de traslación HMD;
- room-scale activo o crouch físico;
- ajuste final de 1,5 / 2,25 m/s;
- salto o movimiento vertical controlado por VR;
- cámara/bob y ajustes de confort;
- calidad o rendimiento del mirror;
- agarres, barras largas, puertas, palancas o joints;
- calibración definitiva de herramientas/manos;
- Enhanced Visuals.

Los valores de plan físico que pueda registrar el shadow son observacionales.
Como todavía no existe una petición física inyectada y collision-aware en metros,
no deben clasificarse como desplazamiento físico aceptado o rechazado.
