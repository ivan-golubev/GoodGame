"""PostToolUse hook: run clang-tidy on C++ files Claude edits under projects/.

Findings are written to stderr with exit code 2, which Claude Code feeds back
to Claude so it can fix them. Compiler flags come from tools/clang_tidy_args.txt
unless -p points at a compile_commands.json.

Usage:
	python clang_tidy.py [--clang-tidy PATH] [-p BUILD_DIR]
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import code_checks  # noqa: E402


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--clang-tidy", default="clang-tidy", help="clang-tidy executable (name on PATH or full path)")
	parser.add_argument("-p", dest="build_path", help="directory containing compile_commands.json")
	args = parser.parse_args()

	file_path = json.load(sys.stdin).get("tool_input", {}).get("file_path")
	if not file_path:
		return 0
	path = Path(file_path).resolve()
	if not path.is_file() or not code_checks.is_project_source(path):
		return 0

	try:
		clang_tidy = code_checks.find_tool(args.clang_tidy)
	except FileNotFoundError as error:
		print(f"clang-tidy hook: {error}", file=sys.stderr)
		return 1

	result = code_checks.tidy_file(clang_tidy, path, args.build_path, code_checks.read_tidy_args())
	if result.error:
		print(f"clang-tidy hook: {code_checks.relative(path)}: {result.error}", file=sys.stderr)
		return 1
	if not result.findings:
		return 0

	print(f"clang-tidy found {len(result.findings)} issue(s) in {code_checks.relative(path)}; please fix them:", file=sys.stderr)
	for finding in result.findings:
		print(finding, file=sys.stderr)
	return 2


if __name__ == "__main__":
	sys.exit(main())
