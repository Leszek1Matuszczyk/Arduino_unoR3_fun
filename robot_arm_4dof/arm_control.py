#!/usr/bin/env python3
"""Wysyła komendy tekstowe do ramienia 4DOF na Arduino UNO (Serial 115200).

Bez argumentów uruchamia tryb interaktywny. Z argumentami wysyła jedną komendę
i wypisuje odpowiedź płytki na stdout (linie diagnostyczne idą na stderr).

Otwarcie portu resetuje UNO. Skrypt czeka na linię „OK ready”.
Każda komenda ruchu wraca od razu — ramię jeszcze jedzie. „WAIT” odpowiada
dopiero, gdy ruch i sekwencja PLAY się skończą; wtedy zwiększ --timeout.

Przykłady:
  python3 arm_control.py status
  python3 arm_control.py set base 90
  python3 arm_control.py
"""

from __future__ import annotations

import argparse
import os
import sys
import time


def default_port() -> str | None:
    for candidate in ("/dev/ttyACM0", "/dev/ttyUSB0"):
        if os.path.exists(candidate):
            return candidate
    return None


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Sterowanie ramieniem 4DOF (Arduino UNO) przez port szeregowy."
    )
    parser.add_argument(
        "-p",
        "--port",
        default=os.environ.get("PORT") or default_port(),
        help="Port szeregowy. Domyślnie zmienna PORT, potem /dev/ttyACM0, potem /dev/ttyUSB0.",
    )
    parser.add_argument(
        "-b",
        "--baud",
        type=int,
        default=115200,
        help="Prędkość portu (domyślnie 115200).",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=8.0,
        help="Ile sekund czekać na linię OK/ERR (dla WAIT daj więcej).",
    )
    parser.add_argument(
        "command",
        nargs=argparse.REMAINDER,
        help="Komenda do wysłania. Bez niej startuje tryb interaktywny.",
    )
    return parser


def import_serial():
    try:
        import serial  # type: ignore
    except ImportError:
        print("Brak modułu pyserial. Zainstaluj bez sudo, jednym ze sposobów:", file=sys.stderr)
        print("  python3 -m pip install --user pyserial", file=sys.stderr)
        print("albo, gdy pip zgłosi externally-managed-environment, venv bez sudo:", file=sys.stderr)
        print('  python3 -m venv "$HOME/.venv/robot-arm"', file=sys.stderr)
        print('  "$HOME/.venv/robot-arm/bin/pip" install pyserial', file=sys.stderr)
        print('  "$HOME/.venv/robot-arm/bin/python" arm_control.py', file=sys.stderr)
        print("Pakiet systemowy (tego polecenia skrypt nie uruchamia, wymaga sudo):", file=sys.stderr)
        print("  sudo apt-get install python3-serial", file=sys.stderr)
        return None
    return serial


def dialout_hint() -> None:
    user = os.environ.get("USER", "")
    print("Skrypt nie uruchamia sudo. Jeśli to odmowa dostępu do portu, wykonaj ręcznie", file=sys.stderr)
    print("i zaloguj się ponownie:", file=sys.stderr)
    print(f'  sudo usermod -a -G dialout "{user}"', file=sys.stderr)


def open_port(serial_mod, port: str, baud: int):
    if not port:
        print("Nie podano portu i nie znaleziono /dev/ttyACM0 ani /dev/ttyUSB0.", file=sys.stderr)
        print("Podłącz UNO albo podaj: python3 arm_control.py -p /dev/ttyUSB0", file=sys.stderr)
        return None
    try:
        # Domyślne DTR resetuje UNO przy otwarciu portu. Dzięki temu
        # czekamy na świeży baner „OK ready”, a nie na starą sesję.
        ser = serial_mod.Serial(port=port, baudrate=baud, timeout=0.2)
    except serial_mod.SerialException as exc:
        print(f"Nie można otworzyć {port}: {exc}", file=sys.stderr)
        text = str(exc).lower()
        if "permission" in text or "errno 13" in text or "odmowa" in text:
            dialout_hint()
        return None
    return ser


