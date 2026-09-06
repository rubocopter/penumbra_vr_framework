# Prueba con PSVR2 Sense — Black Plague

Preparación inicial: 2026-09-06, código fef5d40. El checkpoint actual recompila
con 22/22 tests; la preparación inicial también dejó correctos el preflight del
ejecutable admitido y los archivos `vr` respecto al repositorio. El juego y
SteamVR no se iniciaron durante la preparación.
Esto prepara una prueba experimental; no certifica jugabilidad completa.

## Seguimiento tras la primera prueba

**Actualización tras la segunda prueba:** el agarre a la palma vuelve a estar
habilitado con exclusión temporal de colisión body/character verificada contra
la imagen exacta. Por el incidente de la barra, la primera prueba queda limitada
a un objeto pequeño, movimientos lentos y sin lanzamiento.

El usuario confirmó BAT, recentrado, menús/inventario/libreta, dedos, movimiento,
giro, correr/agacharse y luces estables en la zona probada. Reportó mirror negro
con manchas, rumbo desacoplado del HMD, interacción solo L2, agarres nativos y
glowstick sin control rápido. Dashboard no presentó problemas aparentes.

Correcciones locales posteriores (pendientes de visor): mirror desde ojo
izquierdo, propiedad diestra sin action set offhand, movimiento relativo al HMD,
adquisición de agarre tras confirmar el estado nativo y ciclo L1 de Rework.
Prioridad de la siguiente prueba: esas cinco rutas. Herramientas físicamente
ancladas a la mano y manipulación de joints/puertas siguen incompletas.

## Arranque

### Próxima prueba acotada — tanda de herramientas y diagnóstico

1. Arranca con el BAT actualizado. El log debe incluir `VR monitor mirror
   readback: enabled`; el launcher muestra el archivo leído y confirma el valor.
2. Comprueba el monitor durante partida y al abrir/cerrar inventario. Debe mostrar
   el ojo izquierdo en partida, sin blanco uniforme. Una captura si falla basta.
3. En una zona despejada, usa L1 para glowstick/linterna/apagado. Mueve y gira la
   mano izquierda: modelo y luz deben acompañarla. Anota si hay desplazamiento,
   tamaño u orientación incorrectos. No empujes las herramientas contra paredes.
4. Sin sostener cuerpos, camina recto/diagonal, corre y salta durante unos 20 s.
   Deja otros 10 s quieto. Los registros de temporización ayudarán a distinguir
   velocidad física de un problema del reloj. Indica si el salto sigue acelerado.
5. Abre/cierra inventario y dashboard; comprueba que vuelven herramientas y dedos.
6. Abre/cierra cada mano: el índice debe estar junto al pulgar en ambos lados.
   Comprueba la nueva flexión diferenciada de falanges y del pulgar; indica si
   se cruzan dedos o se siente menos inmediata la respuesta. La geometría sigue
   siendo provisional; el modelo descargado no se ha incorporado.

Resultado recibido: el glowstick siguió la mano, pero quedó dentro de ella; no
ajustar hasta tener la mano final. Los dedos se percibieron parecidos a la versión
anterior. Estas observaciones quedan como referencia para la próxima iteración.

No hacer pruebas de lanzamiento ni repetir el incidente de la barra en esta
primera validación. Esta prueba valida una tanda parcial, no los tres juegos ni
los mecanismos articulados.

1. Conecta el visor y enciende ambos Sense. Para la primera pasada, comprueba
   primero que SteamVR ve los tres dispositivos; así aislamos fallos del mod.
2. Ejecuta `E:\penumbra_vr\Start-Black-Plague-VR.cmd`, no el acceso normal de Steam.
   Debe abrir Black Plague, adjuntar el framework y activar VR sin introducir PID.
3. El mirror está activado en `%LOCALAPPDATA%\PenumbraVR\settings.ini`.
   Mantén teclado y ratón disponibles por si falla el menú con mandos.
4. Usa una partida/punto de prueba y objetos prescindibles. No sobrescribas tu
   guardado principal durante estas comprobaciones. No camines físicamente hacia
   paredes: el movimiento corporal room-scale y la colisión de palmas no existen.
5. Si aparece imagen doble persistente, orientación incorrecta o malestar, detén
   la sesión. Si falla el lanzador, conserva el mensaje de su consola.

## Controles previstos (perfil diestro predeterminado)

Los siguientes son los bindings distribuidos; una personalización de SteamVR
puede sustituirlos. Su funcionamiento real es precisamente parte de la prueba.

| Control | Acción en partida |
|---|---|
| Stick izquierdo | Desplazarse |
| Pulsar stick izquierdo | Correr |
| Stick derecho horizontal | Giro por pasos de 45°, volviendo a neutro entre pasos |
| Pulsar stick derecho | Agacharse (configuración actual con alternancia) |
| X derecho | Saltar |
| R2 | Interactuar o agarrar con la mano derecha; L2 no interactúa |
| R1 | Inventario |
| Cuadrado izquierdo | Libreta |
| L1 | Apagado → glowstick → linterna → apagado (según objetos disponibles) |
| Círculo derecho | Examinar |
| Triángulo izquierdo | Guardar objeto, según estado nativo |
| Options derecho | Pausa |
| Crear izquierdo | Recentrar mirando al frente |

