# 1. Configuração e dependências de build

## Ambiente do ESP-IDF (`get_idf`)

`get_idf` é um alias/função do shell (zsh) deste computador que carrega o ambiente do ESP-IDF.
Depois dele ficam disponíveis `idf.py`, o toolchain e o Python do IDF:

| Item | Valor encontrado |
|---|---|
| `IDF_PATH` | `~/.espressif/v6.1/esp-idf` |
| ESP-IDF | v6.1 |
| Python do IDF (venv) | `~/.espressif/tools/python/v6.1/venv/bin/python` (3.12.3) |

Uso:

```bash
get_idf
idf.py build
```

Os pacotes Python abaixo precisam ser instalados **dentro desse venv**. Por isso o `pip`
só deve ser rodado depois do `get_idf`.

## Por que o primeiro build falhou

O componente `micro_ros_espidf_component` não traz o micro-ROS pronto. No build ele baixa
os fontes do ROS 2 e compila tudo com `colcon`. O erro foi:

```
/bin/sh: 9: colcon: not found
make: *** [libmicroros.mk:33: .../micro_ros_dev/install] Error 127
CMake Error at components/micro_ros_espidf_component/CMakeLists.txt:81 (message):
  FAILED: 2
```

## Pacotes Python necessários

O README do componente pede: `catkin_pkg`, `colcon-common-extensions` e `lark`.
Instalamos também o `empy`, fixado na versão 3.3.4.

```bash
get_idf
pip install catkin_pkg colcon-common-extensions lark
pip install "empy==3.3.4"
```

Observações:

- **`empy==3.3.4`:** o `pip` instalou a 4.2.1 por padrão. O ROS 2 Jazzy usa a série 3.x
  nos geradores de mensagens (a série 4.x é incompatível). Fixei a 3.3.4 de forma
  preventiva; não cheguei a ver o erro da 4.x acontecer.
- **`setuptools`:** a instalação do `colcon` rebaixou o `setuptools` do venv de
  84.2.1 para 79.0.1. O build passou, mas vale saber caso outra ferramenta do IDF reclame.
- O primeiro build é demorado (clona e compila o ROS 2 mínimo). Os seguintes são rápidos.

## Como o micro-ROS entrou no projeto

```bash
cd components
git clone --depth 1 -b jazzy https://github.com/micro-ROS/micro_ros_espidf_component.git
```

- Branch `jazzy`, versões `24.x.y`. O README diz que foi testado com ESP-IDF 5.2 a 6.0
  e com ESP32-S3 entre os targets.
- O `.git` do clone ficou dentro da pasta. Se o projeto for versionado, decidir entre
  submódulo ou remover esse `.git`.

### Arquivos de configuração do projeto

| Arquivo | Função |
|---|---|
| `app-colcon.meta` | Põe o `rmw_microxrcedds` em `-DRMW_UXRCE_TRANSPORT=custom` (transporte próprio, no caso USB-CDC). O componente lê este arquivo da raiz do projeto. |
| `sdkconfig.defaults` | Ativa `MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE`, `MICRO_ROS_ESP_UART_TRANSPORT`, `TINYUSB_CDC_ENABLED` e `TINYUSB_CDC_COUNT=2`. |
| `main/Kconfig.projbuild` | Stack e prioridade da task do micro-ROS, e o GPIO do LED. |
| `main/idf_component.yml` | Dependência `espressif/led_strip`. |

**Atenção:** o `sdkconfig.defaults` só vale quando não existe `sdkconfig`. Se mudar os
defaults, apague o `sdkconfig` e o `build/`:

```bash
rm -rf build sdkconfig
idf.py set-target esp32s3
idf.py build
```

## `components/` e `managed_components/`

| | `components/` | `managed_components/` |
|---|---|---|
| Origem | Colocado por nós (clonado ou copiado) | Baixado pelo IDF Component Manager |
| Pode editar? | Sim | Não (some na próxima resolução) |
| Git | Entra | Normalmente ignorado |
| Versões | O seu git | `dependencies.lock` |

Em `components/`:

- `micro_ros_espidf_component`: o micro-ROS.
- `esp_usbcdc_transport`: liga o micro-ROS ao CDC ACM da USB (`open/close/read/write`).
- `esp_usbcdc_logging`: manda os `ESP_LOGx` para a segunda porta CDC.
  (Os dois últimos foram copiados de `examples/int32_publisher_custom_transport_usbcdc`.)

Em `managed_components/`:

- `espressif__tinyusb`: a pilha USB TinyUSB.
- `espressif__esp_tinyusb`: adaptador do TinyUSB para o ESP-IDF (CDC ACM).
- `espressif__usb`: pilha USB host. O manifesto do `esp_usbcdc_*` a pede quando o IDF é
  6.0 ou mais novo. Não verifiquei o motivo exato.
- `espressif__led_strip`: driver do LED WS2812 (RMT).

Quando um `idf_component.yml` é adicionado ou alterado, o gerenciador baixa a dependência
na próxima configuração. Na primeira vez o `led_strip.h` não foi encontrado até rodar
`idf.py reconfigure`.

## Fluxo de build e gravação (resumo)

```bash
get_idf
idf.py build
# entrar em modo download (ver capítulo 2), depois:
idf.py -p /dev/ttyACM0 flash
```

Tamanho atual do firmware: ~297 KB. O `flash-size` está em 2 MB (default) e pode ser
ajustado no `menuconfig` se a placa tiver mais.
