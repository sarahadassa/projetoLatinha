O projeto AluminiTech consiste no desenvolvimento de uma lixeira inteligente baseada em ESP32, voltada à detecção automática de latas de alumínio e ao registro desses eventos em um sistema de monitoramento embarcado. A solução integra princípios de IoT e Cidades Inteligentes, utilizando um sensor indutivo para identificação do material, LEDs para sinalização do estado da lixeira e um servo motor responsável pela abertura e fechamento da tampa.

O ESP32 atua como unidade central, processando as leituras do sensor, acionando os atuadores e disponibilizando ao usuário um painel web em tempo real, hospedado no próprio microcontrolador. Os registros de coleta são organizados por meio de estruturas de dados, incluindo uma Árvore Binária de Busca para ordenação dos eventos e uma Lista Encadeada para construção do histórico cronológico.

O sistema opera via Wi-Fi, sincroniza data e hora via NTP e mantém todas as informações acessíveis pela interface web, que exibe o estado da lixeira, os depósitos confirmados e o histórico completo. O hardware foi montado utilizando ESP32, sensor indutivo, servo motor e três LEDs responsáveis pela indicação de disponibilidade, detecção e limite de capacidade.

O código-fonte encontra-se organizado em funções que centralizam a inicialização do hardware, a lógica de detecção, o controle do servo motor e o atendimento das requisições do servidor web. O projeto foi testado com sucesso em protótipo físico, validando sua operação e a consistência dos registros.

Este repositório contém o firmware completo e os arquivos de documentação da versão final do sistema.
