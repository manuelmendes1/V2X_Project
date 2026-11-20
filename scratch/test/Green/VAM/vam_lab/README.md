# VAM lab — ASN.1 BIT STRING DEFAULT / OER

**Estado:** experiência isolada validada; não integrada em produção.

## Objetivo

Documentar uma alteração experimental ao compilador `asn1c` para tratar
valores `BIT STRING DEFAULT` com named bits e constraint fixa `SIZE(8)`,
incluindo a omissão do valor default em OER e a sua restauração no decoder.

O caso de referência é compatível com o padrão `EndEntityType DEFAULT {app}`
usado no contexto de permissões ITS.

## Limites

Este laboratório:

- não implementa o codec ASN.1 VAM nem altera o encoder UPER VAM;
- não faz parte do runtime ns-3, Vanetza, BTP ou GeoNetworking;
- não demonstra ITS-AID 638, SSP, assinatura ou autorização;
- não deve ser incluído em `scratch/CMakeLists.txt` nem em targets de produção;
- valida apenas o âmbito de constraints fixas, exatas e não extensíveis
  implementado no patch.

A validação VAM UPER de Profile 1 é feita separadamente pelo teste
`scratch/test/Red/VAM/test_vam_uper_encoder.py`.

## Origem do patch

- Projeto: fork `mouse07410/asn1c`
- Baseline Git: `eefc4cfca40e5ee6e0b6506d6d8c93c24d423cec`
- Estado da árvore de origem no empacotamento: clean, detached HEAD
- Ficheiros modificados: cinco, listados no patch em `patches/`

O patch é gerado relativamente ao commit de baseline e pode ser aplicado
a uma árvore limpa desse mesmo commit. Não altera o clone original.

## Conteúdo

- `patches/bit-string-default-oer.patch` — alteração do compilador;
- `fixtures/*.asn` — casos named default, hexadecimal válido e inválido;
- `test_fork_default_oer.c` — harness de teste;
- `RESULTS.md` — resultados registados;
- `SHA256SUMS` — integridade dos ficheiros do laboratório.

Não são incluídos executáveis, bibliotecas compiladas, diretórios de código
gerado ou credenciais.

## Reexecução

Usar uma árvore limpa do fork no commit indicado acima. Criar uma cópia
temporária dessa árvore, aplicar o patch com `patch -p1` e construir/testar
o compilador nessa cópia. Os comandos de build dependem das ferramentas
Autotools disponíveis no ambiente. Nunca aplicar o patch ao checkout
de produção ou à árvore original de origem.
