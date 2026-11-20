"""Run clang-format and clang-tidy over the project's own C++ sources.

Only files under projects/ are checked; third-party code (lib/, packages/) is skipped.

Usage:
	python tools/code_checks.py [options] [FILES...]

With no FILES, every C++ file under projects/ is checked.
Exit code is 0 when everything is clean, 1 otherwise.

clang-tidy notes: the project uses MSVC-built C++20 modules, which clang can't
import, so compiler errors are ignored and only check findings are reported.
Code that depends on types from unresolved imports may go unchecked.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKED_DIR = "projects"
EXTENSIONS = {".cpp", ".h", ".hpp", ".ixx", ".inl"}
LANGUAGE_BY_EXTENSION = {".ixx": "c++-module", ".h": "c++-header", ".hpp": "c++-header", ".inl": "c++-header"}
TIDY_ARGS_FILE = ROOT / "tools" / "clang_tidy_args.txt"
TIDY_TIMEOUT_SECONDS = 110

DIAGNOSTIC = re.compile(r"^(?P<location>.+?:\d+:\d+): (?:warning|error): (?P<message>.*) \[(?P<checks>[^\]]+)\]$")


@dataclass
class TidyResult:
	path: Path
	findings: list[str] = field(default_factory=list)
	error: str | None = None


def is_project_source(path: Path) -> bool:
	"""True for C++ files under projects/ (path must be absolute)."""
	if path.suffix.lower() not in EXTENSIONS:
		return False
	try:
		relative = path.relative_to(ROOT)
	except ValueError:
		return False
	return bool(relative.parts) and relative.parts[0] == CHECKED_DIR


def project_sources() -> list[Path]:
	return sorted(p for p in (ROOT / CHECKED_DIR).rglob("*") if p.is_file() and is_project_source(p))


def find_tool(name: str) -> str:
	found = shutil.which(name)
	if not found:
		raise FileNotFoundError(f"'{name}' not found (pass a full path or add it to PATH)")
	return found


def read_tidy_args(args_file: Path = TIDY_ARGS_FILE) -> list[str]:
	if not args_file.is_file():
		return []
	args = []
	for line in args_file.read_text(encoding="utf-8").splitlines():
		line = line.strip()
		if line and not line.startswith("#"):
			args.append(os.path.expandvars(line))
	return args


# --- clang-format -------------------------------------------------------------


def format_file(clang_format: str, path: Path) -> int:
	return subprocess.run([clang_format, "-i", "--style=file", str(path)], cwd=ROOT).returncode


def is_formatted(clang_format: str, path: Path) -> bool:
	result = subprocess.run([clang_format, "--style=file", str(path)], cwd=ROOT, capture_output=True, check=True)
	return result.stdout == path.read_bytes()


# --- clang-tidy ---------------------------------------------------------------


def is_check_finding(checks: str) -> bool:
	"""True if the diagnostic comes from a clang-tidy check, not the compiler."""
	names = [name for name in checks.split(",") if not name.startswith("-")]
	return any(not name.startswith("clang-diagnostic-") for name in names)


def tidy_command(clang_tidy: str, path: Path, build_path: str | None, compiler_args: list[str]) -> list[str]:
	command = [clang_tidy, "--quiet", f"--config-file={ROOT / '.clang-tidy'}"]
	if build_path:
		return command + ["-p", build_path, str(path)]

	language = LANGUAGE_BY_EXTENSION.get(path.suffix.lower())
	if language:
		command.append(f"--extra-arg-before=-x{language}")
	# -ferror-limit=0 keeps clang parsing past unresolved imports so checks still see the whole file.
	return command + [str(path), "--", "-ferror-limit=0"] + compiler_args


def tidy_file(clang_tidy: str, path: Path, build_path: str | None = None, compiler_args: list[str] | None = None) -> TidyResult:
	command = tidy_command(clang_tidy, path, build_path, compiler_args or [])
	try:
		result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, errors="replace", timeout=TIDY_TIMEOUT_SECONDS)
	except subprocess.TimeoutExpired:
		return TidyResult(path, error=f"timed out after {TIDY_TIMEOUT_SECONDS}s")

	findings = []
	for line in (result.stdout + result.stderr).splitlines():
		line = line.strip()
		match = DIAGNOSTIC.match(line)
		if match and is_check_finding(match["checks"]):
			findings.append(line)
	return TidyResult(path, findings=list(dict.fromkeys(findings)))


# --- command line -------------------------------------------------------------


def relative(path: Path) -> str:
	return path.relative_to(ROOT).as_posix()


def run_format(clang_format: str, files: list[Path], fix: bool, jobs: int) -> bool:
	print(f"clang-format: checking {len(files)} file(s)")
	with ThreadPoolExecutor(jobs) as pool:
		formatted = list(pool.map(lambda p: is_formatted(clang_format, p), files))
	bad = [path for path, ok in zip(files, formatted) if not ok]

	for path in bad:
		if fix:
			format_file(clang_format, path)
			print(f"  formatted {relative(path)}")
		else:
			print(f"  needs formatting: {relative(path)}")

	if fix or not bad:
		print("clang-format: OK" if not bad else f"clang-format: fixed {len(bad)} file(s)")
		return True
	print(f"clang-format: {len(bad)} file(s) need formatting (run with --fix)")
	return False


def run_tidy(clang_tidy: str, files: list[Path], build_path: str | None, jobs: int) -> bool:
	import naming_abbreviations

	if not naming_abbreviations.is_up_to_date():
		print("clang-tidy: .clang-tidy is out of date with tools/naming_abbreviations.txt; run python tools/naming_abbreviations.py")
		return False

	compiler_args = read_tidy_args()
	print(f"clang-tidy: checking {len(files)} file(s) with {jobs} job(s)")
	with ThreadPoolExecutor(jobs) as pool:
		results = list(pool.map(lambda p: tidy_file(clang_tidy, p, build_path, compiler_args), files))

	findings = 0
	for result in results:
		if result.error:
			print(f"  {relative(result.path)}: {result.error}")
		for finding in result.findings:
			print(f"  {finding}")
		findings += len(result.findings)

	errors = sum(1 for r in results if r.error)
	if findings == 0 and errors == 0:
		print("clang-tidy: OK")
		return True
	print(f"clang-tidy: {findings} finding(s), {errors} file(s) failed")
	return False


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("files", nargs="*", type=Path, help="files to check (default: all C++ files under projects/)")
	parser.add_argument("--format", action="store_true", help="run only clang-format")
	parser.add_argument("--tidy", action="store_true", help="run only clang-tidy")
	parser.add_argument("--fix", action="store_true", help="apply clang-format changes instead of reporting them")
	parser.add_argument("--clang-format", default="clang-format", help="clang-format executable (name on PATH or full path)")
	parser.add_argument("--clang-tidy", default="clang-tidy", help="clang-tidy executable (name on PATH or full path)")
	parser.add_argument("-p", dest="build_path", help="directory containing compile_commands.json (overrides tools/clang_tidy_args.txt)")
	parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 1, help="parallel jobs (default: CPU count)")
	args = parser.parse_args()

	run_both = not args.format and not args.tidy
	if args.files:
		files = [p.resolve() for p in args.files]
		skipped = [p for p in files if not is_project_source(p)]
		for path in skipped:
			print(f"skipping {path} (not a C++ file under {CHECKED_DIR}/)")
		files = [p for p in files if p not in skipped and p.is_file()]
	else:
		files = project_sources()

	if not files:
		print("nothing to check")
		return 0

	try:
		ok = True
		if args.format or run_both:
			ok &= run_format(find_tool(args.clang_format), files, args.fix, args.jobs)
		if args.tidy or run_both:
			ok &= run_tidy(find_tool(args.clang_tidy), files, args.build_path, args.jobs)
	except FileNotFoundError as error:
		print(f"error: {error}", file=sys.stderr)
		return 1
	return 0 if ok else 1


if __name__ == "__main__":
	sys.exit(main())
