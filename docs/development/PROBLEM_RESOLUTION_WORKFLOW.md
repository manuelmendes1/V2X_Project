# Workflow de resolução de problemas V2X

Este workflow aplica a sequência controlada:

\`branch → RED → implementação → GREEN/regressão → stage seletivo → commit → push → PR → checks → squash merge\`.

## 1. Abrir uma branch isolada

A partir de um checkout atualizado e limpo:

\`\`\`bash
scripts/problem-resolution.sh start C-02 cam-v3-security
\`\`\`

O script atualiza \`main\` (ou a base indicada), exige que o worktree esteja limpo e cria:

\`\`\`text
fix/c-02-cam-v3-security
\`\`\`

Não reutilizar uma branch de outro problema.

## 2. Definir os gates antes da implementação

Cada problema tem de possuir um perfil executável:

\`\`\`text
scripts/problem-tests/C-02.sh
\`\`\`

O perfil deve primeiro reproduzir a falha (RED), executar os testes de contrato/regressão relevantes e demonstrar a correção (GREEN). Um problema sem perfil executável não pode avançar para PR.

O workflow GitHub Actions procura automaticamente o identificador no nome da branch e executa o perfil correspondente. Um perfil inexistente causa falha fechada.

Para adicionar um novo problema:
1. criar os testes RED;
2. criar \`scripts/problem-tests/C-NN.sh\`;
3. implementar a correção;
4. acrescentar os testes GREEN e regressões ao perfil;
5. só depois executar \`finish\`.

## 3. Stage, commit, push, PR e merge

Depois de os testes locais passarem, indicar explicitamente os caminhos intencionais:

\`\`\`bash
scripts/problem-resolution.sh finish C-02 \
  "fix(security): bind CAM verification to V3 identity" \
  --paths \
  scratch/vanetza_integration/security/cam_security.cc \
  scratch/test/Red/CAM/test_cam_v3_binding.py \
  scripts/problem-tests/C-02.sh
\`\`\`

O script:
- executa novamente o perfil local;
- executa \`git diff --check\`;
- faz stage apenas dos caminhos listados;
- verifica que o index contém exatamente essa allowlist;
- cria o commit e faz push;
- abre (ou reutiliza) o PR para \`main\`;
- espera pelos checks do GitHub Actions;
- faz squash merge e apaga a branch remota apenas se os checks passarem.

Não usar \`git add .\` nem \`git add -A\`.

## 4. GitHub Actions

\`.github/workflows/problem-resolution.yml\` corre nos PRs para \`main\` e pode ser reexecutado manualmente para uma branch \`fix/c-NN-...\`.

O workflow instala as dependências nativas do perfil e executa a validação específica do problema. Cada perfil controla os seus testes e não deve esconder falhas com \`|| true\`, skips silenciosos ou critérios permissivos.

O script local não faz merge quando \`gh pr checks --watch\` falha. Para obter a mesma proteção em merges manuais pela interface GitHub, configurar também o check \`Problem-specific RED/GREEN and regression gates\` como obrigatório nas regras de proteção de \`main\`.

## 5. Estado dos problemas

Só criar um perfil depois de definir os testes que demonstram o problema. O perfil C-01 é a referência inicial para o contrato geodésico; os seguintes devem ser específicos ao defeito e à sua camada de arquitetura.