En menú: apunta con la mano derecha y selecciona con R2. Círculo vuelve atrás;
R1, Cuadrado u Options envían cerrar. El ratón tiene prioridad aproximadamente
1,5 segundos después de moverlo: espera ese tiempo al volver al apuntado VR.

## Lista ordenada

- [ ] **Arranque y mirror:** imagen en ambos ojos, menú visible y escritorio con
  imagen. No quedarse únicamente en la pantalla de cine virtual de SteamVR.
- [ ] **Menú inicial:** apuntar, seleccionar una opción, entrar/salir de ajustes,
  volver y cargar partida. Si falla, usar el ratón para continuar y anotarlo.
- [ ] **Orientación y recentrado:** girar cabeza izquierda/derecha y arriba/abajo;
  el mundo no gira al revés, no se inclina y no duplica imágenes. Crear debe
  realinear la orientación; no se espera desplazamiento corporal room-scale.
- [ ] **Visibilidad:** en una habitación, mirar detrás y a los lados y desplazarse
  con el stick. Paredes y objetos no deberían aparecer solo al acercarse o al
  orientar la cámara con el ratón.
- [ ] **Lámparas (prioritaria):** repetir la lámpara problemática cerca, a media
  distancia y lejos, mirando arriba/abajo y a ambos lados. Comprobar la luz sobre
  paredes/suelo, no solo la bombilla. Anotar si desaparece en uno o ambos ojos.
- [ ] **Locomoción:** caminar, parar, correr, saltar y agacharse. Probar cada paso
  de giro con retorno a neutro. Al soltar los controles no debe continuar andando.
- [ ] **Manos y selección:** ver ambos guantes provisionales, moverlos por separado
  y seleccionar un objeto con la mano derecha. El rayo y el objeto seleccionado deben
  corresponder; las manos no deben dibujarse por delante de una pared que las tapa.
  Dedos simplificados sin skeleton son una limitación conocida.
- [ ] **Agarrar/soltar — prueba de seguridad prioritaria:** un objeto pequeño y
  libre con R2; L2 no debe apropiárselo. Mover la mano despacio sin caminar,
  después dar un paso corto con stick y soltar. Debe seguir la palma y caer con
  gravedad sin acelerar ni desplazar al jugador. Si hay cualquier tirón, soltar
  inmediatamente y no continuar. No usar barras, puertas ni palancas.
- [ ] **Propiedad:** sin lanzar, pulsar la otra mano mientras se sostiene el objeto:
  no debe cambiar de mano ni quedar pegado al soltar el gatillo propietario.
- [ ] **Inventario, libreta y pausa:** abrir/cerrar varias veces desde partida,
  apuntar y seleccionar. Verificar que vuelve el mundo y no quedan acciones
  pulsadas. Abrir pausa mientras sujetas un objeto: debe soltar sin lanzarlo.
- [ ] **Pérdida de foco:** estando quieto y con un objeto prescindible, abrir el
  dashboard de SteamVR. Al regresar no debe caminar solo ni conservar un agarre
  atascado. No desconectar cables ni golpear/ocultar físicamente dispositivos.
- [ ] **Linterna y acciones nativas:** alternar linterna, examinar y guardar un
  objeto cuando proceda. Solo probar activación: la luz aún no sigue la mano.
- [ ] **Teclado/ratón:** alternar con los mandos; comprobar que siguen funcionando
  y que no quedan movimientos o botones atascados al cambiar de dispositivo.
- [ ] **Fluidez y salida:** observar tirones al girar y cerca de luces; salir desde
  el menú y comprobar que no se bloquean juego ni compositor. El mirror añade un
  pase de renderizado: esta pasada no es una medición de rendimiento sin mirror.
- [ ] **Segundo arranque opcional:** una vez superado lo anterior, con juego y
  SteamVR cerrados pero dispositivos conectados, usar de nuevo el script para
  comprobar también la inicialización automática del runtime.

## No esperar todavía

Puertas/palancas manipuladas espacialmente, colisión física de manos, filtro
exclusivo del jugador (el binario usa el más amplio `CollideCharacter`), room-scale,
modelos de manos completos de Rework y Enhanced visuals GPU.
Durante el agarre espacial, el rayo secundario de examinar tampoco está portado.
No confundir estas limitaciones con regresiones de esta tanda.

## Cómo comunicar un fallo

Indica número/nombre de prueba, mapa o estancia, objeto/lámpara, mano utilizada,
distancia aproximada, si afecta a uno o ambos ojos y pasos para repetirlo.
Para fluidez, distingue tirones visibles de cualquier cifra mostrada por SteamVR.
Los logs del framework están en `%LOCALAPPDATA%\PenumbraVR\logs`; no los borres.
No hace falta publicar capturas binarias del proceso ni archivos de la partida.
