# Próxima prueba: iluminación de Black Plague en VR

Estado: corrección implementada y validada en pruebas automáticas el
2026-09-05; **pendiente de confirmar en el juego con el visor**.

## Síntoma y causa investigada

La bombilla sigue encendida, pero su iluminación sobre paredes y suelo
desaparece a distancia media al mover la cabeza, especialmente arriba/abajo.
De cerca no sucede.

HPL recorta el dibujo de cada luz con un rectángulo de pantalla (`scissor`).
Overture Rework devuelve el tamaño VR al calcular ese rectángulo. El backend
binario aún recibía coordenadas de escritorio: en la sesión observada,
2560×1440 frente a 3400×3468 por ojo. Cuando la cámara está muy cerca del volumen
de luz, la ruta HPL evita el recorte; esto encaja con la diferencia por distancia.
La confirmación visual de que esta es la única causa sigue pendiente.

## Cambios preparados

- Reescalado conservador de los recortes durante cada pasada estéreo, con
  redondeo hacia fuera y margen para la conversión vertical del HPL antiguo.
- Rectángulos limitados al ojo; los vacíos siguen vacíos y los valores inválidos
  conservan el comportamiento OpenGL original.
- Protección por hilo, contexto, framebuffer y viewport: el mirror, la interfaz
  y los efectos que cambian de destino no reciben ese reescalado.
- Cada ojo guarda y restaura el rectángulo y la activación del recorte. Un
  recorte anterior no puede limitar el borrado inicial del siguiente ojo.
- Contadores `eye_scissor_remapped` y `eye_scissor_bypassed` en el log habitual.
- Hook reversible sobre la importación `glScissor`, instalado dentro del ciclo
  del backend autorizado por hash. No se modifica el ejecutable instalado.

No se han activado nuevos shaders, tonemapping ni Enhanced visuals en Black
Plague. Esta corrección conserva su iluminación existente. Overture Rework no
ha sido modificado.

## Preparación

Usar la DLL y el launcher de `build/bin/Release`, no la compilación sin OpenVR.
Con el juego recién iniciado, adjuntar el probe y ejecutar `--start-vr` como en
las sesiones anteriores. El mirror conserva la preferencia guardada; puede
cambiarse con `--vr-mirror-on` / `--vr-mirror-off`.

## Comprobaciones con el visor

1. Volver a la lámpara que fallaba. A distancia media, quedarse quieto y mirar
   arriba/abajo y a ambos lados; comprobar paredes y suelo, no solo la bombilla.
2. Acercarse y alejarse lentamente atravesando la distancia donde aparecía el
   corte. Repetir tapando alternativamente cada ojo si hay diferencias visibles.
3. Repetir junto a esquinas/puertas y con varias luces visibles; comprobar que
   no reaparecen paredes u objetos ausentes.
4. Revisar la linterna y otros focos disponibles: no deben adquirir bordes
   rectangulares ni iluminar zonas incorrectas.
5. Comparar mirror activado/desactivado, abrir/cerrar el menú y parar/reanudar VR.
   No debe quedar un recorte en el escritorio ni contaminarse un ojo con el otro.
6. Observar los tiempos de SteamVR en la misma escena. La corrección consulta
   el destino de dibujo por recorte; su coste real todavía debe medirse.

En una escena con luces recortadas se espera `eye_scissor_remapped > 0`, dos
pasadas de ojo y ninguna avería estéreo/restauración. Un cero cerca de una luz
no prueba un fallo: HPL puede desactivar el recorte allí. `bypassed` distingue
llamadas dentro de la pasada que no cumplen las condiciones de reescalado.
Los contadores son por frame observado, no acumulados de toda la sesión.

Si persiste el defecto, anotar lugar, distancia, orientación y si afecta a uno
o ambos ojos. Revisar primero los contadores; después investigar visibilidad y
stencil/sombras de esa luz. No dar la incidencia por cerrada solo por compilar.

## Verificación realizada sin juego ni visor

La tanda inicial de iluminación pasó 17/17 pruebas. La batería completa a
2026-09-06 pasa 22/22 en OpenVR Release, OpenVR Debug y Release sin OpenVR, con avisos
tratados como errores. Las pruebas de OpenGL real verifican instalación y
retirada del hook, lectura de píxeles dentro/fuera del recorte, exclusión de
otros destinos, ámbitos anidados y restauración del estado. La prueba matemática
cubre escalado desigual, resoluciones pequeñas, bordes, vacíos y límites enteros.
