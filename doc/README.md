# Anotações: micro-ROS no ESP32-S3

Projeto: ESP32-S3 + ESP-IDF 6.1 + micro-ROS (ROS 2 Jazzy), transporte USB (CDC).
Linguagem do firmware: C (API `rclc`).

## Estado atual

- Compila e grava no ESP32-S3.
- A placa publica `std_msgs/Int32` em `/esp32s3/counter` a cada 1 s.
- A placa assina `std_msgs/ColorRGBA` em `/esp32s3/led` (a mensagem chega, o log aparece,
  mas o LED RGB da placa **não acendeu**; ver "Pendências").
- Comunicação com o ROS 2 via micro-ROS Agent em Docker, pela USB.

## Capítulos

1. [Configuração e dependências de build](01-configuracao-build.md)
2. [Portas USB (ttyACM0/1) e modo download](02-portas-usb.md)
3. [Arquitetura: ESP32-S3, Agent e imagens Docker](03-arquitetura-docker.md)
4. [Rede: Agent e ROS 2 espalhados](04-rede.md)

## Pendências

- LED RGB não acende. Testado GPIO48 e GPIO38 (`CONFIG_LED_STRIP_GPIO`). Falta confirmar a
  versão da placa e o pino no esquemático. Pode também ser um LED comum, e não WS2812.
- Transporte Wi-Fi/UDP (hoje só USB).
