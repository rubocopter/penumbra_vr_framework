# Arranque VR y estado de los mandos

Actualizado: 2026-09-06. Backend **Black Plague FD316F…** únicamente.
La prueba del usuario confirmó arranque BAT, menús, inventario/libreta, dedos,
giro, correr/agacharse y ausencia del fallo de luces en la zona probada.
Las correcciones posteriores descritas aquí todavía necesitan visor.

La prueba posterior de herramientas confirmó que el glowstick sigue la mano
izquierda. Su socket provisional lo deja dentro de la mano; se aplaza el ajuste
visual hasta integrar las manos definitivas, para no calibrarlo dos veces. El
usuario percibió el movimiento de dedos parecido al anterior: la política nueva
queda validada por tests matemáticos, pero no como mejora visual apreciable.

### Aviso tras la segunda prueba (2026-09-06)

Tanda posterior lista para prueba acotada: glowstick/linterna anclados a la mano
izquierda (sockets provisionales); limpieza de textura rectangular GL antes de
dibujar mirror/menús/manos; lectura de vuelta del mirror desde el proceso tras
arrancar con BAT. El registro añade `native_update_timing` (ratio tiempo simulado
/real del ButtonHandler, no del solver Newton), contadores de herramientas y
agarres rechazados por seguridad. Se amplía el buffer de log para evitar truncar diagnósticos.
No se modifica la velocidad de simulación, gravedad ni duración del salto.

El usuario reportó salida del mapa al caminar con una barra agarrada. La ruta se
bloqueó y solo se ha reactivado tras mapear el indicador nativo +3C8 que excluye
el cuerpo sujeto de consultas de personaje, rayos y contactos Newton. Se guarda
y restaura su valor original al soltar. La regresión cubre ambos valores iniciales,
pero aún no lo certifica en el motor real: la primera prueba debe usar un objeto
pequeño, lentamente y sin lanzamiento ni barras largas.

El stick ahora limita su vector a longitud 1 para evitar sobrevelocidad diagonal.
Esto no resuelve por sí solo la sensación de salto/tiempo acelerado, aún pendiente.
El launcher imprime ruta efectiva de settings y valor del mirror tanto en
preflight como al activarlo. La lectura actual comprobada es `on`; el registro
de la sesión 9156 decía `disabled`. No se considera resuelto el mirror en visor.

## Arrancar sin adjuntar el mod a mano

Desde `E:\penumbra_vr`, doble clic en `Start-Black-Plague-VR.cmd`.
Usa el ejecutable Release y la instalación Steam habitual. También admite la
ruta del ejecutable como primer argumento si Steam está en otra biblioteca.

El lanzador comprueba el hash admitido y los archivos VR, abre Black Plague
mediante Steam (appid 22120), espera la inicialización del ejecutable protegido,
adjunta el framework y activa la presentación continua con el mirror guardado.
No modifica el ejecutable del juego ni sus opciones de lanzamiento en Steam.
Abrir directamente el juego en Steam **no** ejecuta este lanzador automáticamente.
SteamVR debe estar instalado y los dispositivos configurados; OpenVR solicita
su inicialización. El usuario confirmó el arranque directo con el BAT.

Si ya está abierto el mismo ejecutable, reutiliza ese proceso. No elige procesos
por nombre solamente: Overture, Black Plague y Requiem deben distinguirse por
ruta y huella. Si hay dos instancias coincidentes, se detiene. En caso de error
no mata el juego ni la sesión de SteamVR. El detalle queda en la consola y en
`%LOCALAPPDATA%\PenumbraVR\logs`.

Comprobación sin abrir el juego ni SteamVR:

```powershell
& E:\penumbra_vr\build\bin\Release\PenumbraVR.ProbeLauncher.exe --check-vr 'C:\Program Files (x86)\Steam\steamapps\common\Penumbra Black Plague\redist\Penumbra.exe'
```

## Qué está implementado

1. Lector real de acciones OpenVR: gameplay/UI, poses grip/aim de ambas manos,
   curls de dedos y salida háptica limitada. Bindings de PSVR2 y demás perfiles
   se copian a `build/bin/<config>/vr` en cada compilación. No se sondea por ojo:
   lo hace una vez `cButtonHandler::Update`.
2. Puente nativo exacto: movimiento analógico combinado con teclado, giro por
   pasos de 45 grados con retorno a neutro, salto, correr, agacharse, interactuar,
   examinar, guardar objeto, inventario, libreta, pausa y ciclo de luz rápida
   apagado → glowstick → linterna → apagado, como Rework. El stick sigue el yaw
   horizontal del HMD; el teclado conserva sus ejes nativos. Tracking obsoleto
   inhibe movimiento VR. No equivale a cuerpo completo ni room-scale.
   No usa emulación de teclas de Windows. Los nombres de acción conservan su
   ABI antiguo y siempre se ejecuta la consulta original del juego.
3. Menú inicial, pausa/inventario/libreta: captura del escritorio presentada en
   ambos ojos como panel de 2,4 metros de ancho a 2 metros, orientado según el
   yaw al abrirlo. Apuntado derecho en menú principal, inventario y libreta,
   coordenadas nativas 800×600, recentrado y prioridad temporal del ratón cuando
   se mueve. Los diálogos especiales conservan sus rutas nativas y aún necesitan
   cobertura en ejecución. Se restauran matrices, texturas, programas GL,
   viewport, scissor y demás estado gráfico tras dibujar el panel.
