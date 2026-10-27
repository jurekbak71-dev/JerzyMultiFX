#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
fail() { echo; echo "$*"; exit 1; }
if [[ ${1:-} == --help ]]; then
 echo 'Uruchom: bash instaluj-rpi.sh. Instalator zapyta o kartę dźwiękową.'; exit 0
fi
[[ $(uname -s) == Linux && $(uname -m) == aarch64 ]] || fail 'Potrzebny jest Raspberry Pi z systemem 64-bit. Nie uruchamiaj tego na komputerze Windows.'
[[ -r /etc/os-release ]] || fail 'Nie udało się rozpoznać systemu.'
. /etc/os-release
[[ ${VERSION_CODENAME:-} == trixie ]] || fail 'Potrzebny jest Raspberry Pi OS 64-bit Trixie. Ten instalator nie zmienia ani nie aktualizuje całego systemu.'
package=JerzyMultiFX-RPi-aarch64-v0.1.0-preview.1.tar.gz
[[ -f $package && -f PACZKA.sha256 ]] || fail 'Brakuje plików. Najpierw rozpakuj cały ZIP do jednego folderu.'
sha256sum -c PACZKA.sha256 || fail 'Plik programu jest uszkodzony. Pobierz ponownie cały ZIP.'
echo 'Jerzy MultiFX — instalacja na Raspberry Pi'
echo 'Potrzebne są internet i karta dźwiękowa z wejściem oraz wyjściem.'
echo 'Program zostanie dodany do systemu i będzie uruchamiany po włączeniu Raspberry Pi.'
echo 'Karta pamięci nie będzie formatowana. Istniejąca instalacja MultiFX otrzyma kopię zapasową.'
read -r -p 'Naciśnij Enter, aby kontynuować, lub Ctrl+C, aby przerwać. ' _
sudo -v
if pgrep -x jackd >/dev/null; then fail 'Inny program korzysta już z serwera dźwięku JACK. Zatrzymaj go i uruchom instalator ponownie.'; fi
if ! command -v arecord >/dev/null || ! command -v aplay >/dev/null; then
 sudo apt-get update
 sudo apt-get install -y --no-install-recommends alsa-utils
fi
capture=$(LC_ALL=C arecord -l 2>/dev/null) || fail 'Nie znaleziono wejścia audio. Podłącz kartę dźwiękową USB z wejściem i wyjściem, potem spróbuj ponownie.'
playback=$(LC_ALL=C aplay -l 2>/dev/null) || fail 'Nie znaleziono wyjścia audio.'
choices=(); labels=()
while IFS= read -r line; do
 if [[ $line =~ ^card\ ([0-9]+):\ ([A-Za-z0-9_]+)\ \[(.*)\],\ device\ ([0-9]+): ]]; then
  card=${BASH_REMATCH[1]}; id=${BASH_REMATCH[2]}; label=${BASH_REMATCH[3]}; dev=${BASH_REMATCH[4]}
  if grep -Eq "^card $card:.*device $dev:" <<< "$playback"; then
   choices+=("hw:$id,$dev"); labels+=("$label (wejście/wyjście $dev)")
  fi
 fi
done <<< "$capture"
((${#choices[@]})) || fail 'Nie znaleziono karty z wejściem i wyjściem. Podłącz zewnętrzną kartę audio USB i spróbuj ponownie.'
echo; echo 'Wybierz kartę, do której podłączysz instrument i słuchawki lub głośniki:'
for i in "${!choices[@]}"; do echo "$((i+1)). ${labels[i]}"; done
read -r -p 'Wpisz numer karty i naciśnij Enter: ' answer
[[ $answer =~ ^[1-9][0-9]*$ && ${#answer} -le 3 ]] || fail 'Nie wybrano poprawnego numeru. Uruchom instalator ponownie.'
((answer <= ${#choices[@]})) || fail 'Nie ma takiego numeru karty.'
device=${choices[answer-1]}
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
tar -xzf "$package" -C "$tmp"
installer=$(find "$tmp" -mindepth 3 -maxdepth 3 -path '*/scripts/install.sh' -type f)
[[ -n $installer && $installer != *$'\n'* ]] || fail 'Nieprawidłowa zawartość paczki.'
echo 'Instaluję. Może to potrwać kilka minut.'
sudo bash "$installer" --enable --device "$device"
for i in {1..20}; do
 if sudo test -s /var/lib/jerzy-multifx/token && systemctl is-active --quiet jerzy-multifx-web.service; then break; fi
 sleep 1
done
for service in jerzy-multifx-jack jerzy-multifx jerzy-multifx-web; do
 systemctl is-active --quiet "$service.service" || fail "Usługa $service nie uruchomiła się. Zrób zdjęcie komunikatu i poproś o pomoc; nie trzeba instalować systemu ponownie."
done
echo; echo 'GOTOWE. Na Raspberry Pi otwórz w przeglądarce:'
echo 'http://localhost:8097'
ip=$(hostname -I | awk '{print $1}')
[[ -z $ip ]] || echo "Na telefonie lub komputerze w tej samej sieci otwórz: http://$ip:8097"
echo 'Na stronie wklej poniższy klucz dostępu (zachowaj go dla siebie):'
sudo cat /var/lib/jerzy-multifx/token
echo; echo 'Podłącz instrument do wejścia wybranej karty, a słuchawki/głośniki do jej wyjścia.'
echo 'To wersja próbna: może występować słyszalne opóźnienie dźwięku.'
