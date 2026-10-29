# Latencia y modos
Dos modos, como el Auto-Tune Realtime de UAD (Classic / Modern). El plugin informa la latencia real al host al cambiar de modo.

| Modo | Grano WSOLA | Latencia | A 48 kHz | Comportamiento |
|---|---|---|---|---|
| CLASSIC | 64 | 64 muestras | 1.33 ms | Correccion directa; Retune Speed manda; Humanize y Flex-Tune desactivados |
| MODERN | 192 | 192 muestras | 4.0 ms | Humanize y Flex-Tune activos; mas natural en voces graves |

Referencia UAD (segun soporte de UA): Classic ~50-100 muestras, Modern ~175-300 muestras a 44.1 kHz.
Valores teoricos, no medidos en un DAW. Cambiar de modo reinicia el buffer del shifter (puede oirse un corte breve).

No incluido respecto al de UAD: Vibrato, control de notas por MIDI y Formant Preserve (el boton existe pero no actua).
