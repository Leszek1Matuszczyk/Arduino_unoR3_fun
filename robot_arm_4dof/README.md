# Ramię robota 4DOF z chwytakiem (Arduino UNO R3)

Szkic steruje pięcioma serwami hobbystycznymi: cztery osie ramienia i chwytak.
Ruch jest płynny (kroki liczone z `millis()`, bez `delay()` w pętli głównej).
Raspberry Pi 5 wysyła proste komendy tekstowe po USB, 115200 baud.

**Tego programu nie uruchamiano na płytce ani na ramieniu.** Sprawdzona została
kompilacja `arduino-cli compile --fqbn arduino:avr:uno` oraz składnia skryptów.
Przed pierwszym włączeniem serw ustaw limity i pozycję domową pod swój mechanizm
i trzymaj ramię w wolnej przestrzeni — po resecie serwa skoczą do pozycji domowej.

## Zasilanie

Serw **nie zasilaj z pinu 5V Arduino UNO**. Prąd kilku serw, szczególnie gdy któreś
stanie w miejscu i będzie pchać w ogranicznik, przekracza możliwości stabilizatora
na płytce. Skutek to reset UNO albo uszkodzenie regulatora.

- Osobny zasilacz **5–6 V** (zgodnie z kartą konkretnych serw), o prądzie z zapasem.
  Pięć małych serw SG90/MG90S potrafi w szczycie wziąć kilka amperów; większe
  (np. MG996R) wyraźnie więcej. Dobierz zasilacz do modelu, którego używasz.
- **Wspólna masa:** GND zasilacza serw połączony z GND Arduino.
- Przewód sygnałowy (zwykle żółty lub pomarańczowy) idzie na piny 3, 4, 5, 6 i 7.
- Plus zasilacza serw idzie tylko na czerwone przewody serw. Nie łącz go z pinem 5V UNO.
- Przy pierwszym uruchomieniu odłącz mechanikę albo ustaw ramię tak, żeby skok do
  pozycji domowej niczego nie uderzył.

## Założenie mapy pinów

Leszek podał, że serwa są na pinach **3, 4, 5, 6, 7** i że są to **kolejne osie od dołu**.
Nazwy ról w tabeli są **założeniem tego programu**. Da się je zmienić w jednym miejscu:
sekcja `KONFIGURACJA` na górze pliku `robot_arm_4dof.ino` (tablica `JOINT_CFG`).

| Oś od dołu | Rola (założenie) | Pin | Nazwy w komendach |
| --- | --- | --- | --- |
| 1 | podstawa (obrót) | 3 | `1`, `base`, `b`, `podstawa` |
| 2 | bark | 4 | `2`, `shoulder`, `s`, `bark` |
| 3 | łokieć | 5 | `3`, `elbow`, `e`, `lokiec` |
| 4 | nadgarstek | 6 | `4`, `wrist`, `w`, `nadgarstek` |
| 5 | chwytak | 7 | `5`, `gripper`, `g`, `chwytak` |

**Leszek nie potwierdził roli pinu 7.** Program przyjmuje, że pin 7 to chwytak.
Jeśli to inna oś, popraw pin w `JOINT_CFG` albo przepnij przewód. Komenda `SET 3 90`
oznacza trzecią oś od dołu (w tym założeniu: łokieć, pin 5), a nie pin cyfrowy 3.

Biblioteka Servo na UNO używa Timer1. `analogWrite` na pinach 9 i 10 przestaje działać.
Piny 0 i 1 zostaw wolne — to UART używany przez USB.

## Szybki start na Raspberry Pi 5

Raspberry Pi OS 64-bit (aarch64), UNO podłączone kablem USB z liniami danych.
Wszystko poniżej instaluje się w katalogu domowym. **Żaden skrypt nie wywołuje sudo.**
Jeśli sudo jest potrzebne, skrypt tylko wypisuje gotowe polecenie.

```bash
cd robot_arm_4dof
bash install_arduino_cli.sh
```

Skrypt dokłada `arduino-cli` do `~/.local/bin`, pobiera rdzeń `arduino:avr` i bibliotekę
`Servo`. W rdzeniu 1.8 ta biblioteka nie jest już w środku — bez niej szkic się nie zbuduje.

Gdy w nowym terminalu nie widać `arduino-cli`:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

Grupa `dialout` (skrypty tego **nie** uruchamiają; po nim wyloguj się i zaloguj ponownie):

```bash
sudo usermod -a -G dialout "$USER"
```

Wgranie szkicu. Port jest wybierany tak: zmienna `PORT`, potem `/dev/ttyACM0`
(oficjalne UNO), potem `/dev/ttyUSB0` (częsty klon z CH340).

```bash
bash upload.sh
# klon CH340, gdy oba porty istnieją naraz:
PORT=/dev/ttyUSB0 bash upload.sh
```

Sterowanie z Pythona (pyserial):

