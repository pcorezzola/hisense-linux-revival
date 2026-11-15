#!/bin/sh
# Arma ModernTV_Launcher.pkg (tar.gz con el directorio ModernTV_Launcher/install.sh adentro).
# El formato lo impone /3rd_rw/software-update/appselfupgrade.sh:
#   cd /3rd_rw/upgrade/ && tar -zxf ModernTV_Launcher.pkg && cd ModernTV_Launcher && sh ./install.sh
set -eu
cd "$(dirname "$0")"

rm -rf build ModernTV_Launcher.pkg
mkdir -p build/ModernTV_Launcher
cp install.sh build/ModernTV_Launcher/install.sh
chmod +x build/ModernTV_Launcher/install.sh

# -h para no duplicar el prefijo del directorio; el tar debe tener "ModernTV_Launcher/..." en la raiz
tar -czf ModernTV_Launcher.pkg -C build ModernTV_Launcher

echo "== contenido del paquete =="
tar -tzvf ModernTV_Launcher.pkg
ls -l ModernTV_Launcher.pkg
