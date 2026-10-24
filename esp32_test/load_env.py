"""
Pre-build script PlatformIO: .env  ->  macro C++.

Sull'ESP32 non esiste un sistema operativo, quindi getenv() restituisce sempre
NULL: le variabili d'ambiente vanno "cotte" nel firmware al momento della
compilazione. Questo script legge .env e passa ogni riga CHIAVE=valore al
compilatore come  -D ENV_CHIAVE="valore".

Esempio:  URL_WEBSITE=https://x.up.railway.app/update-coordinates
diventa:  #define ENV_URL_WEBSITE "https://x.up.railway.app/update-coordinates"

Se cambi il .env, la build successiva ricompila da sola (SCons si accorge
che i flag del compilatore sono cambiati).
"""
from pathlib import Path

Import("env")  # noqa: F821  (iniettato da PlatformIO/SCons)

SECRET_HINTS = ("TOKEN", "PASSWORD", "SECRET", "KEY")


def parse_env_file(path: Path) -> dict:
    values = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()          # tollera "URL_WEBSITE =valore"
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]    # tollera valori tra virgolette
        values[key] = value
    return values


def masked(key: str, value: str) -> str:
    if any(h in key.upper() for h in SECRET_HINTS):
        return f"****({len(value)} caratteri)" if value else "(vuoto)"
    return value or "(vuoto)"


env_path = Path(env.subst("$PROJECT_DIR")) / ".env"  # noqa: F821

if not env_path.exists():
    print("[load_env] ATTENZIONE: .env non trovato -> URL e token andranno inseriti dal portale WiFi")
else:
    for key, value in parse_env_file(env_path).items():
        macro = f"ENV_{key}"
        env.Append(CPPDEFINES=[(macro, env.StringifyMacro(value))])  # noqa: F821
        print(f"[load_env] {macro} = {masked(key, value)}")
