#!/usr/bin/env python3
"""Executor unittest para as suites oficiais em scratch/test/."""

import argparse
import importlib.util
import sys
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
TEST_ROOT = Path(__file__).resolve().parent


def discover_tests(suite_name):
    suite_path = TEST_ROOT / suite_name
    if not suite_path.is_dir():
        raise FileNotFoundError(f"Suite inexistente: {suite_path}")
    excluded_dirs = {"evidence", "historical", "__pycache__"}
    return sorted(
        path
        for path in suite_path.rglob("test_*.py")
        if path.is_file()
        and not any(
            part.casefold() in excluded_dirs
            for part in path.relative_to(suite_path).parts
        )
    )


def load_test_file(path, index):
    module_name = f"ns3_test_{index}_{path.stem}"
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
        default="green",
        help="Suite a executar (predefinição: green)",
    )
    parser.add_argument(
        "--test",
        help="Ficheiro .py relativo a scratch/test/",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="Listar os ficheiros encontrados sem os executar",
    )
    args = parser.parse_args()

    try:
        if args.test:
            path = (TEST_ROOT / args.test).resolve()
            if TEST_ROOT.resolve() not in path.parents:
                raise ValueError("O teste tem de estar dentro de scratch/test/")
            if not path.is_file() or path.suffix != ".py":
                raise FileNotFoundError(f"Ficheiro de teste inválido: {path}")
            paths = [path]
        else:
            suites = (
                ("Green", "Red")
                if args.suite == "all"
                else ({"green": "Green", "red": "Red"}[args.suite],)
            )
            paths = sorted({
                path
                for suite_name in suites
                for path in discover_tests(suite_name)
            })

        print(f"Diretoria de testes: {TEST_ROOT}")
        print(f"Ficheiros encontrados: {len(paths)}")
        for path in paths:
            print(f"  {path.relative_to(TEST_ROOT)}")

        if args.list:
            return 0
        if not paths:
            print("ERRO: nenhum teste encontrado.", file=sys.stderr)
            return 2

        combined = unittest.TestSuite()
        for index, path in enumerate(paths):
            loaded_suite = load_test_file(path, index)
            test_count = loaded_suite.countTestCases()

            if test_count == 0:
                raise RuntimeError(
                    "Ficheiro descoberto sem testes unittest: "
                    f"{path.relative_to(TEST_ROOT)}"
                )

            print(
                f"TEST_FILE_CASES="
                f"{path.relative_to(TEST_ROOT)}:{test_count}"
            )
            combined.addTests(loaded_suite)

        result = unittest.TextTestRunner(verbosity=2).run(combined)
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
