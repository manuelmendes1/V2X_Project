#!/usr/bin/env python3

import math
import os
import shutil
import subprocess
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path


def repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestSumoOsmProjectionReference(unittest.TestCase):
    def test_osm_control_node_matches_sumo_utm_projection(self):
        root = repository_root(Path(__file__))
        scenario = (
            root / "sumo/current/scenario/georeferenced/"
            "vila-nova-de-famalicao"
        )
        osm_path = scenario / "source/famalicao-roads.osm"
        net_path = scenario / "famalicao.net.xml"

        self.assertTrue(osm_path.is_file(), f"OSM inexistente: {osm_path}")

        cs2cs = shutil.which("cs2cs")
        self.assertIsNotNone(
            cs2cs,
            "É necessário PROJ cs2cs para a referência externa independente.",
        )

        # Fixture obtida da rede gerada com SUMO/netconvert 1.25.0.
        # A rede final é >100 MB e é intencionalmente excluída do Git.
        # SUMO_PROJECTION_FIXTURE_ONLY=1 testa o cenário de clone limpo.
        fixture_only = os.environ.get("SUMO_PROJECTION_FIXTURE_ONLY") == "1"
        use_fixture = fixture_only or not net_path.is_file()

        node_id = "11645226075"
        if use_fixture:
            projection = (
                "+proj=utm +zone=29 +ellps=WGS84 +datum=WGS84 "
                "+units=m +no_defs"
            )
            offset_x, offset_y = -526913.64, -4576484.19
            actual_x, actual_y = 20374.270, 9640.360
            print("SUMO_NETWORK_SOURCE=RECORDED_NETCONVERT_FIXTURE")
        else:
            net_root = ET.parse(net_path).getroot()
            location = net_root.find("location")
            self.assertIsNotNone(location, "Falta <location> na rede SUMO.")

            projection = location.attrib.get("projParameter", "")
            offset = location.attrib.get("netOffset")
            self.assertIsNotNone(offset, "Falta netOffset na rede SUMO.")
            offset_x, offset_y = map(float, offset.split(","))

            junction = next(
                (
                    item for item in net_root.findall("junction")
                    if item.attrib.get("id") == node_id
                ),
                None,
            )
            self.assertIsNotNone(
                junction, f"Junction SUMO {node_id} não encontrado."
            )
            actual_x = float(junction.attrib["x"])
            actual_y = float(junction.attrib["y"])
            print("SUMO_NETWORK_SOURCE=LOCAL_NET_XML")

        self.assertIn("+proj=utm", projection)
        self.assertIn("+zone=29", projection)
        self.assertIn("+datum=WGS84", projection)

        osm_position = None
        for _, element in ET.iterparse(osm_path, events=("end",)):
            if element.tag == "node" and element.attrib.get("id") == node_id:
                osm_position = (
                    float(element.attrib["lat"]),
                    float(element.attrib["lon"]),
                )
                break
            element.clear()

        self.assertIsNotNone(
            osm_position, f"Nó OSM de controlo {node_id} não encontrado."
        )
        latitude, longitude = osm_position

        self.assertAlmostEqual(latitude, 41.425290000, delta=1e-7)
        self.assertAlmostEqual(longitude, -8.434076200, delta=1e-7)

        result = subprocess.run(
            [
                cs2cs,
                "+proj=longlat", "+datum=WGS84", "+no_defs",
                "+to",
                "+proj=utm", "+zone=29", "+ellps=WGS84",
                "+datum=WGS84", "+units=m", "+no_defs",
                "-f", "%.9f",
            ],
            input=f"{longitude:.12f} {latitude:.12f}\n",
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(
            result.returncode, 0,
            msg=f"Falha no PROJ cs2cs: {result.stderr}",
        )

        fields = result.stdout.split()
        self.assertGreaterEqual(
            len(fields), 2, msg=f"Saída PROJ inválida: {result.stdout!r}"
        )

        easting, northing = float(fields[0]), float(fields[1])
        predicted_x = easting + offset_x
        predicted_y = northing + offset_y
        error_x = predicted_x - actual_x
        error_y = predicted_y - actual_y
        error_norm = math.hypot(error_x, error_y)

        print(f"OSM_CONTROL_NODE={node_id}")
        print(f"OSM_LAT={latitude:.9f}")
        print(f"OSM_LON={longitude:.9f}")
        print(f"SUMO_X_ACTUAL={actual_x:.6f}")
        print(f"SUMO_Y_ACTUAL={actual_y:.6f}")
        print(f"SUMO_X_PREDICTED={predicted_x:.6f}")
        print(f"SUMO_Y_PREDICTED={predicted_y:.6f}")
        print(f"PROJECTION_ERROR_NORM_M={error_norm:.6f}")

        self.assertLessEqual(
            error_norm, 0.05,
            "O nó SUMO diverge da projeção UTM independente em mais de 5 cm.",
        )
        print("SUMO_OSM_PROJECTION_REFERENCE=PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
