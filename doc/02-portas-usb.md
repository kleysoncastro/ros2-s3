# 2. Portas USB (ttyACM0 e ttyACM1) e modo download

## Quem cria as portas e por quê

O firmware usa a **USB nativa** do ESP32-S3 (pinos D+/D−) como dispositivo USB, via TinyUSB.
Com `CONFIG_TINYUSB_CDC_COUNT=2` ele se apresenta ao computador como um dispositivo com
**duas interfaces CDC-ACM** (porta serial virtual):

```
lsusb:  303a:4002  Espressif Systems Espressif Device
/dev/serial/by-id/usb-Espressif_Systems_Espressif_Device_123456-if00 -> ../../ttyACM0
/dev/serial/by-id/usb-Espressif_Systems_Espressif_Device_123456-if02 -> ../../ttyACM1
```

Quem monta o quê:

1. **O firmware** (`esp_tinyusb` + TinyUSB) declara as duas interfaces CDC.
2. **O kernel Linux** (driver `cdc_acm`) vê cada uma e cria `/dev/ttyACMn`.
3. **O udev** define dono e permissão do nó (`root:dialout`, modo `0660`).

| Porta | Uso | Quem a abre |
|---|---|---|
| `/dev/ttyACM0` (interface `if00`) | **Transporte micro-ROS** (XRCE-DDS nos dois sentidos) | O micro-ROS Agent |
| `/dev/ttyACM1` (interface `if02`) | **Logs** (`ESP_LOGI` etc.) | Você, com um terminal serial |

Notas:

- Só o Agent usa a `ttyACM0`. Se outro programa abrir essa porta, o Agent perde o acesso.
- A numeração pode inverter ou mudar se outros dispositivos USB forem plugados. O caminho
  fixo `/dev/serial/by-id/...-if00` evita essa dependência.
- O `rclc_support_init` só retorna quando o Agent conecta. Sem Agent, o firmware não
  publica nem imprime os `Published`. Os logs de boot saem antes de a porta ser aberta.

## Permissão

As portas pertencem ao grupo `dialout`, e o usuário `castro` **não está** nesse grupo
(grupos atuais: adm, cdrom, sudo, dip, plugdev, users, lpadmin, libvirt, docker).

Correção definitiva (exige logout/login ou `newgrp dialout`):

```bash
sudo usermod -aG dialout $USER
```

Solução temporária usada (vale até desconectar a placa):

```bash
sudo chmod 666 /dev/ttyACM0 /dev/ttyACM1
```

O Agent em Docker com `--privileged` roda como root e não precisa disso.

## Ler os logs

O `idf.py monitor` não serve nessa porta (ele tenta resetar a placa pelas linhas DTR/RTS,
e o CDC do TinyUSB não faz isso). Use um terminal serial simples:

```bash
python -m serial.tools.miniterm /dev/ttyACM1 115200
```

## Por que o `idf.py flash` falha com o firmware rodando

Com a USB nativa nas mãos do TinyUSB, o `esptool` não consegue mandar a placa para o
bootloader automaticamente. No projeto antigo (sem TinyUSB) a placa aparecia como
USB-Serial/JTAG, que sabe se resetar para o bootloader. Agora é preciso entrar em modo
download manualmente.

## Modo download: BOOT + RESET/EN

1. Segure o botão **BOOT**.
2. Aperte e solte **RESET** (ou **EN**, o nome varia conforme a placa).
3. Solte o **BOOT**.
4. Confira com `lsusb | grep 303a`. Em modo download o ID deve ser diferente de
   `303a:4002` (esperado `303a:1001`, USB-Serial/JTAG da ROM; este valor é de memória, não
   foi registrado nos testes).
5. Grave (veja qual porta apareceu com `ls /dev/ttyACM*`):

   ```bash
   idf.py -p /dev/ttyACM0 flash
   ```

6. Aperte **RESET/EN** de novo para executar o firmware novo.

Alternativa: se a placa tiver duas portas USB, gravar pela porta da ponte UART evita o
procedimento manual. O firmware continua usando a USB nativa para o micro-ROS.