```bash
python3 -m pip install --user -r requirements.txt
python3 arm_control.py status
python3 arm_control.py set base 90
python3 arm_control.py
```

Gdy `pip` odpowie `externally-managed-environment`, zrób venv bez sudo:

```bash
python3 -m venv "$HOME/.venv/robot-arm"
"$HOME/.venv/robot-arm/bin/pip" install -r requirements.txt
"$HOME/.venv/robot-arm/bin/python" arm_control.py status
```

Albo pakiet z systemu (ręcznie, skrypt tego nie robi):

```bash
sudo apt-get install python3-serial
```

Samodzielna kompilacja bez wgrywania:

```bash
arduino-cli compile --fqbn arduino:avr:uno .
```

## Jak to działa

- **Bezpieczny start.** Każde serwo ma własny pin, kąt domowy oraz minimum i maksimum
  w `JOINT_CFG`. Po resecie (gdy `ATTACH_ON_BOOT` jest `true`) serwa dostają od razu
  pozycję domową i ją trzymają. Wartość spoza 0–180 albo home poza min/max jest
  obcinana, a na port idzie linia `warn config_clamped ...` jeszcze przed `OK ready`.
- **Każda komenda kąta jest obcinana do limitu tej osi.** Odpowiedź podaje kąt
  faktycznie przyjęty, a przy obcięciu dopisek `clipped_from` albo `clipped`.
- **Płynny ruch.** Cel nie jest wpisywany do serwa od razu. Pętla co kilka milisekund
  dosuwa kąt o kawałek zależny od prędkości (`SPEED`, domyślnie `DEFAULT_SPEED_DPS`).
  W `loop()` nie ma `delay()`.
- **Port 115200, linie zakończone LF** (w monitorze: Newline albo Both NL & CR).
  Wielkość liter nie ma znaczenia. Nazwy są ASCII, bez polskich znaków (`lokiec`,
  nie `łokieć`).
- **Odpowiedź dla człowieka i skryptu.** Linie opisowe nie zaczynają się od `OK` ani
  `ERR`. Ostatnia linia odpowiedzi zawsze zaczyna się od `OK` albo `ERR`.
  `arm_control.py` na tym końcu linii polega. Kod błędu jest drugim wyrazem, np.
  `ERR bad_axis foo`, `ERR detached`, `ERR empty_slot 2`.
- **Sekwencja w RAM.** `SAVE` / `LOAD` / `PLAY` trzymają do 8 pozycji. Reset płytki
  (także otwarcie portu z Pi) kasuje sloty.
- **`STOP` a `DETACH`.** `STOP` zostawia sygnał i moment — serwo trzyma kąt, na którym
  akurat jest. `DETACH` zdejmuje sygnał. Ramię może opaść. `ATTACH` włącza sygnał
  z powrotem na bieżącym kącie logicznym, bez jazdy do home.

Otwarcie portu szeregowego **resetuje UNO**. Każde uruchomienie `arm_control.py`
zaczyna program od nowa: serwa wracają do pozycji domowej, sloty są puste.
Komendy `SET`, `ALL`, `HOME`, `LOAD` i `PLAY` wracają natychmiast, w trakcie ruchu.
`WAIT` odpisuje `OK wait` dopiero, gdy osie dojadą i skończy się `PLAY`. Do długiego
ruchu zwiększ czas: `python3 arm_control.py --timeout 30 wait`.

`SAVE` zapisuje kąt wystawiany na serwo w tej chwili, nie cel, jeśli ramię jeszcze
jedzie. Poczekaj na koniec ruchu albo wyślij `STOP`, a potem `SAVE`.

## Komendy

| Komenda | Znaczenie | Przykład odpowiedzi |
| --- | --- | --- |
| `HELP` lub `?` | lista komend | wiele linii, na końcu `OK help` |
| `PING` | test łącza | `OK pong` |
| `STATUS` | prędkość, osie, limity, sloty | wiele linii, na końcu `OK status` |
| `SET <oś> <kąt>` | jedna oś (alias `MOVE`) | `OK set base 90` |
| `ALL <b> <s> <e> <w> <g>` | wszystkie osie (alias `POSE`) | `OK all 90 80 100 90 40` |
| `HOME` | jazda do pozycji domowej | `OK home 90 90 90 90 90` |
| `STOP` | zatrzymaj, trzymaj kąt | `OK stop 90 80 100 90 40` |
| `DETACH` / `DETACH <oś>` | zdejmij sygnał | `OK detach all` |
| `ATTACH` / `ATTACH <oś>` | włącz sygnał | `OK attach all` |
| `SPEED` / `SPEED <n>` | odczyt albo °/s (1–180) | `OK speed 30` |
| `SAVE <1–8>` | zapisz bieżącą pozycję | `OK save 1 90 90 90 90 90` |
| `LOAD <1–8>` | jedź do slotu | `OK load 1 90 90 90 90 90` |
| `CLEAR <1–8>` / `CLEAR ALL` | skasuj sloty | `OK clear all` |
| `LIST` | wypisz sloty | `slot 1 empty` … `OK list` |
| `PLAY` | odtwórz zapisane sloty po kolei | `OK play 1 2 3` |
| `PLAY <slot> ...` | odtwórz podane sloty | `OK play 2 1` |
| `WAIT` | czekaj na koniec ruchu i PLAY | `OK wait` |