def read_line(ser, deadline: float) -> str | None:
    while time.monotonic() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        return raw.decode("utf-8", errors="replace").strip()
    return None


def wait_ready(ser, timeout: float, echo) -> bool:
    """Czeka na reset UNO i linię OK ready. Zwraca False, gdy jej nie ma."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = read_line(ser, deadline)
        if line is None:
            break
        if line == "":
            continue
        echo(line)
        if line == "OK ready":
            return True
    return False


def read_response(ser, timeout: float) -> list[str]:
    """Czyta linie aż do pierwszej zaczynającej się od OK lub ERR."""
    deadline = time.monotonic() + timeout
    lines: list[str] = []
    while time.monotonic() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").strip()
        if line == "":
            continue
        lines.append(line)
        if line.startswith("OK") or line.startswith("ERR"):
            return lines
    if not lines:
        raise TimeoutError(
            "Brak odpowiedzi. Sprawdź baud 115200, kabel USB i czy szkic jest wgrany. "
            "Dla komendy WAIT zwiększ --timeout."
        )
    raise TimeoutError(
        "Odpowiedź urwała się przed linią OK/ERR. Dla WAIT zwiększ --timeout."
    )


def send_command(ser, command: str, timeout: float) -> list[str]:
    text = command.strip()
    if text == "":
        return []
    ser.write((text + "\n").encode("utf-8"))
    ser.flush()
    return read_response(ser, timeout)


def response_failed(lines: list[str]) -> bool:
    return bool(lines) and lines[-1].startswith("ERR")


def interactive(ser, timeout: float) -> int:
    print("Tryb interaktywny. Komendy jak do płytki, koniec: quit albo Ctrl-D.", file=sys.stderr)
    print("SET/ALL/HOME wracają od razu — w tym czasie można wysłać STOP.", file=sys.stderr)
    print("WAIT czeka na koniec ruchu. Ctrl-C zamyka port i zwykle resetuje UNO.", file=sys.stderr)
    while True:
        try:
            command = input("arm> ")
        except EOFError:
            print(file=sys.stderr)
            return 0
        except KeyboardInterrupt:
            print(file=sys.stderr)
            return 0
        stripped = command.strip()
        if stripped == "":
            continue
        if stripped.lower() in {"quit", "exit"}:
            return 0
        try:
            lines = send_command(ser, stripped, timeout)
        except TimeoutError as exc:
            print(str(exc), file=sys.stderr)
            continue
        for line in lines:
            print(line)


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    command = " ".join(args.command).strip()
    # argparse.REMAINDER potrafi zostawić wiodący „--”.
    if command.startswith("-- "):
        command = command[3:].strip()

    serial_mod = import_serial()
    if serial_mod is None:
        return 1

    ser = open_port(serial_mod, args.port, args.baud)
    if ser is None:
        return 1

    interactive_mode = command == ""
    try:
        ready = wait_ready(
            ser,
            timeout=max(args.timeout, 8.0),
            echo=print if interactive_mode else (lambda line: print(line, file=sys.stderr)),
        )
        if not ready:
            print(
                "Nie doczekałem się „OK ready”. UNO zwykle resetuje się przy otwarciu portu.",
                file=sys.stderr,
            )
            print("Sprawdź kabel, port (-p) i czy wgrany jest ten szkic.", file=sys.stderr)
            return 1
        ser.reset_input_buffer()

        if interactive_mode:
            return interactive(ser, args.timeout)

        lines = send_command(ser, command, args.timeout)
        for line in lines:
            print(line)
        if response_failed(lines):
            return 1
        return 0
    except TimeoutError as exc:
        print(str(exc), file=sys.stderr)
        return 1
    finally:
        ser.close()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print(file=sys.stderr)
        sys.exit(130)
