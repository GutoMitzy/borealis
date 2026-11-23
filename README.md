![borealis logo](https://github.com/natinusala/borealis/blob/main/resources/img/borealis_96.png?raw=true)

# Borealis — Joy-Con Motion Fork

Este repositório é um fork da biblioteca de interface [Borealis](https://github.com/xfangfang/borealis), originalmente criada por [natinusala](https://github.com/natinusala/borealis) e mantida e ampliada pela comunidade. **Este fork é usado como submódulo do [Moonlight-Switch Joy-Con Motion](https://github.com/GutoMitzy/Moonlight-Switch)**, com alterações na camada de entrada HID do Nintendo Switch.

## O que é Borealis?

Biblioteca de interface C++ voltada para navegação com controles e telas de TV em plataformas como Nintendo Switch, PC e dispositivos móveis. Entre os recursos gerais do projeto original estão:

- Interface inspirada na experiência do Nintendo Switch.
- Renderização acelerada por hardware e gráficos vetoriais com **NanoVG**.
- Layouts flexíveis baseados em **Yoga Layout**, escalonamento de interface para TV e navegação automática por gamepad.
- Entrada por touchscreen e controles, definição de interfaces por XML e componentes reutilizáveis.
- Internacionalização, armazenamento, logger, animações, tarefas e controles de listas eficientes.

Consulte o [projeto original](https://github.com/xfangfang/borealis) e a [wiki](https://github.com/xfangfang/borealis/wiki) para plataformas, exemplos e utilização geral da biblioteca.

## O que este fork acrescenta?

A camada Switch de Borealis foi adaptada para necessidades específicas do Moonlight-Switch:

- **Mapeamento de Joy-Cons:** enumeração lógica de metades esquerda e direita, inclusive dois pares conectados e determinados arranjos físicos identificados como `JoyDual`.
- **Identidade dos controles:** tentativas de preservar slots lógicos ao longo de reconexões e mudanças na ordem dos dispositivos; a identidade no Sunshine e no emulador continua dependendo do host.
- **Sensores SixAxis:** leitura do giroscópio e acelerômetro por controle, com validação de amostras e rejeição de sequências antigas ou repetidas.
- **Calibração do viés angular:** calibração automática e manual por sensor com repouso HID, aceleração próxima de 1 g e amostras consecutivas. Uma lacuna reinicia a coleta. Uma calibração válida continua em uso durante a coleta manual, e o ajuste automático converge gradualmente, inclusive em eixos cujo viés anterior era zero.
- **Eventos com tempo de amostragem:** o `SensorEvent` oferece estado de repouso e duração válida do intervalo HID.
- **Recuperação de amostras em frames lentos:** até oito amostras SixAxis são capturadas por leitura e reproduzidas da mais antiga para a mais recente, para reduzir perda de movimento durante oscilações da renderização do dock.

Essas mudanças **não criam por si só uma ponte DSU** e não redefinem a orientação interna do emulador. A biblioteca continua sendo uma infraestrutura de entrada e interface, não um substituto do Sunshine ou do Fritchii. O comportamento em consoles físicos precisa ser validado.

## Usar com o Moonlight-Switch

O método recomendado é utilizar o submódulo já fixado na versão do Moonlight, evitando incompatibilidade entre as estruturas `SensorEvent` do aplicativo e da biblioteca:

```bash
git clone --recurse-submodules -b joycon-fidelity-v20 https://github.com/GutoMitzy/Moonlight-Switch.git
cd Moonlight-Switch
git submodule update --init --recursive
```

Depois compile o cliente conforme o [README do Moonlight-Switch](https://github.com/GutoMitzy/Moonlight-Switch/tree/joycon-fidelity-v20#compila%C3%A7%C3%A3o).

Use a revisão fixada no submódulo para manter compatibilidade com os eventos de movimento e as consultas de topologia dos Joy-Cons.

## Compilar e testar a biblioteca

Para desenvolver Borealis de modo independente, consulte os guias upstream:

- [Nintendo Switch](https://github.com/xfangfang/borealis/wiki/NintendoSwitch)
- [Windows, macOS e Linux](https://github.com/xfangfang/borealis/wiki/Windows-macOS-Linux)
- [Android](https://github.com/xfangfang/borealis/wiki/Android)
- [iOS](https://github.com/xfangfang/borealis/wiki/iOS)
- [PS Vita](https://github.com/xfangfang/borealis/wiki/PS-Vita)
- [PS4](https://github.com/xfangfang/borealis/wiki/PS4)

O consumidor deve usar **C++17 ou superior** e manter os requisitos de compilação do Borealis original. Para os testes de lógica SixAxis, a compilação pode ser feita no PC sem iniciar o Switch:

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Ilibrary/include \
  tests/sixaxis_quality_test.cpp -o /tmp/sixaxis_quality_test
/tmp/sixaxis_quality_test

g++ -std=c++20 -O2 -Wall -Wextra -Werror -Ilibrary/include \
  tests/stable_joycon_slots_test.cpp -o /tmp/stable_joycon_slots_test
/tmp/stable_joycon_slots_test

g++ -std=c++20 -O2 -Wall -Wextra -Werror -Ilibrary/include \
  tests/motion_sample_timing_test.cpp -o /tmp/motion_sample_timing_test
/tmp/motion_sample_timing_test
```

Para a compilação do aplicativo Switch, são necessários o toolchain e as dependências [devkitPro](https://devkitpro.org/wiki/Getting_Started).

## Projetos que usam Borealis

O upstream é utilizado por vários aplicativos, entre eles [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch), [wiliwili](https://github.com/xfangfang/wiliwili), [switchfin](https://github.com/dragonflylee/switchfin), [SimpleModDownloader](https://github.com/PoloNX/SimpleModDownloader) e [UpcomingSwitchGames](https://github.com/PoloNX/UpcomingSwitchGames).

## Créditos e licenças

- Projeto original: [natinusala/borealis](https://github.com/natinusala/borealis).
- Manutenção e evolução upstream: [xfangfang/borealis](https://github.com/xfangfang/borealis) e demais colaboradores.
- Tecnologias fundamentais: [NanoVG](https://github.com/memononen/nanovg), [Yoga Layout](https://github.com/facebook/yoga) e dependências do repositório.
- Integração deste fork: [GutoMitzy/Moonlight-Switch](https://github.com/GutoMitzy/Moonlight-Switch).

Os direitos autorais, licenças e avisos existentes no código e nas dependências permanecem válidos. Este fork não é afiliado aos mantenedores originais.
