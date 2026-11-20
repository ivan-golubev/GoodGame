"""PostToolUse hook: run clang-format on C++ files Claude edits under projects/.

Usage:
	python clang_format.py [--clang-format PATH]

PATH defaults to "clang-format" (looked up on PATH).
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import code_checks  # noqa: E402


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--clang-format", default="clang-format", help="clang-format executable (name on PATH or full path)")
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
	return code_checks.format_file(clang_format, path)


if __name__ == "__main__":
	sys.exit(main())
