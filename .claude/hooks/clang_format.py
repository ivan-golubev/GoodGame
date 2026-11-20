"""PostToolUse hook: run clang-format on C++ files Claude edits under projects/.

With --check-conventions, the formatted file is then checked with
tools/check_conventions.py. Findings are written to stderr with exit code 2,
which Claude Code feeds back to Claude so it can fix them. Both steps run in
this one hook so the check always sees the formatted file (separate hooks run
in parallel).

Usage:
	python clang_format.py [--clang-format PATH] [--check-conventions]

PATH defaults to "clang-format" (looked up on PATH).
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import check_conventions  # noqa: E402
import code_checks  # noqa: E402


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--clang-format", default="clang-format", help="clang-format executable (name on PATH or full path)")
	parser.add_argument("--check-conventions", action="store_true", help="check the formatted file with tools/check_conventions.py")
	args = parser.parse_args()

	file_path = json.load(sys.stdin).get("tool_input", {}).get("file_path")
	if not file_path:
		return 0
	path = Path(file_path).resolve()
	if not path.is_file() or not code_checks.is_project_source(path):
		return 0

	try:
		clang_format = code_checks.find_tool(args.clang_format)
	except FileNotFoundError as error:
		print(f"clang-format hook: {error}", file=sys.stderr)
		return 1
	result = code_checks.format_file(clang_format, path)
	if result != 0 or not args.check_conventions:
		return result

	findings = check_conventions.check_files([path])
	if not findings:
		return 0
	print(f"Convention check found {len(findings)} issue(s) in {code_checks.relative(path)}; please fix them:", file=sys.stderr)
	for finding in findings:
		print(finding, file=sys.stderr)
	print("Rules: python tools/check_conventions.py --help", file=sys.stderr)
	return 2


if __name__ == "__main__":
	sys.exit(main())
