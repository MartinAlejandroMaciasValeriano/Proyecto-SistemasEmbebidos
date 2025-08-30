# Documento de Diseño - Inventario Inteligente LAB Embebidos

**Autor:** Macias Valeriano Martin Alejandro  
**Matrícula:** 202214011  
**Materia:** Sistemas Embebidos  
**Fecha:** 29 de agosto del 2025  

---

## 1. Introducción

Este documento describe el diseño de un sistema de inventario inteligente implementado con tecnologías embebidas. El objetivo es gestionar la disponibilidad y préstamo de componentes electrónicos en un laboratorio, utilizando una Raspberry Pi como servidor local y una ESP32 para el control de luces indicadoras. El sistema busca eficiencia, trazabilidad y visualización en tiempo real.

---

## 2. Alcance y Limitaciones

**Alcance:**
- Registro de entradas y salidas de componentes.
- Control de iluminación para ubicación rápida en el inventario.
- Visualización en monitor o interfaz web local.
- Comunicación entre Raspberry Pi y ESP32.

**Limitaciones:**
- No se implementa autenticación avanzada.
- No hay conexión a la nube.
- Sin soporte para múltiples usuarios simultáneos.

---

## 3. Diagrama de Contexto

El sistema se representa como una **caja negra** con entradas y salidas claramente definidas:

**Entradas:**
- Usuario (acciones: prestar, devolver, agregar, descontar).
- Teclado y mouse.
- Datos almacenados en la base de datos (SQLite).

**Salidas:**
- Pantalla HDMI (interfaz gráfica).
- Interfaz web en red local.
- LEDs WS2812B indicando el cajón correspondiente.
- Reportes de inventario.

*Ilustración 1: Diagrama de contexto con entradas y salidas*  

---

## 4. Diagrama de Bloques del Sistema

**Componentes principales y sus interconexiones:**
- Raspberry Pi:
  - Gestiona la base de datos SQLite.
  - Ejecuta la interfaz gráfica / web.
  - Se comunica con la ESP32 vía WiFi/Serial.
- ESP32:
  - Controla las tiras LED WS2812B.
  - Recibe comandos de la Raspberry Pi.
- Periféricos:
  - Entrada: teclado y mouse USB.
  - Salida: pantalla HDMI, LEDs indicadores.

*Ilustración 2: Diagrama de bloques con interconexiones y protocolos*  

---

## 5. Máquina de Estados

La máquina de estados describe el flujo lógico del sistema:

1. **Inicialización** → Configuración de red, LEDs y base de datos.  
2. **Espera de acción** → El sistema aguarda una instrucción del usuario.  
3. **Registrar préstamo** → Se actualiza la base de datos y se ilumina el cajón.  
4. **Registrar devolución** → Se incrementa la cantidad disponible en la base de datos.  
5. **Agregar/Descontar stock** → Ajuste manual del inventario.  
6. **Ubicar cajón** → LEDs parpadean indicando la posición física.  
7. **Error/Validación fallida** → Mensaje al usuario, LEDs rojos.  
8. **Confirmación** → Se muestra mensaje de éxito y se retorna a “Espera”.

*Ilustración 3: Diagrama de máquina de estados detallada*  

---

## 6. Diseño de Interfaces

**Interfaces físicas y lógicas:**
- **Pantalla HDMI** → interfaz visual con menús de registro.  
- **Interfaz web (ESP32)** → control remoto desde navegador.  
- **Teclado/mouse USB** → entrada de datos por parte del usuario.  
- **LEDs WS2812B** → señalización física de cajones.  

El sistema busca que la interacción sea intuitiva: al seleccionar un componente, automáticamente se ilumina el cajón correspondiente.

---

## 7. Alternativas de Diseño

**Opción 1: Arduino UNO + LCD**  
- Pros: bajo costo, simplicidad.  
- Contras: sin conectividad nativa a red, limitada memoria y almacenamiento.  

**Opción 2: ESP32 + Raspberry Pi (seleccionada)**  
- Pros: conectividad WiFi integrada, mayor procesamiento, manejo de base de datos local, escalabilidad.  
- Contras: costo mayor comparado con Arduino, mayor complejidad de configuración.  

Evidencia: La Raspberry Pi permite correr SQLite y servidor local, mientras la ESP32 maneja periféricos y LEDs, asegurando modularidad y escalabilidad.

---

## 8. Plan de Prueba y Validación

Se define que **“funcional”** significa que el sistema realiza correctamente la tarea prevista en al menos un **95% de los casos de prueba**.

**Criterios de validación:**
- Conexión estable Raspberry Pi – ESP32.  
- Tiempo de respuesta < 2s en iluminación de LEDs al registrar préstamo.  
- Base de datos consistente después de cada operación.  
- Interfaz web accesible desde cualquier dispositivo en la red local.  
- Pruebas de estrés con múltiples operaciones consecutivas.

---

## 9. Consideraciones Éticas

- El sistema evita pérdidas y promueve el uso responsable de los recursos.  
- No se almacenan datos personales sensibles.  
- Se fomenta la transparencia y el orden en el laboratorio.

---

## 10. Conclusiones y Recomendaciones

El diseño propuesto integra hardware, software e interfaces de usuario de manera eficiente. La **Raspberry Pi** actúa como núcleo de gestión de datos y servidor, mientras la **ESP32** maneja la interacción física con LEDs.  

**Recomendaciones:**
- Implementar autenticación básica de usuarios.  
- Realizar respaldos automáticos de la base de datos.  
- Desarrollar una interfaz gráfica más amigable.  
- Escalar a control remoto vía dispositivos móviles y nube.  

---

## Anexo: Respuestas a Retroalimentación

- **Contexto:** El diagrama fue rediseñado como caja negra con entradas y salidas estándar.  
- **Bloques:** Se añadieron flechas y protocolos (WiFi, Serial, GPIO).  
- **Estados:** Se desarrolló una máquina de estados con 8 estados principales.  
- **Interfaces:** Se detallaron todas las interacciones físicas y lógicas.  
- **Alternativas:** Ahora se incluyen comparaciones con evidencias.  
- **Validación:** Se definió claramente que significa “funcional” con criterios medibles.  

---