Kąt spoza limitu nie jest błędem. Program go obcina:

```text
OK set base 180 clipped_from 250
OK all 90 170 90 90 90 clipped
OK speed 180 clipped_from 400
```

Ruch przy odłączonym serwie jest błędem (`ERR detached`) — najpierw `ATTACH`.
Pusta sekwencja: `ERR no_slots`. Pusty slot: `ERR empty_slot 3`.

Przykładowa sesja z Pi (po każdym `python3 arm_control.py` płytka się resetuje,
więc slot z poprzedniego wywołania znika; kilka komend z rzędu wysyłaj w trybie
interaktywnym albo jednym skryptem, który trzyma port otwarty):

```text
arm> status
arm> speed 20
arm> set base 120
arm> wait
arm> all 90 70 110 90 40
arm> wait
arm> save 1
arm> home
arm> wait
arm> save 2
arm> play 1 2
arm> wait
arm> stop
arm> detach
```

Nowa komenda ruchu (`SET`, `ALL`, `HOME`, `LOAD`, `STOP`, `DETACH`, `CLEAR`)
przerywa trwające `PLAY`. W trybie interaktywnym `SET` oddaje znak zachęty od razu,
więc `STOP` można wysłać w trakcie jazdy. `WAIT` blokuje terminal do końca ruchu.
Ctrl-C zamyka port i zwykle resetuje UNO, a serwa wracają do pozycji domowej.

## Co zmieniać w szkicu

Wszystko jest na górze `robot_arm_4dof.ino`, sekcja `KONFIGURACJA`.

| Stała | Znaczenie |
| --- | --- |
| `JOINT_CFG` | pin, kąt domowy, minimum, maksimum każdej osi |
| `ATTACH_ON_BOOT` | `false`, jeśli po włączeniu serwa mają zostać bezwładne |
| `DEFAULT_SPEED_DPS` | prędkość po starcie, stopnie na sekundę |
| `MIN_SPEED_DPS`, `MAX_SPEED_DPS` | zakres komendy `SPEED` |
| `POSE_DWELL_MS` | pauza między pozami `PLAY` |
| `SERVO_MIN_US`, `SERVO_MAX_US` | szerokość impulsu, gdy serwo nie dochodzi do skraju |
| `SLOT_COUNT` | ile pozycji trzymać w RAM (komendy używają numerów 1…SLOT_COUNT; domyślnie 8) |

Po zmianie stałych wgraj szkic ponownie (`bash upload.sh`). Limitów nie da się
poszerzyć komendą z portu — to celowe.

Domyślne home `90 90 90 90 90` to elektryczny środek serwa, a nie poza sprawdzona
na tym ramieniu. To samo dotyczy limitów `0..180`: skraj często dobija do
ogranicznika wewnątrz serwa i grzeje silnik. Zawęź `min`/`max`, gdy poznasz mechanikę.

## Gdy coś nie działa

- `arduino-cli: nie znaleziono` — uruchom `install_arduino_cli.sh` i dopisz `~/.local/bin` do `PATH`.
- `Permission denied` na `/dev/ttyACM0` albo `/dev/ttyUSB0` — grupa `dialout` (polecenie wyżej), potem nowe logowanie i ponowne wpięcie USB.
- Brak portu — kabel tylko zasilający nie ma linii danych; oficjalne UNO to zwykle `ttyACM0`, klon CH340 to `ttyUSB0`.
- W monitorze są krzaki — prędkość **115200**, nie 9600.
- Python milczy i kończy się błędem timeout — płytka nie wysłała `OK ready` (zły szkic, zły port) albo `WAIT` jedzie dłużej niż `--timeout`.
- Serwo brzęczy albo grzeje się w miejscu — kąt jest poza ruchem mechanizmu. Zawęź limit albo popraw home. Odłącz sygnał komendą `DETACH` i odetnij zasilacz serw.

## Czego nie sprawdzono na sprzęcie

Nie było podłączonego UNO, serw ani ramienia. Nie sprawdzono kierunku obrotu,
rzeczywistych ograniczników, prądu zasilacza ani roli pinu 7.

Kompilacja, która się udała: `arduino-cli` 1.5.1, rdzeń `arduino:avr` 1.8.8,
biblioteka `Servo` 1.3.0, FQBN `arduino:avr:uno`.

- Flash: 12754 bajty (39% z 32256)
- RAM: 747 bajtów (36% z 2048), zostaje 1301 bajtów na zmienne lokalne

To potwierdza tylko składnię i linkowanie, nie ruch mechanizmu.
