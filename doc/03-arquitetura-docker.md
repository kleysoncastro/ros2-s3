# 3. Arquitetura: ESP32-S3, Agent e imagens Docker

## Visão geral

```
ESP32-S3             micro-ROS Agent                ROS 2 Jazzy
(cliente micro-ROS)  (tradutor / proxy)             (ros2 topic, nós, etc.)

  rclc / XRCE-DDS --USB--> ttyACM0 --> Agent --DDS--> ros2 topic echo ...
```

## O ESP32-S3 como cliente micro-ROS

- O ESP32 **não roda o ROS 2 completo**. Roda o **micro-ROS**: `rcl`, `rclc` e
  `rmw_microxrcedds`, uma versão enxuta pensada para microcontroladores.
- Ele fala **XRCE-DDS**, uma versão leve do DDS. Por isso não conversa diretamente com
  nós ROS 2 comuns.
- O firmware é escrito em **C com a API `rclc`** (`main/main.c`): inicia o suporte, cria um
  nó, um publisher, um subscriber e um timer, e roda um executor.
- Aqui:
  - publica `std_msgs/Int32` em `/esp32s3/counter` (1 por segundo);
  - assina `std_msgs/ColorRGBA` em `/esp32s3/led` (r, g, b de 0.0 a 1.0).

## O micro-ROS Agent

O **Agent** é um programa no PC que faz a ponte: recebe o XRCE-DDS pela serial e se
apresenta ao ROS 2 como se fosse o nó do ESP32. Para o ROS 2, `/esp32s3/counter` parece um
tópico comum. Sem o Agent, o `ros2 topic list` só mostra `/parameter_events` e `/rosout`.

O ESP32 não aparece como "nó ROS 2 completo": ele depende do Agent estar rodando.

## As imagens Docker

| Imagem | Contém | Para quê |
|---|---|---|
| `ros:jazzy` | ROS 2 Jazzy completo (`ros2 topic`, etc.) | O lado ROS 2: listar, ouvir e publicar tópicos. **Não** tem o Agent. |
| `microros/micro-ros-agent:jazzy` | ROS 2 Jazzy + o micro-ROS Agent | Só para rodar o Agent. |

Dava para instalar o Agent dentro da `ros:jazzy` com `apt` (pacote
`ros-jazzy-micro-ros-agent`, não verificado). A imagem pronta é mais garantida.

### Terminal 1: Agent

```bash
docker run -it --rm -v /dev:/dev -v /dev/shm:/dev/shm --privileged --net=host \
  microros/micro-ros-agent:jazzy serial --dev /dev/ttyACM0 -v4
```

- `-v /dev:/dev` e `--privileged`: acesso à porta serial.
- `--net=host`: o DDS descobre os nós por multicast; em rede bridge isso quebra.
- `-v /dev/shm:/dev/shm`: o Fast DDS usa memória compartilhada entre processos/containers.

Com o Agent no ar, aperte RESET na placa. Deve aparecer `session established`,
`create_participant`, `create_topic`, `create_publisher`.

### Terminal 2: ROS 2

```bash
docker run -it --rm --net=host -v /dev/shm:/dev/shm ros:jazzy bash
```

Dentro do container:

```bash
source /opt/ros/jazzy/setup.bash
ros2 topic list
ros2 topic echo /esp32s3/counter

# acender o LED (cor vermelha):
ros2 topic pub --once /esp32s3/led std_msgs/msg/ColorRGBA "{r: 1.0, g: 0.0, b: 0.0, a: 1.0}"
```

Na `ttyACM1` aparece o log da placa, por exemplo `LED r=1.00 g=0.00 b=0.00`.

## Armadilha encontrada

Um `ros2 topic list` sem o Agent rodando não mostra o tópico da placa. O container `ros:jazzy`
sozinho não fala XRCE. Antes de depurar a placa, confira com `docker ps` se o container do
Agent está de pé.
