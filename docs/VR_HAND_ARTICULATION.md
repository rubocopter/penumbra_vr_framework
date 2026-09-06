# Articulación de manos independiente

2026-09-06. Se elige no incorporar el modelo descargado de TurboSquid 2564269:
no se han copiado ni extraído malla, rig, pesos, texturas ni poses de su .blend.
No se afirma que esta decisión cambie la licencia de los recursos de Rework.

`src/runtime/vr_hand_pose.*` convierte los cinco curls de entrada en flexiones
locales independientes, separación de dedos y orientación del pulgar. Conserva
la respuesta instantánea del framework: sin suavizado temporal adicional ni
sustitución del curl del pulgar por el grip. Cada falange tiene su propia curva;
la distal cierra progresivamente respecto a la intermedia. El pulgar distingue
su base de sus otras dos articulaciones. Estos son ajustes visuales originales,
no límites médicos validados ni una reproducción del rig descargado.

El dibujado provisional usa esta política y longitudes distintas por falange.
Se corrige la simetría de posiciones: índice junto al pulgar en ambas manos.
No se altera la lógica de interacción, los sockets de herramientas ni la física.

## Pendiente para las mallas de Rework

La política no es todavía un rig de malla con skinning. Antes de aplicarla a
Rework hay que mapear los huesos y ejes de reposo, ajustar pesos de deformación y
verificar ambas manos. No copiar ángulos Euler de un esqueleto a otro. Mantener
la palma visual alineada con sockets y el ancla de interacción; las poses de
herramientas deben mezclarse explícitamente sin perder el input independiente.

Tests: límites, monotonía, simetría izquierda/derecha, independencia de dedos y
rechazo de valores no finitos. La suite WGL sigue comprobando dibujo/oclusión y
restauración del estado gráfico. Falta validar aspecto y comodidad con visor.

## Resultado de la primera prueba

El usuario indicó que el movimiento se percibe parecido al anterior. No se
observó todavía una mejora anatómica clara con la geometría procedural. Esto es
coherente con el alcance actual: se cambiaron curvas y longitudes, no se incorporó
una malla deformable ni skinning. La siguiente mejora visual debe hacerse sobre
las manos definitivas, conservando el input individual que ya funciona bien.
