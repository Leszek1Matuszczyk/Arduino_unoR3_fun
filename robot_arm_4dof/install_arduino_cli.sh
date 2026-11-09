#!/usr/bin/env bash
# Instaluje arduino-cli, rdzeń arduino:avr i bibliotekę Servo w katalogu
# użytkownika. Działa na Raspberry Pi OS 64-bit (aarch64) i na innych Linuxach:
# oficjalny instalator sam wybiera architekturę.
#
# Skrypt nie używa sudo. Jeśli czegoś brakuje w systemie, tylko wypisuje
# polecenie, które trzeba uruchomić ręcznie.

set -euo pipefail

if [[ "$(id -u)" -eq 0 ]]; then
  echo "Uruchom ten skrypt jako zwykły użytkownik, nie jako root i nie przez sudo."
  exit 1
fi

BINDIR="${HOME}/.local/bin"
mkdir -p "${BINDIR}"
# Zanim instalator sprawdzi, czy arduino-cli jest widoczne.
export BINDIR
export PATH="${BINDIR}:${PATH}"

if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
  echo "Brak curl i wget. Skrypt nie instaluje pakietów sam."
  echo "Polecenie do ręcznego uruchomienia (wymaga sudo):"
  echo "  sudo apt-get update && sudo apt-get install -y curl ca-certificates"
  exit 1
fi

if command -v arduino-cli >/dev/null 2>&1; then
  CLI="$(command -v arduino-cli)"
  echo "arduino-cli już jest w PATH: ${CLI}"
elif [[ -x "${BINDIR}/arduino-cli" ]]; then
  CLI="${BINDIR}/arduino-cli"
  echo "Używam istniejącego ${CLI}"
else
  echo "Pobieram arduino-cli do ${BINDIR} (bez sudo)..."
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
  CLI="${BINDIR}/arduino-cli"
fi

if [[ ! -x "${CLI}" ]]; then
  echo "Nie udało się uruchomić arduino-cli (${CLI})."
  exit 1
fi

if ! "${CLI}" config dump >/dev/null 2>&1; then
  "${CLI}" config init
fi

echo "Pobieram indeks i rdzeń arduino:avr (bez sudo, do katalogu domowego)..."
"${CLI}" core update-index
"${CLI}" core install arduino:avr

# Rdzeń arduino:avr 1.8.x nie zawiera już Servo.h — bibliotekę trzeba doinstalować.
echo "Instaluję bibliotekę Servo (Library Manager)..."
"${CLI}" lib update-index
"${CLI}" lib install Servo

SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo
echo "Testowa kompilacja szkicu (nic nie jest wgrywane na płytkę)..."
"${CLI}" compile --fqbn arduino:avr:uno "${SKETCH_DIR}"

echo
echo "Gotowe."
echo "Wersja: $("${CLI}" version)"
echo
echo "Jeśli w nowym terminalu nie ma polecenia arduino-cli, dopisz raz do ~/.bashrc:"
echo '  export PATH="$HOME/.local/bin:$PATH"'
echo
echo "Wgrywanie na UNO wymaga grupy dialout. Tego polecenia skrypt NIE uruchamia"
echo "(sudo, potem wylogowanie i ponowne logowanie):"
echo "  sudo usermod -a -G dialout \"${USER}\""
echo "Sprawdzenie: id -nG ; ls -l /dev/ttyACM0 /dev/ttyUSB0"
echo
echo "Następny krok, gdy płytka jest podłączona: bash upload.sh"
