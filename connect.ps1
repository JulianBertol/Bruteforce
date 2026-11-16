$server = "10.1.0.190"
$user = "admin-sbs"
$password = "DEIN_PASSWORT"

# Startet SSH in einem neuen Konsolenfenster
Start-Process powershell -ArgumentList "-noexit", "-command", "ssh $user@$server"

# 1,5 Sekunden warten, bis das Passwort-Feld im SSH-Fenster offen ist
Start-Sleep -Milliseconds 1500

# Passwort automatisch eintippen und Enter senden
Add-Type -AssemblyName System.Windows.Forms
[System.Windows.Forms.SendKeys]::SendWait("$password{ENTER}")
