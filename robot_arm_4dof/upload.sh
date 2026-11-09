#!/usr/bin/env bash
# Kompiluje i wgrywa szkic na Arduino UNO R3.
# FQBN: arduino:avr:uno
# Port: $PORT, a gdy puste — /dev/ttyACM0, potem /dev/ttyUSB0.
#
# Skrypt nie używa sudo. Przy braku uprawnień do portu tylko wypisuje polecenie.

set -euo pipefail

FQBN="arduino:avr:uno"
SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/robot_arm_4dof-build"

if command -v arduino-cli >/dev/null 2>&1; then
  CLI="$(command -v arduino-cli)"
elif [[ -x "${HOME}/.local/bin/arduino-cli" ]]; then
  CLI="${HOME}/.local/bin/arduino-cli"
else
  echo "Nie znaleziono arduino-cli."
  echo "Najpierw, z tego katalogu: bash install_arduino_cli.sh"
  exit 1
fi

echo "FQBN: ${FQBN}"
echo "Szkic: ${SKETCH_DIR}"
echo "Kompilacja..."
mkdir -p "${BUILD_DIR}"
"${CLI}" compile --fqbn "${FQBN}" --build-path "${BUILD_DIR}" "${SKETCH_DIR}"

PORT_PATH="${PORT:-}"
if [[ -z "${PORT_PATH}" ]]; then
  if [[ -e /dev/ttyACM0 && -e /dev/ttyUSB0 ]]; then
    echo "Wykryto i /dev/ttyACM0, i /dev/ttyUSB0."
    echo "Wybieram /dev/ttyACM0 (oficjalne UNO). Klon z CH340 to zwykle /dev/ttyUSB0."
    echo "Aby wymusić port: PORT=/dev/ttyUSB0 bash upload.sh"
    PORT_PATH="/dev/ttyACM0"
  elif [[ -e /dev/ttyACM0 ]]; then
    PORT_PATH="/dev/ttyACM0"
  elif [[ -e /dev/ttyUSB0 ]]; then
    PORT_PATH="/dev/ttyUSB0"
  else
    echo
    echo "Szkic jest skompilowany. Nie wgrano: brak /dev/ttyACM0 i /dev/ttyUSB0."
    echo "Podłącz UNO kablem USB z liniami danych (nie tylko zasilającym)."
    echo "Lista widziana przez arduino-cli:"
    "${CLI}" board list || true
    exit 1
  fi
fi

if [[ ! -e "${PORT_PATH}" ]]; then
  echo "Szkic jest skompilowany. Port ${PORT_PATH} nie istnieje."
  exit 1
fi

echo
echo "Port: ${PORT_PATH}"
echo "Grupa dialout: zapis do portu szeregowego wymaga członkostwa w tej grupie."
if id -nG | tr ' ' '\n' | grep -qx dialout; then
  echo "Użytkownik ${USER} jest w grupie dialout."
else
  echo "Użytkownik ${USER} nie jest w grupie dialout."
fi
ls -l "${PORT_PATH}" || true

if [[ ! -w "${PORT_PATH}" ]]; then
  echo
  echo "Brak prawa zapisu do ${PORT_PATH}. Wgrywanie wstrzymane."
  echo "Skrypt nie uruchamia sudo. Wykonaj ręcznie, wyloguj się i zaloguj ponownie:"
  echo "  sudo usermod -a -G dialout \"${USER}\""
  echo "Potem sprawdź: id -nG"
  echo "Odłącz i podłącz USB i uruchom ponownie: bash upload.sh"
  exit 1
fi

echo
echo "Wgrywanie na ${PORT_PATH}..."
"${CLI}" upload -p "${PORT_PATH}" --fqbn "${FQBN}" --input-dir "${BUILD_DIR}" "${SKETCH_DIR}"
echo
echo "Wgrano. Port szeregowy: 115200, koniec linii LF."
echo "Otwarcie portu resetuje UNO. Przy ATTACH_ON_BOOT serwa skoczą do pozycji domowej."
