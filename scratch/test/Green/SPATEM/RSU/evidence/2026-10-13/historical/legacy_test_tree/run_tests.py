#!/usr/bin/env python3
"""Runner local para os testes da integração Vanetza."""

import argparse
import importlib.util
import sys
import unittest
from pathlib import Path

sys.dont_write_bytecode = True

TEST_ROOT = Path(__file__).resolve().parent


def discover_tests(suite_name):
    """Descobre ficheiros test_*.py numa suite."""
    suite_path = TEST_ROOT / suite_name

    if not suite_path.is_dir():
        raise FileNotFoundError(f"Suite inexistente: {suite_path}")

    return sorted(
        path
        for path in suite_path.rglob("test_*.py")
        if path.is_file()
    )


def load_test_file(path, index):
    """Carrega um ficheiro de teste sem exigir que seja um pacote Python."""
    module_name = f"vanetza_test_{index}_{path.stem}"
    spec = importlib.util.spec_from_file_location(module_name, path)

    if spec is None or spec.loader is None:
        raise RuntimeError(f"Não foi possível carregar {path}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)

    return unittest.defaultTestLoader.loadTestsFromModule(module)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--suite",
        choices=("green", "red", "all"),
        default="red",
        help="Suite a executar (predefinição: red)",
    )
    parser.add_argument(
        "--test",
        help=(
            "Executar um único ficheiro, relativamente a test/. "
            "Ex.: red/spatem/rsu/test_rsu_ipv4_sender_resolution.py"
        ),
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="Listar os testes descobertos sem os executar",
    )
    args = parser.parse_args()

    try:
        if args.test:
            path = (TEST_ROOT / args.test).resolve()

            if TEST_ROOT.resolve() not in path.parents:
                raise ValueError("O teste tem de estar dentro da diretoria test/")

            if not path.is_file() or path.suffix != ".py":
                raise FileNotFoundError(f"Ficheiro de teste inválido: {path}")

            paths = [path]
        else:
            suites = (
                ("green", "red") if args.suite == "all"
                else (args.suite,)
            )
            paths = []
            for suite_name in suites:
                paths.extend(discover_tests(suite_name))
            paths = sorted(set(paths))

        print(f"Diretoria de testes: {TEST_ROOT}")
        print(f"Ficheiros encontrados: {len(paths)}")

        for path in paths:
            print(f"  {path.relative_to(TEST_ROOT)}")

        if args.list:
            return 0

        if not paths:
            print("ERRO: nenhum teste encontrado.", file=sys.stderr)
            return 2

        combined_suite = unittest.TestSuite()

        for index, path in enumerate(paths):
            combined_suite.addTests(load_test_file(path, index))

        result = unittest.TextTestRunner(verbosity=2).run(combined_suite)

        print(
            f"\nResumo: {result.testsRun} executado(s), "
            f"{len(result.failures)} falha(s), "
            f"{len(result.errors)} erro(s)."
        )

        return 0 if result.wasSuccessful() else 1

    except Exception as exc:
        print(f"ERRO NO RUNNER: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
