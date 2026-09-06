# Continuidad VR entre los tres juegos

Objetivo del usuario (2026-09-06): Overture, Black Plague y Requiem deben sentirse
como una continuación, con Rework como referencia y mejoras compartidas. Esto
es un criterio de aceptación, no una afirmación de paridad actual.

## Comportamiento compartido

- Mismo perfil diestro, selección R2 y ciclo L1 apagado/glowstick/linterna.
- Mismos umbrales de stick, rumbo relativo al HMD, recentrado y giro.
- Agarre consistente con la palma visible, liberación sin transferencia de mano,
  momento de lanzamiento limitado y ausencia de autopropulsión del jugador.
- Menús, inventario y libreta con reglas coherentes de foco y cierre.
- Mirror con preferencia persistida observable y misma orientación que el visor.
- Mantener el seguimiento de dedos que el usuario valoró positivamente.

Conservar diferencias narrativas, objetos disponibles y reglas propias de cada
juego. No uniformar velocidades modificando arbitrariamente física o gravedad:
primero medir tiempo de simulación, distancia recorrida y duración del salto.

## Orden de integración

1. Seguridad: reproducir y filtrar colisiones entre objeto sostenido y jugador.
   Rework aplica filtros en CharacterBody.cpp, PhysicsWorld.cpp y
   PhysicsMaterialNewton.cpp; copiar solo SetMatrix no constituye equivalencia.
2. Diagnóstico: registrar configuración efectiva y medir temporización sin visor.
   El incidente del mirror no está explicado completamente por el archivo actual.
3. Herramientas: palma visible, punto de agarre y dirección de luz coherentes;
   no copiar calibraciones de modelos distintos sin verificar geometría.
4. Puertas/joints, opciones de lateralidad y mejoras de confort.
5. Ejecutar el mismo conjunto de aceptación en cada backend/build admitido.

Políticas independientes del ejecutable pertenecen a runtime; direcciones y
transiciones nativas pertenecen al backend exacto. Ningún offset de Overture se
considera válido en Black Plague/Requiem sin evidencia. Las pruebas de un juego
no certifican los otros dos. Rework se conserva sin modificar como referencia.
