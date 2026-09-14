## 12. Prompt de implementación para Sol

> Implementa una intervención coherente en Penumbra VR Framework para completar contratos de tracking/cuerpo/render, postura, contacto y lifecycle. La auditoría se realizó sobre `E:\penumbra_vr`, branch `main`, commit `73c70f0aad6fd2f33dc27983a44971ec44215f4a`, working tree limpio. Comprueba si HEAD cambió y revisa sólo las diferencias relevantes; no repitas toda la auditoría.
>
> La referencia conductual es `E:\penumbra_vr_rework` revisión **`23c890f7dbd06b939be9951d282e6e948d9a6623`**, no el HEAD actual de ese checkout. Lee AGENTS y los handoffs actuales. No sustituyas algoritmos demostrados por otros nuevos.
>
> **Baseline conocido**
>
> Pasaban builds Win32 Debug/Release, 30/30 CTest en ambos, Release sin SDK OpenVR con 30/30 tests, metadata, verificador BP y Overture `Release -Full`. Overture adicionalmente pasó 289 checks, 16 shaders, 8.752 comprobaciones visuales CPU y selección/decodificación de 231 texturas.
>
> Body adapter, shadow y frontera física BP tienen evidencia live. PID 8092 validó técnicamente stick métrico dentro del gate room-scale. El crouch actual `ChangeMoveState(4/0)` sólo estaba host-tested. El pullback de desplazamientos físicos cortos seguía abierto. No promociones estados de validación sin pruebas nuevas.
>
> **Arquitectura**
>
> Overture es un producto autónomo fuente: `Game.cpp → ButtonHandler/Player → OvertureSourceIntegration → OvertureBackend → runtime + HPL adapters`.
>
> BP usa launcher Steam/carga remota y `PenumbraVR_Initialize`. Instala OpenGL telemetry, RenderWorld/visibilidad, SDL swap, NativeInputBridge, BodyCollisionProbe, BodyAdapter, movement probe e interacción.
>
> Owners exactos BP:
>
> - NativeInputBridge: consultas y callsites de movimiento.
> - `D460A → D6E00`: único tick nativo del character body.
> - `D7312 → D4830`: solver horizontal observado.
> - `D7281`: inyección X/Z antes de resolución.
> - `EDF84 → 12A8F0`: visibilidad, anterior a RenderWorld.
> - `EE010 → 12CB10`: RenderWorld.
>
> No instales hooks superpuestos ni llames otro `D6E00`.
>
> **Diagnóstico confirmado**
>
> 1. `body_reconciliation_shadow.cpp::Observe` se ejecuta post-tick, calcula `PlanBodyReconciliation(anchor_, body_before, physical_delta)` y el adapter encola el resultado para otro tick. Mezcla épocas. Una rampa libre uniforme puede producir peticiones oscilantes.
> 2. `render_world_probe.cpp` adquiere HMD fresco en RenderWorld, pero la visibilidad anterior consume caches anteriores. Controllers se adquieren en input. Faltan identidad y épocas comunes de pose/yaw.
> 3. `native_input_bridge.cpp::HookedQuery` devuelve false para crouch incluso sin VR activo. La política diferida no procesa esa pulsación sin gameplay enfocado. El owner de crouch tampoco está ligado correctamente a la generación del jugador.
> 4. `HookedSideways` llama a locomoción con `constrained=false` y calcula metros con dt de input. Seated permanece en `OvertureBackend::UpdatePlayMode`. La política común de crouch no recibe el bloqueo de stand que protege el baseline en Rework.
> 5. `spatial_interaction.cpp` selecciona mediante aim/ray corto pero adquiere con grip, sin revalidar contacto/distancia/oclusión. No reproduce la palma resuelta por colisiones de Rework. El fallback nativo puede confundirse con selección VR. Un mismatch de jugador puede dejar hold huérfano.
> 6. Inicialización ignora algunos errores de rollback y vuelve a estado limpio; shutdown puede volver a listo estando parcialmente desmontado. `rel32_call_hook.cpp::SuspendOthers` asigna memoria después de suspender otros threads.
>
> No se demostró un world scale global erróneo. Rework calibra altura mediante `(y−0.2)*1.065` pero conserva distancias relativas mediante transformación rígida. No cambies esa fórmula como compensación.
>
> **Fase 1: ownership de crouch**
>
> Modifica `src/backends/black_plague/native_input_bridge.*` y `src/runtime/vr_crouch_policy.*`.
>
> Define ownership nativo/VR/liberación pendiente ligado a sesión y generación de jugador. Preserva consultas nativas fuera de VR. Adopta edges nativos al latch común sólo cuando VR sea owner. Aplica liberación en game thread. Mantén `ChangeMoveState(4/0)`; no vuelvas a toggles compensatorios. Distingue deseo, estado efectivo y bloqueo de stand; protege baseline como Rework.
>
> Crea un harness que pruebe el bridge real, no sólo `VrNativeIntents`: sin sesión, focus, disconnect, cambio de jugador, doble edge y techo bajo.
>
> **Fase 2: lifecycle**
>
> Modifica `src/probe/black_plague_probe.cpp`, `src/hooks/rel32_call_hook.cpp` y reporting del launcher.
>
> Mantén ledger de componentes y estado parcial explícito. Ready significa conjunto requerido instalado; rollback fallido no significa limpio. Shutdown debe poder reintentarse. Conserva observabilidad y memoria mientras haya callbacks residentes. Prepara handles/almacenamiento antes de suspender; no asignes ni formatees errores en la región suspendida. Timeout remoto significa indeterminado.
>
> Prueba fallos en cada instalación/retirada, callbacks durante startup y retry de shutdown.
>
> **Fase 3: transacción por tick**
>
> Modifica `body_collision_probe.*`, `black_plague_body_callbacks.hpp`, `body_adapter_boundary.*`, `black_plague_body_adapter.*`, `body_reconciliation_shadow.*` y publicación desde NativeInputBridge.
>
> En el wrapper existente, antes de `g_original_update`, resuelve B0 e identidad, consume la última pose publicada y una intención lógica, integra delta HMD una vez y calcula locomoción con dt físico. Asocia petición a secuencia monotónica de tick, cuerpo, generación y época. `D7281` consume una vez. Después observa B1, verifica matching, reconcilia una vez, aplica carry una vez y publica ancla.
>
> No esperes al compositor desde física. Shadow usa el mismo planner sin escrituras. Conserva clamp horizontal combinado `0.05 m`, prioridad de input nativo y Y/jump nativos.
>
> Reutiliza `PlanBodyReconciliation`, `ReconcilePhysicalBodyMotion` y `CarryHeadAnchorWithLocomotion`. Elimina planificación del siguiente tick contra B0 de un tick ya terminado.
>
> Prueba rampas, parada, 60 Hz físico con 72/90/120 Hz render, muestras repetidas, pared/slide/jitter, recenter y cambios de cuerpo. El total aceptado es observado; su reparto físico/stick es derivado. No afirmes que una resolución combinada equivale a dos resoluciones independientes.
>
> **Fase 4: snapshot, yaw y postura común**
>
> Extiende `vr_tracking_types.hpp` con identidad/tiempo/épocas. Adquiere una muestra de presentación en el owner anterior de visibilidad y reutilízala para ojos. Define fallback para menús. Separa poses de edges para no duplicar input.
>
> Cámara, manos, herramientas y picking consumen transformaciones identificadas. Predice sólo delta HMD posterior al sample del cuerpo dentro de la misma época de yaw.
>
> Extrae `OvertureBackend::UpdatePlayMode` a política común, por ejemplo `vr_play_mode_policy.*`, y conecta BP. Completa crouch compartido conservando la aplicación nativa en adapters. Mapea restricciones BP mediante evidencia exacta; no copies enums Overture. Conserva `1.5 m/s`, sprint `2.25`, restringido `0.5`.
>
> Alinea ownership VR de yaw con tracking-world-yaw de Rework y adapta cambios nativos externos mediante delta/rebase explícito. Valida yaw en gate separado.
>
> Actualiza source integration y `PenumbraVR.Framework.Overture.props`; cambia el núcleo neutral de ButtonHandler sólo con tests diferenciales.
>
> **Fase 5: contacto e interacción**
>
> En `spatial_interaction.*`, unifica grip/palma, valida contacto al adquirir, impide adquisición VR por fallback remoto y dibuja alcance real. Liga hold a generación. Restaura una vez mientras el cuerpo siga vivo; tras destrucción demostrada descarta sin escribir memoria caducada.
>
> Conserva composición de `vr_grab_pose`, sockets medidos y articulación BP. Define grip→palma→modelo mediante perfiles; no inventes offsets globales.
>
> Para palma completa, porta desde Rework `Player.cpp::CheckVRHandWorldCollision`, `FindVRHandRecoveryPose` y actualización de colisión de manos. Extrae matemática/contactos/recuperación a `vr_hand_contact.*`; deja queries/shapes/HPL en adapters.
>
> **Frontera pendiente acotada:** aún no está demostrada la ABI BP equivalente a `CheckShapeWorldCollision`, callbacks y lifetime de shapes. Localízala mediante código HPL y xrefs del build soportado, documenta firma, verifica bytes y prueba query sin escrituras antes de conectarla. No inventes RVAs ni reabras el body mapping ya completado.
>
> Mantén mechanisms/joints en su ruta nativa hasta adaptar sus estados reales. Free-body no es soporte VR de mecanismos.
>
> **Validación automática**
>
> Ejecuta desde `E:\penumbra_vr`:
>
> ```
> cmake --preset vs2022-win32
> cmake --build --preset debug
> ctest --preset debug --output-on-failure
> cmake --build --preset release
> ctest --preset release --output-on-failure
> cmake -S . -B build-no-openvr -G "Visual Studio 17 2022" -A Win32 -DBUILD_TESTING=ON -DPENUMBRA_VR_OPENVR_SDK=
> cmake --build build-no-openvr --config Release
> ctest --test-dir build-no-openvr -C Release --output-on-failure
> .\tools\Test-PenumbraVrMetadata.ps1
> .\tools\Test-BlackPlagueInputMap.ps1 -ImagePath .\artifacts\black-plague-22000-live.bin
> .\tools\Build-OvertureProduct.ps1 -Configuration Release -Full
> git diff --check
> ```
>
> La captura BP mencionada contiene hooks de render instalados. Su verifier pasa sobre un subconjunto; no es prueba de imagen totalmente prístina. No normalices evidencia silenciosamente.
>
> **Validación live**
>
> Usa `tools/Start-BlackPlagueRoomScaleValidation.ps1` y un save registrado con suelo plano, pared, esquina, paso bajo, caja y cuerpo largo. Prueba: native-only crouch; activar/parar VR; quietud/tilt; desplazamientos físicos cortos; pared/slide; stick/sprint con distintos yaw; físico+stick opuestos; dos ciclos físicos crouch `4/0` con shapes `0.95/1.65`; botón/Hybrid/techo; seated/recenter; contactos de cuerpo largo; herramientas; pérdida de tracking y cambio de mapa agarrando.
>
> Registra de forma acotada generaciones, pose/tick/yaw, B0/B1, petición, total aceptado, reparto derivado, carry, ancla, predicción y correlación de postura. No apruebes crouch sólo con contadores acumulados.
>
> **Aceptación y restricciones**
>
> Overture sin regresión; una actualización nativa por tick; ninguna muestra integrada dos veces; sin petición oscilante libre ni pullback percibido; coherencia métrica; input nativo preservado; postura y hold con ownership correcto; contacto/visual/palma alineados; lifecycle parcial descrito honestamente; tests/verifier correctos.
>
> Mantén default-off hasta cerrar los gates. No retunes salto/gravedad/radio de cuerpo sin nueva evidencia; no añadas collider de cabeza, hooks superpuestos, segundo `D6E00`, offsets arbitrarios ni abstracciones para Requiem. No degradas dedos BP ni reescribas el loop, interacción probada o renderer completo de Overture.
>
> Mirror/focus es gate separado. El crash SDL histórico no tiene causa demostrada: si reaparece, captura dump y módulos. No lo atribuyas al mutex de activación ni al shadow por proximidad temporal.
>