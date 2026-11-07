import sys
from pathlib import Path

BUILD_TOOLS = Path(__file__).resolve().parents[1]

if str(BUILD_TOOLS) not in sys.path:
    sys.path.insert(0, str(BUILD_TOOLS))

from ai_control_mcp.cli import main  # noqa: E402

if __name__ == "__main__":
    raise SystemExit(main())