4. Adaptador espacial integrado en la DLL y compilación: selección desde aim
   dominante (derecho en este puente) en el estado normal, refresco antes de pulsar,
   agarre relativo a la palma de cuerpos libres y lanzamiento limitado a 9 m/s.
   Las transiciones originales conservan la gestión de masa/gravedad; el
   adaptador restaura los límites de velocidad que cambia. Un botón de la otra
   mano no transfiere el objeto ni oculta la liberación de la mano propietaria.
   Pérdida de tracking/foco, UI o salto de pose sueltan sin impulso de lanzamiento.
   Se emiten pulsos hápticos cortos al coger/soltar. Son rutas verificadas con
   dobles de las funciones nativas, no una simulación del motor Newton real.
   Mientras se sostiene el cuerpo, su `CollideCharacter` nativo se desactiva para
   impedir que empuje al jugador y se restaura exactamente al soltar. La instalación
   exige firmas de constructor, mundo, rayo y las dos ramas de contacto Newton.
   La velocidad de lanzamiento usa una mediana móvil de hasta cinco muestras:
   ignora picos aislados y una liberación sin historial suficiente no transfiere
   momento. Un salto de tracking mayor de 35 cm suelta de forma segura.
   Corregido el orden de Enter: el juego publica el estado Grab después de
   regresar de Enter. La adquisición espera al estado confirmado; antes podía
   quedar en el agarre nativo de escritorio. Regresión añadida con ese orden.
5. Guantes geométricos provisionales para ambas manos, dedos según curls OpenVR
   (o postura aproximada sin skeleton), rayo de apuntado y oclusión por profundidad
   en cada ojo. No son las mallas/esqueletos HPL de Rework y no proyectan sombras.
   Las manos se sitúan respecto al HMD actual para convivir con el tracking
   rotacional existente; esto no añade room-scale ni colisión de las palmas.
6. Mirror en partida: copia de la textura del ojo izquierdo al escritorio justo
   antes del swap, conservando proporción y bandas negras. No renderiza un tercer
   mundo nativo. Los menús conservan la captura/presentación nativa del escritorio.
   Prueba WGL de píxeles, orientación, bandas y restauración de viewport/scissor.

La numeración anterior describe módulos implementados, **no certifica que los
tres hitos pedidos por el usuario estén terminados**.

## Lo que NO está terminado

- El tercer hito original sigue parcial: puertas/palancas y cuerpos con joints
  o padres conservan el comportamiento nativo. Falta la colisión de palmas,
  validar físicamente la exclusión conservadora cuerpo/character, las mallas HPL
  de Rework y calibrar físicamente las herramientas/luces ancladas. Durante un agarre espacial
  todavía no se actualiza el rayo secundario de examinar como hace Rework.
- `PlayerState_Interact_VR.cpp` es la referencia, pero no se copian offsets de
  Overture. Véase `BLACK_PLAGUE_SPATIAL_NOTES.md` para llamadas verificadas,
  pruebas, límites y el siguiente punto de integración de herramientas.
- La configuración de este puente es diestra y de giro por pasos. El lector
  soporta acciones zurdas, pero falta persistir/aplicar esa preferencia al puente.
  Se desactiva el action set offhand para evitar propiedad mezclada. L2 no
  interactúa ni selecciona en el perfil PSVR2 diestro; R2 es el gatillo principal.
- Tracking corporal posicional y Enhanced visuals GPU no están conectados.
- Falta validar transiciones mientras se mantiene un botón, la pérdida de
  tracking/foco en una partida real y la convivencia de mandos y teclado al
  mantener ambos la misma acción. Las pruebas puras no sustituyen esa prueba.
- La iluminación pasó la zona probada por el usuario; falta cobertura en otras zonas.

A petición del usuario se prepara una prueba experimental de lo incorporado,
sin marcar los tres hitos como completos. Véase la
[lista para el visor](VR_HEADSET_TEST_CHECKLIST.md). Rework permanece intacto
en `E:\penumbra_vr_rework`.

## Evidencia reproducible

Resultado de esta tanda: **22/22 tests en Release, 22/22 en Debug y 22/22
en Release sin SDK OpenVR**, además del verificador binario y de metadatos.

El workflow de GitHub compila Debug/Release sin SDK y ejecuta 21 pruebas por
configuración: excluye explícitamente `opengl_eye_targets`, que requiere el
driver WGL con FBO/programas y se verifica localmente. El resultado de CI es
independiente de los 22/22 locales; ninguno certifica una prueba con visor.

- CTest incluye `vr_action_input`, `vr_native_intents` y `vr_math` con foco,
  poses, límites, liberación, consumo único de edges, giro, rayo del menú y ABI
  x86. El test OpenGL comprueba píxeles de ambos ojos y restauración de estado.
- `spatial_interaction` ejecuta el adaptador real contra una imagen sintética y
  funciones nativas dobles: restauración de límites/masa/gravedad, lanzamiento,
  foco/tracking/UI, cero tiempo, salto de pose, joints, retirada segura y
  restauración de `CollideCharacter` para cuerpos originalmente true/false.
  `vr_grab_pose` cubre transformaciones, límites y filtrado robusto de lanzamiento;
  OpenGL comprueba además píxeles
  de manos y que no atraviesen una profundidad más cercana.
- `tools/Test-BlackPlagueInputMap.ps1 -ImagePath <captura-inicializada>` contrasta
  los bytes de 76 consultas, 2 movimientos, 3 cursores y la vtable de Update.
  También contrasta 11 slots espaciales, las entradas SetMatrix/GetJointNum,
  la llamada de picking y las escrituras del contacto local de agarre.
  La captura local es `artifacts/black-plague-22000-live.bin`, imagen virtual con
  base 0x00400000; **no** usar los offsets como posiciones en el PE de disco.
- Solo `--check-vr` se ejecutó contra la instalación en esta tanda. El juego y
  SteamVR estaban cerrados; no se afirma haber probado arranque, controles o
  manos en el visor.
