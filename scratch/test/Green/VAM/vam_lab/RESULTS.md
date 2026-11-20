# Resultados registados — ASN.1 BIT STRING DEFAULT / OER

## Casos positivos

Para `DEFAULT {app}` e `DEFAULT '80'H`:

- geração de código: PASS;
- compilação do harness: PASS;
- tamanho codificado do default: 1 byte;
- tamanho codificado do valor não-default: 2 bytes;
- default omitido em OER: PASS;
- decoder restaura `eeType=0x80`, `size=1`, `bits_unused=0`: PASS;
- valor não-default preservado como `eeType=0x40`: PASS;
- round-trip OER: PASS.

Bytes observados:

- default: `00`;
- não-default: `8040`.

## Caso negativo

Para `DEFAULT '8'H` com constraint `SIZE(8)`:

- rejeição na geração: PASS;
- diagnóstico identifica o valor de 4 bits contra os 8 exigidos: PASS.

## Pretty-printer

- `DEFAULT {app}` preservado na saída canónica: PASS;
- comparação com `150-with-components-OK.asn1`: sem diferenças.

## Suite do compilador

Resultado reportado para `make check`:

- 107 PASS;
- 1 XFAIL esperado;
- 0 FAIL;
- 0 ERROR.

O XFAIL registado corresponde ao teste `check-158.-fcompound-names.c`.

## Interpretação

Os resultados validam o comportamento isolado do fork no âmbito descrito.
Não validam o ASN.1/UPER VAM nem autorizam a utilização desta alteração no
compilador ou nas bibliotecas de produção.
