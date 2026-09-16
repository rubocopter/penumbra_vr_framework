# Articulación de manos independiente

2026-09-06. Se elige no incorporar el modelo descargado de TurboSquid 2564269:
no se han copiado ni extraído malla, rig, pesos, texturas ni poses de su .blend.
No se afirma que esta decisión cambie la licencia de los recursos de Rework.

`src/runtime/vr_hand_pose.*` convierte los cinco curls de entrada en flexiones
locales independientes, separación de dedos y orientación del pulgar. El input
usa el acondicionamiento compartido ya extraído de Rework (deadzone/suavizado y
fallback grip/trigger cuando el backend dispone de esos analógicos); Black
Plague conserva después su articulación de cinco dedos más rica. Cada falange
tiene su propia curva y la distal cierra progresivamente respecto a la
intermedia. El pulgar distingue su base de sus otras dos articulaciones.

Black Plague ya consume las mallas/rigs derecha e izquierda de Rework desde un
adaptador de renderer propio. Los DAE y la textura se preprocesan a datos C++;
el runtime aplica CPU skinning sobre la jerarquía/inverse-bind probada y mapea la
salida de `VrHandArticulation` a los ejes del rig. No se altera la lógica de
interacción, los sockets de herramientas ni la física.

## Estado del rig de Rework

Los 17 joints, jerarquía, inverse-bind y asignación rígida de un hueso por
posición están integrados. El renderer conserva el perfil visual de Rework y
deduplica posición/UV para dibujar 3.376 vértices con 18.102 índices por mano,
en vez de expandir los 18.102 corners completos en cada ojo. La presentación
sigue en estado `host-tested`: falta confirmar con visor escala, orientación,
movimiento de dedos, alineación de herramientas y frame pacing.

Tests: límites, monotonía, simetría izquierda/derecha, independencia de dedos y
rechazo de valores no finitos. La suite WGL sigue comprobando dibujo/oclusión y
restauración del estado gráfico. Falta validar aspecto y comodidad con visor.

## Resultado de la primera prueba

La primera prueba descrita aquí corresponde a la etapa procedural anterior y se
conserva como histórico. La siguiente prueba relevante ya debe evaluar las mallas
Rework integradas, conservando el input individual que funcionaba bien.
