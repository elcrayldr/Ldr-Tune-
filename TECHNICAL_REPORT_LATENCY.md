# Informe técnico de latencia (valores de código, no medición end-to-end)

El plugin informa al host el retardo fijo del camino de audio en cada modo:

| Modo | Tamaño de grano | Retardo declarado | A 48 kHz |
|---|---:|---:|---:|
| CLASSIC | 64 muestras | 32 muestras | 0,667 ms |
| MODERN | 192 muestras | 96 muestras | 2,000 ms |

Estos números salen de getLatencySamples() = grainSize / 2 y del retardo del camino de ganancia unitaria. No incluyen el búfer ASIO, el búfer de FL Studio, la conversión de la interfaz ni la monitorización directa/por software. No son una medición acústica o de loopback del sistema completo.

La compensación de latencia del host alinea pistas durante reproducción; no elimina el retardo percibido al monitorizar una voz en directo.

## Limitaciones detectadas

- El desplazador de tono actual usa dos cabezales con interpolación lineal. Que sea de baja latencia no implica que sea transparente ni que preserve formantes.
- El control Formant Preserve está expuesto en la interfaz, pero actualmente no está conectado al DSP.
- Cambiar de modo llama a setGrainSize() desde el callback de audio y reinicia el búfer. Esto puede provocar un corte o un pico de carga en el instante del cambio; debe trasladarse a una transición segura.
- Los tests anteriores solo comprobaban que la salida fuera finita y acotada; no verificaban la frecuencia de salida ni la calidad perceptiva. La CI ahora ejecuta los tests DSP, pero aún hace falta ampliar las pruebas de respuesta en frecuencia y medir la latencia real en loopback.

No se debe anunciar este plugin como de latencia cero ni como preservador de formantes hasta que esas propiedades se hayan implementado y medido.
