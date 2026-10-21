"""Check declaration reach through local fragments without crossing source units."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import public_prose_gate as gate


class IncludeReachTests(unittest.TestCase):
    def measure(self, files):
        with tempfile.TemporaryDirectory(prefix="prose-includes-") as directory:
            root = Path(directory)
            for path, text in files.items():
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(text, encoding="utf-8")
            with patch.object(gate, "tracked", return_value=(sorted(files), [])):
                return gate.measure(directory)

    def test_nested_local_fragment_sees_including_units_header(self):
        counters, _, detail = self.measure({
            "src/votv-coop/include/api/live.h": "void LiveAction();\n",
            "src/votv-coop/src/unit.cpp": '#include "api/live.h"\n#include "parts/outer.inc"\nvoid LiveAction(){}\n',
            "src/votv-coop/src/parts/outer.inc": '#include "./inner.inc"\n',
            "src/votv-coop/src/parts/inner.inc": "void Caller(){ LiveAction(); }\n",
        })
        self.assertEqual(counters["src.dead_declarations"], 0, detail)

    def test_relative_header_resolution(self):
        counters, _, detail = self.measure({
            "src/votv-coop/include/api/live.h": "void LiveAction();\n",
            "src/votv-coop/src/nested/unit.cpp": '#include "../../include/api/live.h"\nvoid LiveAction(){}\nvoid Caller(){ LiveAction(); }\n',
        })
        self.assertEqual(counters["src.dead_declarations"], 0, detail)

    def test_shared_fragment_does_not_export_context_to_other_unit(self):
        first = "src/votv-coop/include/api/first.h"
        second = "src/votv-coop/include/api/second.h"
        counters, _, detail = self.measure({
            first: "namespace First {\nvoid SameAction();\n}\n",
            second: "namespace Second {\nvoid SameAction();\n}\n",
            "src/votv-coop/src/first.cpp": '#include "api/first.h"\nnamespace First {\n#include "shared.inc"\nvoid SameAction(){}\n}\n',
            "src/votv-coop/src/second.cpp": '#include "api/second.h"\nnamespace Second {\n#include "shared.inc"\nvoid SameAction(){}\nvoid Caller(){ SameAction(); }\n}\n',
            "src/votv-coop/src/shared.inc": "void EmptyHelper(){}\n",
        })
        self.assertEqual(counters["src.dead_declarations"], 1, detail)
        self.assertEqual(detail["src.dead_declarations"][first], ["SameAction"])
        self.assertFalse(detail["src.dead_declarations"].get(second))

    def test_unreferenced_fragment_cannot_hide_unused_declaration(self):
        header = "src/votv-coop/include/api/unused.h"
        counters, _, detail = self.measure({
            header: "void UnusedAction();\n",
            "src/votv-coop/src/unit.cpp": '#include "api/unused.h"\nvoid UnusedAction(){}\n',
            "src/votv-coop/src/unreferenced.inc": "void Caller(){ UnusedAction(); }\n",
        })
        self.assertEqual(counters["src.dead_declarations"], 1, detail)
        self.assertEqual(detail["src.dead_declarations"][header], ["UnusedAction"])


if __name__ == "__main__":
    unittest.main()
