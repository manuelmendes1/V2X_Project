import os
import subprocess
import tempfile
import unittest
from pathlib import Path


def repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / ".git").exists()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 nao encontrada.")


def read_cmake_cache_value(cache_path, key):
    prefix = key + ":"

    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1].strip()

    raise RuntimeError(
        f"Variavel {key} nao encontrada em {cache_path}"
    )


class TestItsStartTimeParser(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))

        cache_path = cls.root / "cmake-cache" / "CMakeCache.txt"
        if not cache_path.is_file():
            raise RuntimeError(
                f"CMakeCache nao encontrado: {cache_path}"
            )

        compiler = read_cmake_cache_value(
            cache_path, "CMAKE_CXX_COMPILER"
        )
        vanetza_root = read_cmake_cache_value(
            cache_path, "V2X_VANETZA_ROOT"
        )

        cls.compiler = Path(compiler)
        cls.vanetza_root = Path(vanetza_root)
        cls.include_dir = cls.vanetza_root / "include"
        cls.library_dir = cls.vanetza_root / "lib"
        cls.archive = cls.library_dir / "libvanetza_common.a"

        for required in (
            cls.compiler,
            cls.include_dir,
            cls.archive,
        ):
            if not required.exists():
                raise RuntimeError(
                    f"Dependencia do toolchain nao encontrada: {required}"
                )

        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="its-start-time-parser-"
        )
        cls.binary = (
            Path(cls.tempdir.name) / "test_its_start_time_parser"
        )
        cls.link_map = Path(cls.tempdir.name) / "its-link.map"

        test_source = (
            cls.root
            / "scratch/test/Red/Context/test_its_start_time_parser.cc"
        )
        implementation = (
            cls.root
            / "scratch/vanetza_integration/context/its_time/"
              "its_start_time_parser.cc"
        )

        command = [
            str(cls.compiler),
            "-std=c++23",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            f"-I{cls.root / 'scratch'}",
            f"-I{cls.include_dir}",
            str(test_source),
            str(implementation),
            f"-Wl,-Map={cls.link_map}",
            "-Wl,--start-group",
            str(cls.archive),
            "-Wl,--end-group",
            "-o",
            str(cls.binary),
        ]

        result = subprocess.run(
            command,
            cwd=cls.root,
            capture_output=True,
            text=True,
            check=False,
            timeout=60,
        )

        if result.returncode != 0:
            raise RuntimeError(
                "ITS_START_TIME_PARSER_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{result.stdout}\n"
                f"STDERR:\n{result.stderr}"
            )

        if not cls.link_map.is_file():
            raise RuntimeError("Mapa de ligacao nao foi gerado.")

        map_content = cls.link_map.read_text(
            encoding="utf-8", errors="replace"
        )

        if str(cls.archive) not in map_content:
            raise RuntimeError(
                "A biblioteca Vanetza configurada nao aparece "
                "no mapa de ligacao."
            )

        print(f"ITS_START_TIME_PARSER_COMPILER={cls.compiler}")
        print(f"ITS_START_TIME_PARSER_VANETZA={cls.vanetza_root}")
        print("ITS_START_TIME_PARSER_LINK_MAP=PASS")
        print("ITS_START_TIME_PARSER_BUILD=PASS")

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "tempdir"):
            cls.tempdir.cleanup()

    def test_parser_is_independent_of_process_timezone(self):
        for timezone in (
            "UTC",
            "Europe/Lisbon",
            "America/New_York",
        ):
            with self.subTest(timezone=timezone):
                env = os.environ.copy()
                env["TZ"] = timezone
                env["LD_LIBRARY_PATH"] = (
                    str(self.library_dir)
                    + ":"
                    + env.get("LD_LIBRARY_PATH", "")
                )

                result = subprocess.run(
                    [str(self.binary)],
                    capture_output=True,
                    text=True,
                    env=env,
                    check=False,
                    timeout=20,
                )

                output = result.stdout + result.stderr
                print(f"TIMEZONE={timezone}")
                print(output, end="")

                self.assertEqual(result.returncode, 0, msg=output)
                self.assertIn(
                    "ITS_START_TIME_PARSER_CONTRACT=PASS",
                    output,
                )


if __name__ == "__main__":
    unittest.main(verbosity=2)
