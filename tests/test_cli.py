import math
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class CompilerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.directory.name) / "expr.exe"
        subprocess.run(["g++","-std=c++17","-O2","-Wall","-Wextra","-pedantic",str(ROOT/"main.cpp"),"-o",str(cls.binary)],check=True,capture_output=True,text=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_expr(self, value, *vars):
        return subprocess.run([str(self.binary),value,*vars],capture_output=True,text=True)

    def test_precedence_folding_and_functions(self):
        output=self.run_expr("2*3+x^2", "x=4")
        self.assertEqual(output.returncode,0)
        self.assertIn("PUSH 6.000000",output.stdout)
        self.assertTrue(output.stdout.endswith("RESULT 22\n"))
        self.assertAlmostEqual(float(self.run_expr("max(2,sin(0)+3)").stdout.split("RESULT ")[1]),3)
        self.assertEqual(self.run_expr("2^3^2").stdout.split("RESULT ")[1].strip(),"512")

    def test_invalid_inputs(self):
        for expression in ("2+", "1/0", "x+1", "unknown(2)", "sqrt(-1)"):
            self.assertNotEqual(self.run_expr(expression).returncode,0,expression)


if __name__ == "__main__": unittest.main()

