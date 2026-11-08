from __future__ import annotations

import sys

from PySide6.QtWidgets import QApplication

from .ui.mainwindow import MainWindow


def main(argv: list[str] | None = None) -> int:
    args = sys.argv if argv is None else argv
    app = QApplication.instance() or QApplication(args)
    window = MainWindow()
    window.show()
    if len(args) > 1:
        window.open_file(args[1])
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
