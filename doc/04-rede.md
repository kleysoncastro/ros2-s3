# 4. Rede: Agent e ROS 2 espalhados

A pergunta: se o Agent roda numa rede, dá para ter hosts ROS 2 Jazzy, o Agent e vários
ESP32 (micro-ROS) espalhados e tudo se enxergar bastando os IPs estarem corretos?
Quase. A rede tem **dois trechos** com regras diferentes:

```
ESP32  --(1) XRCE-DDS-->  Agent  --(2) DDS/RTPS-->  ROS 2 Jazzy (outros hosts)
```

## Trecho 1: ESP32 → Agent

- O ESP32 só precisa saber o **IP e a porta do Agent**. Ele não conhece os hosts ROS 2.
- Pela rede, o componente usa **UDP** por padrão. O TCP está desligado no `colcon.meta` do
  componente (`UCLIENT_PROFILE_TCP=OFF`); para usar TCP seria preciso habilitar e
  recompilar.
- Vários ESP32 podem apontar para o mesmo Agent, ou cada grupo para um Agent.
- Hoje o projeto usa **USB serial**, então a placa precisa estar plugada no host do Agent.
  Para espalhar placas pela rede, trocar para o transporte Wi-Fi/UDP do componente (o
  exemplo `int32_publisher`, com o IP do Agent no `menuconfig`).

## Trecho 2: Agent → ROS 2

- É **DDS normal**, não TCP. O Agent é só mais um nó ROS 2 da rede.
- Os outros hosts enxergam os tópicos se:
  - estiverem na **mesma sub-rede**;
  - usarem o **mesmo `ROS_DOMAIN_ID`**;
  - a descoberta (multicast UDP) chegar até eles;
  - o firewall permitir UDP (portas a partir de 7400).
- Em Wi-Fi, o multicast pode ser bloqueado ou degradado pelo roteador. Entre sub-redes ou
  VPNs ele geralmente não passa. Alternativas: **Fast DDS Discovery Server** ou configurar
  peers por unicast.
- No Docker, usar `--net=host`. Em rede bridge a descoberta falha.

## Resumo

Com os IPs certos o ESP32 enxerga o Agent. Os hosts ROS 2 se enxergam entre si e com o Agent
pelo DDS, e isso depende de domínio, sub-rede e multicast, não só de IP. Ter o ROS 2 em um
host e o Agent em outro funciona, atendidos esses requisitos do trecho 2.
